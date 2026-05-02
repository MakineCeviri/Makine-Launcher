# SWR (Stale-While-Revalidate) — Manifest Cache

> **Status:** Implemented in commit `<TBD>`
> **Owner:** ui-dev
> **Tracks:** A — Manifest cache refresh UX

## Problem

Mevcut `ManifestSyncService` startup akışı:

```
constructor → loadCachedIndex (disk → memory)        ← UI hemen görünür
syncCatalog() → fetchCatalogMeta (CDN)                ← arka planda
              → fetchCatalogDelta / FullCatalog       ← cache'i güncelle
              → catalogReady emit                     ← UI tekrar bağlanır
```

İki sorun:
1. **Sessiz refresh** — kullanıcı arka plan refresh'in olduğunu bilmez. Önceki kataloga göre yeni yamalar geldiyse haber alamaz.
2. **Stale cache farkındalığı yok** — bayatlamış cache ile yeni cache arasında UI fark göstermez. "Yeni içerik geldi, listeyi yenileyin" sinyali yok.

## Çözüm

**Stale-While-Revalidate (SWR):** cache zaten gösteriliyor (mevcut davranış), arka planda yeni veri çekiliyor (mevcut davranış), **YENI:** değişiklik varsa UI'a "yeni içerik mevcut" rozet göster.

### Akış

```
constructor → loadCachedIndex                          ← cache hemen göster
syncCatalog → fetch → parseIndex                       ← changed entries tracked
                    → invalidateAndNotifyRefresh:
                       - beforeRefresh = changedCount  ← PEEK
                       - invalidateChangedDetails       ← TAKE + detail fetcher invalidate
                       - if changedCount > 0:
                         emit catalogRefreshed(total, changed) ← YENİ SIGNAL
                    → catalogReady (mevcut signal)
```

### UX Kararı

- **Sessiz refresh** — startup ve periyodik retry'lerde sessiz, ekrana popup atmaz
- **Corner badge** — değişiklik varsa sağ alt köşede 3 sn'lik rozet:
  - "✨ Yeni içerik mevcut: %1 yama"
- **Auto-fade** — 3 sn sonra opacity 1 → 0, click yok (passif bilgilendirme)
- **Modal yok** — kullanıcının iş akışını bölmez

### Edge Case'ler

| Durum | Davranış |
|-------|----------|
| İlk launch (cache yok) | Sessiz, badge yok (hepsi "yeni" değil, hepsi *ilk*) |
| Cache'le aynı CDN cevabı (304) | changedCount = 0, badge yok |
| Sadece silinen entry var | Şu an sadece eklenenler/değişenler tracking → silme audit kapsamı dışı |
| Sync timeout (B2-10 fix sonrası) | Reply abort olur, badge tetiklenmez |
| Concurrent syncCatalog (deduplicated) | Tek refresh, tek badge |

## Bileşenler

### Backend
- **`CatalogStore::changedCount() const`** — peek-only, m_changedAppIds.size()
- **`ManifestSyncService::catalogRefreshed(int totalCount, int changedCount)`** — yeni Q_SIGNAL
- **`ManifestSyncService::invalidateAndNotifyRefresh()`** — helper: peek count → invalidateChangedDetails → emit catalogRefreshed (count > 0 ise)
- 3 reply path'inde (handleDeltaResponse, handleFullCatalogResponse, fallbackToLegacySync) invalidateChangedDetails yerine invalidateAndNotifyRefresh çağırılır

### Frontend
- **`qml/qml/components/RefreshIndicator.qml`** — yeni component (~50 LoC)
  - Anchored bottomRight + margin
  - Background: Theme.bgSecondary, 1px Theme.borderSubtle, radius 8
  - Text: "✨ Yeni içerik mevcut: %1 yama" (i18n için tr())
  - SequentialAnimation: 0→1 fade (200ms) → 2600ms hold → 1→0 fade (200ms) — toplam 3s
  - ManifestSync.catalogRefreshed → property update + animation start
- **`Main.qml`** — RefreshIndicator instance'ı yerleştir (Overlay parent ya da root ApplicationWindow)

## Test Senaryoları

| Senaryo | Beklenen |
|---------|----------|
| Cold launch + cache var + CDN aynı | Badge YOK |
| Cold launch + cache var + 1 yama versionu yükseltilmiş | Badge "1 yama" |
| Cold launch + cache yok + CDN dolu | Badge YOK (ilk) |
| Çevrimdışı + retry sonrası bağlanma + 5 değişiklik | Badge "5 yama" |
| Hızlı 2 manuel refresh (debounce yok) | Tek badge (concurrent dedup mevcut) |

## Out of Scope (Bilinçli Olarak)

- Tıklanabilir badge (modal ile detay) — passif kalır, scroll-up ile katalog zaten yeni entry'leri gösterir
- "Şimdi yeniden başlat" CTA — refresh otomatik, restart gerekmez
- Silinen yamaları tracking — audit dışı, beta sonrası

## İlişkili
- B2-10 (`9a6dff7`) — syncCatalog race fix; SWR'ın doğru çalışması için reply tracking şart
- `cdn::kCatalogMeta`, `cdn::kCatalogDelta` — mevcut endpoint'ler değişmedi
