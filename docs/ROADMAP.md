# MakineAI Yol Haritası

**Son Güncelleme:** 2026-02-06

---

## Genel Durum

```
██████████████████░░░░░░░░░░  62%  GENEL TAMAMLANMA
```

| Bölüm | Tamamlanma | Ağırlık | Durum |
|-------|-----------|---------|-------|
| C++ Core Library | %72 | %35 | Altyapı sağlam, parser/download eksik |
| QML Arayüz | %75 | %25 | Görsel tamam, core entegrasyon yok |
| Core-UI Entegrasyon | %10 | %20 | **KRİTİK DARBOĞAZ** |
| Test & QA | %55 | %10 | 120 test var, bazı modüller eksik |
| CI/CD & DevOps | %70 | %5 | Pipeline var, iyileştirme devam |
| Dokümantasyon | %85 | %5 | 32 dosya, kapsamlı |

---

## Faz 1: UI Düzeltmeleri ✅ (%90 Tamamlandı)

- [x] Flutter'dan Qt6/QML'e geçiş
- [x] 80 QML dosyası, 59 bileşen
- [x] Tema sistemi (dark/light)
- [x] Animasyonlar ve GPU optimizasyonu
- [x] Flutter referans temizliği
- [ ] Küçük UI bug'lar (devam ediyor)

---

## Faz 2: Core Entegrasyonu 🔄 (Aktif - %10)

> **Epic Issue:** [#12](https://github.com/jlceaser/MakineAI/issues/12)

Bu projenin en kritik aşaması. C++ Core ile QML UI'ı birleştirmek.

| # | Görev | Issue | Durum | Öncelik |
|---|-------|-------|-------|---------|
| 1 | Build sistemini birleştir | [#13](https://github.com/jlceaser/MakineAI/issues/13) | ❌ Başlamadı | Kritik |
| 2 | CoreBridge gerçek implementasyon | [#14](https://github.com/jlceaser/MakineAI/issues/14) | ❌ Başlamadı | Kritik |
| 3 | HTTP client entegrasyonu | [#15](https://github.com/jlceaser/MakineAI/issues/15) | ❌ Başlamadı | Yüksek |
| 4 | Asset parser gerçek extraction | [#16](https://github.com/jlceaser/MakineAI/issues/16) | ❌ Başlamadı | Yüksek |
| 5 | Oyun tarama entegrasyonu | [#17](https://github.com/jlceaser/MakineAI/issues/17) | ❌ Başlamadı | Yüksek |

**Bağımlılık Sırası:** #13 → #14 → #17 → #15 → #16

---

## Faz 3: Paket Sistemi (%30)

> **Epic Issue:** [#19](https://github.com/jlceaser/MakineAI/issues/19)

| Görev | Durum |
|-------|-------|
| Çeviri paketi formatı tanımla | ❌ Başlamadı |
| Paket indirme mekanizması | ❌ Başlamadı (Faz 2'ye bağlı) |
| Paket yükleme | Kısmen yazıldı |
| Uyumluluk kontrolü | Kısmen yazıldı |
| Geri alma (rollback) | Patch Engine ile mevcut |

---

## Faz 4: Test ve Polish (%20)

| Görev | Issue | Durum |
|-------|-------|-------|
| Eksik modül testleri | [#18](https://github.com/jlceaser/MakineAI/issues/18) | ❌ Başlamadı |
| Gerçek oyunlarla e2e test | - | ❌ Başlamadı |
| Performans optimizasyonu | - | ❌ Başlamadı |
| Windows installer | [#20](https://github.com/jlceaser/MakineAI/issues/20) | ❌ Başlamadı |

---

## Çalışma Modu

### Mod 1: Desteklenen Oyunlar
```
Kullanıcı oyunu seçer → Uyumluluk kontrolü → Çeviri paketi indir → Dil dosyalarını yükle → Tamamlandı!
```

### Mod 2: Topluluk Çevirileri
```
Kullanıcı oyunu seçer → Topluluk paketi ara → Patch uygula → Tamamlandı!
```

---

## İleri Vizyon (v1.x+)

### Gaming Companion AI

Oyun oynarken yanında bir arkadaş gibi AI asistan.

**Konsept:** F12 ile ekran görüntüsü → AI analiz → Sahneye uygun yorum → Overlay

**Kişilik Sistemi:**
- Sadece "araştırmış" değil, "yaşamış" gibi konuşsun
- Soğuk bilgi yerine samimi deneyim paylaşımı
- Duruma göre ton değişimi (komik/duygusal/heyecanlı)

**Teknik Gereksinimler:**
- [ ] Screenshot capture (F12) - MEVCUT
- [ ] Claude API entegrasyonu
- [ ] Oyun tanımlama sistemi
- [ ] Overlay mesaj UI
- [ ] Kişilik/ton motoru

**Hedef:** v1.0 veya sonrası

---

## Risk Matrisi

| Risk | Etki | Olasılık | Öncelik |
|------|------|----------|---------|
| Core-UI entegrasyonu gecikmesi | Yüksek | Yüksek | **KRİTİK** |
| Asset parser'ların stub kalması | Yüksek | Orta | Yüksek |
| HTTP/download sisteminin olmaması | Yüksek | Yüksek | Yüksek |
| CI pipeline kırılganlığı | Orta | Orta | Orta |
| Test coverage boşlukları | Orta | Düşük | Düşük |

---

## Araçlar

### Geliştirme
- Qt 6.10.1 + MinGW 13.1.0
- Visual Studio 2022 (Core için MSVC)
- CMake 3.25+ + Ninja
- vcpkg (18 bağımlılık)

### DevOps
- GitHub Actions (CI/CD)
- CodeQL (güvenlik analizi)
- clang-format + clang-tidy (kod kalitesi)
- Dependabot (bağımlılık güncellemeleri)

### Yardımcı
- GitHub Copilot Pro
- Claude Code

---

*Bu doküman aktif olarak güncellenmektedir.*
*CEDRA Interactive - 2026*
