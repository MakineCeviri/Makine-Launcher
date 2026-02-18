# MakineAI Yol Haritası

**Son Güncelleme:** 2026-02-18

---

## Genel Durum

```
█████████████████░░░░░░░░░░░  60%  GENEL TAMAMLANMA
```

| Bölüm | Tamamlanma | Durum |
|-------|-----------|-------|
| Makine: Oyun Tespit & Tarama | %90 | Steam/Epic/GOG gerçek tarama çalışıyor |
| Makine: Çeviri Paket Kurulumu | %70 | Yerel paketler kurulabiliyor |
| Makine: Sunucu & Dağıtım | %5 | Henüz başlanmadı |
| MakineAI: Güncelleme Tespiti | %5 | İskelet sınıf yapısı mevcut |
| MakineAI: Adaptasyon Motoru | %0 | Tasarım aşaması |
| UI & Kullanıcı Deneyimi | %80 | Alpha kalitesinde, çalışır durumda |
| CI/CD & DevOps | %75 | Pipeline çalışıyor |

---

## Makine — Çeviri Dağıtım Platformu

### Faz 1: Temel İşlevsellik ✅ (%85 Tamamlandı)

- [x] Steam kütüphanesi tarama (Registry + VDF + ACF)
- [x] Epic Games tarama (Manifest JSON)
- [x] GOG tarama (Registry)
- [x] Oyun motoru tespiti (dosya imzaları)
- [x] Anti-cheat tespiti (EAC, BattlEye, Vanguard)
- [x] Çeviri paketi kurma/kaldırma (dosya kopyalama)
- [x] Yedekleme ve geri yükleme
- [x] Performans: lazy loading, visibility-aware timers
- [ ] Hata yönetimi iyileştirmeleri (devam ediyor)

### Faz 2: Dağıtım Sistemi (%5)

| Görev | Durum | Öncelik |
|-------|-------|---------|
| Çeviri paketi sunucu formatı | Başlanmadı | Kritik |
| Paket indirme mekanizması | Başlanmadı | Kritik |
| Versiyon kontrolü ve güncelleme | Başlanmadı | Yüksek |
| Paket imzalama/doğrulama | Başlanmadı | Yüksek |
| CDN veya GitHub Releases entegrasyonu | Başlanmadı | Orta |

### Faz 3: Topluluk (%0)

| Görev | Durum | Öncelik |
|-------|-------|---------|
| Çevirmen katkı sistemi | Başlanmadı | Orta |
| Çeviri kalite puanlama | Başlanmadı | Düşük |
| Oyun talep sistemi | Başlanmadı | Düşük |

---

## MakineAI — Adaptasyon Motoru

### Faz A: Güncelleme Tespiti (%0)

> **Gerçek Sorun:** Oyun güncellendi → Türkçe yama bozuldu

| Görev | Durum | Açıklama |
|-------|-------|----------|
| Dosya hash kaydı | Başlanmadı | Yama kurulurken her dosyanın hash'ini kaydet |
| Değişiklik tespiti | Başlanmadı | Uygulama açıldığında dosya hash'lerini karşılaştır |
| Steam versiyon kontrolü | Başlanmadı | Steam API'den oyun versiyonunu al |
| Kullanıcı bildirimi | Başlanmadı | "Oyun güncellendi, yama kontrol ediliyor" |

### Faz B: Analiz (%0)

| Görev | Durum | Açıklama |
|-------|-------|----------|
| Dosya diff sistemi | Başlanmadı | Eski vs yeni dosya karşılaştırması |
| String değişiklik haritası | Başlanmadı | Hangi stringler eklendi/silindi/taşındı |
| Yapı değişikliği tespiti | Başlanmadı | Dosya formatı/yapısı değişti mi |

### Faz C: Otomatik Uyarlama (%0)

| Görev | Durum | Açıklama |
|-------|-------|----------|
| Değişmeyen dosyaları koru | Başlanmadı | Hash eşleşen dosyalara dokunma |
| Fuzzy string matching | Başlanmadı | Taşınan stringleri bul ve yeniden eşle |
| Yeni string işaretleme | Başlanmadı | Çeviri gerektiren yeni stringleri belirle |
| Akıllı merge | Başlanmadı | Çatışan dosyaları birleştir |
| Adaptasyon doğrulama | Başlanmadı | Uyarlanan yamanın bütünlüğünü kontrol et |

---

## Kapalı / Ertelenmiş Özellikler

Aşağıdaki özellikler mevcut yönle uyumsuz veya önceliği düşük:

| Özellik | Durum | Neden |
|---------|-------|-------|
| Font analizi | **Kapatıldı** | Pratik değeri düşük, motor çeşitliliği çok fazla |
| Engine Handler'lar | **Kaldırıldı** | Koddan silindi (2026-02-12, ~6800 satır). ADR-0006 |
| Translation Pipeline | **Kaldırıldı** | Koddan silindi. ADR-0003 geçersiz |
| String Classifier | **Kaldırıldı** | Koddan silindi. Motor bazlı araçlara devredildi |
| Translation Memory | **Ertelendi** | Adaptasyon motoruna entegre edilecek (Faz C) |
| QA servisi | **Ertelendi** | Topluluk aşamasında değerlendirilecek |
| Gaming Companion AI | **v2.0+** | Öncelik değil, temel sorunlar çözmeli |

---

## Öncelik Sırası

```
1. Makine Faz 2: Dağıtım sistemi (sunucu + indirme)
   ↓
2. MakineAI Faz A: Güncelleme tespiti
   ↓
3. MakineAI Faz B: Analiz
   ↓
4. MakineAI Faz C: Otomatik uyarlama
   ↓
5. Makine Faz 3: Topluluk özellikleri
```

---

## Risk Matrisi

| Risk | Etki | Olasılık | Önlem |
|------|------|----------|-------|
| Oyun güncelleme formatları çok çeşitli | Yüksek | Yüksek | Motor bazlı adaptasyon stratejileri |
| Sunucu maliyeti | Orta | Orta | GitHub Releases / CDN başlangıcı |
| Çevirmen topluluğu yetersiz | Yüksek | Orta | Önce mevcut çevirilerle başlangıç |
| Binary dosya formatları değişken | Yüksek | Yüksek | Metin tabanlı dosyalara öncelik ver |

---

## Araçlar

### Geliştirme
- Qt 6.10.1 + MinGW 13.1.0
- Visual Studio 2022 (Core için MSVC)
- CMake 3.28+ + Ninja
- vcpkg (19 bağımlılık)

### DevOps
- GitHub Actions (CI/CD)
- CodeQL (güvenlik analizi)
- clang-format + clang-tidy (kod kalitesi)

---

---

## Son Değişiklikler

### 2026-02-18: Kod kalitesi & performans
- Güvenlik denetimi (14 bulgu düzeltildi)
- Modülerlik iyileştirmeleri (GameService decoupling, detail/ header yapısı)
- Kod minimizasyonu (gereksiz yorumlar, kullanılmayan import'lar)
- Qt Quick performans optimizasyonları (Image sourceSize, redundant clip)
- Dokümantasyon güncellemesi (build preset açıklamaları, Core entegrasyon durumu)

### 2026-02-14: Dead code temizliği
- GameListModel, dead Q_INVOKABLE/Q_PROPERTY/signal'lar kaldırıldı
- Dimensions.qml 76 dead property, DebugHelper.qml 7 dead fonksiyon kaldırıldı

### 2026-02-12: Büyük temizlik
- ~32K satır dead code kaldırıldı (handler'lar, TM, Glossary, QA, Pipeline)
- 13 ölü QML bileşeni silindi
- Veri hataları düzeltildi (D2R fallback mapping, Vulkan probe UB)
- Build system temizlendi (gereksiz lib, Qt modülleri)
- ADR-0006 oluşturuldu (adaptasyon motoru yön değişikliği)

---

*Bu doküman aktif olarak güncellenmektedir.*
*MakineAI — 2026*
