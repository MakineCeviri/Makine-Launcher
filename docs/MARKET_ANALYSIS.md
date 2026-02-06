# MakineAI Piyasa Analizi

**Tarih:** 2026-01-20
**Versiyon:** 1.0

---

## 1. Mevcut Piyasa Durumu

### 1.1 Uluslararası Oyun Çeviri Araçları

| Araç | Tür | Desteklenen Motorlar | Fiyat | Güçlü Yanlar | Zayıf Yanlar |
|------|-----|---------------------|-------|--------------|--------------|
| **XUnity.AutoTranslator** | Runtime Hook | Unity (Mono/IL2CPP) | Ücretsiz | Gerçek zamanlı çeviri, DeepL entegrasyonu | Sadece Unity, kurulum karmaşık |
| **Translator++** | CAT Tool | RPG Maker, Wolf RPG, Kirikiri, LiveMaker, Yu-ris | Ücretsiz/Patreon | Çok motorlu destek, MTL + manuel hibrit | Visual novel odaklı, AAA oyun desteği yok |
| **MTool** | One-click | RPG Maker, Wolf RPG, Kirikiri, Ren'Py, TyranoBuilder | Ücretsiz/Patreon | Drag-drop basitlik, hile özellikleri | Japonca odaklı, Unreal/Unity AAA yok |
| **Translumo** | OCR Screen | Tüm oyunlar | Ücretsiz | Evrensel, hardcoded subtitle | Performans etkisi, doğruluk sorunu |
| **BaconanaMTLTool** | Batch MTL | Unity, Wolf RPG, Kirikiri, NScripter | Ücretsiz | AI-powered, batch işlem | Visual novel odaklı |
| **AiNiee** | AI Translation | RenPy, Mtool uyumlu | Ücretsiz | LLM entegrasyonu, glossary | Batch-only, runtime yok |
| **UEExtractor** | Export Tool | Unreal Engine 4.0-5.6 | Ücretsiz | .pak/.utoc desteği | Sadece export, otomatik çeviri yok |

### 1.2 Türk Oyun Çeviri Toplulukları

| Topluluk | URL | Özellikler |
|----------|-----|-----------|
| **Turkce-yama.com** | turkce-yama.com | Yama indirme portalı, kurulum rehberleri |
| **Oyunceviri.net** | oyunceviri.net | Aktif çeviri topluluğu |
| **Anonymous Çeviri** | - | Discord tabanlı, gönüllü çevirmenler |
| **Technopat Sosyal** | technopat.net/sosyal | Teknik tartışmalar, yama yapım rehberleri |
| **DonanımHaber Forum** | forum.donanimhaber.com | Genel oyun çeviri tartışmaları |

### 1.3 Ticari Yerelleştirme Çözümleri

| Çözüm | Hedef Kitle | Fiyat |
|-------|-------------|-------|
| **Alocai** | Oyun stüdyoları | Enterprise |
| **Gridly** | Oyun stüdyoları | Enterprise |
| **Crowdin** | Açık kaynak projeler, stüdyolar | Freemium |
| **OneSky** | Mobil/oyun geliştiriciler | Enterprise |
| **AI Localization Automator** (UE) | Unreal geliştiriciler | Marketplace |

---

## 2. Piyasa Boşlukları ve Fırsatlar

### 2.1 Tespit Edilen Boşluklar

1. **Türkçe Odaklı Entegre Çözüm Eksikliği**
   - Mevcut araçlar Japonca→İngilizce odaklı
   - Türkçe karakter (ş, ğ, ü, ö, ç, ı, İ) desteği sorunlu
   - Türk oyuncular için özelleştirilmiş UX yok

2. **AAA Oyun Motor Desteği Zayıf**
   - Unreal Engine AAA oyunları için otomatik çözüm yok
   - Unity IL2CPP oyunları için kolay kurulum yok
   - Bethesda (BA2) desteği sınırlı

3. **Tek Tıklama Deneyimi Eksik**
   - XUnity.AutoTranslator: BepInEx kurulumu gerekli
   - Translator++: Manuel export/import gerekli
   - MTool: Japonca oyunlara özel

4. **Topluluk Çevirileri Dağınık**
   - Her site kendi formatını kullanıyor
   - Merkezi çeviri deposu yok
   - Versiyon takibi yok

5. **Oyun Güncellemesi Sorunu**
   - Oyun güncellenince yama bozuluyor
   - Otomatik güncelleme algılama yok
   - Yedekleme/geri yükleme manuel

### 2.2 MakineAI'ın Hedef Pozisyonu

```
┌─────────────────────────────────────────────────────────────────┐
│                    PIYASA POZISYONLAMA                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Karmaşıklık                                                   │
│       ▲                                                        │
│       │                                                        │
│       │    Translator++    AI Localization                     │
│  Yüksek│         ●              Automator                      │
│       │                            ●                           │
│       │                                                        │
│       │      XUnity.Auto        Crowdin                        │
│   Orta│         Translator ●      ●                            │
│       │                                                        │
│       │                    ★ MakineAI                          │
│       │                    (HEDEF)                             │
│  Düşük│    MTool ●                                             │
│       │                                                        │
│       └────────────────────────────────────────────────►       │
│           Indie/VN        AAA/Modern        Motor Desteği     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Rakip Analizi: Öne Çıkan Özellikler

### 3.1 XUnity.AutoTranslator
**Güçlü:**
- Gerçek zamanlı hooking (oyun çalışırken çeviri)
- DeepL, Google Translate, Sugoi entegrasyonu
- BepInEx/IPA/ReiPatcher desteği
- Çeviri cache'leme

**Zayıf:**
- Sadece Unity motorlu oyunlar
- Teknik bilgi gerektiren kurulum
- UI/UX zayıf (config dosyası düzenleme)

### 3.2 Translator++
**Güçlü:**
- CAT tool yaklaşımı (kaliteli çeviri)
- 20+ MTL servisi entegrasyonu
- Context marking (script/fonksiyon koruma)
- Versiyon kontrolü (grid view)

**Zayıf:**
- Batch-only (gerçek zamanlı yok)
- Visual novel/indie odaklı
- Export/import manuel işlem

### 3.3 MTool
**Güçlü:**
- Drag-drop basitlik
- Hile özellikleri (hız artırma)
- Japonca oyunlar için optimize

**Zayıf:**
- Japonca→İngilizce odaklı
- AAA oyun desteği yok
- Patreon'da premium özellikler

---

## 4. MakineAI Rekabet Avantajları

### 4.1 Teknik Farklılaşma

| Özellik | Rakipler | MakineAI |
|---------|----------|----------|
| Türkçe karakter desteği | Kısmi | Tam entegre |
| Unity IL2CPP | XUnity (karmaşık) | Tek tıklama |
| Unreal Engine AAA | UEExtractor (export only) | Tam pipeline |
| Bethesda BA2 | Yok | Destekleniyor |
| GameMaker | MTool | Destekleniyor |
| Otomatik oyun tespiti | Yok | Steam/Epic/GOG tarama |
| Versiyon takibi | Yok | Otomatik güncelleme algılama |
| Yedekleme/geri yükleme | Manuel | Otomatik |

### 4.2 Kullanıcı Deneyimi Farklılaşması

1. **Türkçe UI/UX**
   - Tamamen Türkçe arayüz
   - Türk oyunculara özel tasarım
   - Yerel topluluk entegrasyonu

2. **Tek Tıklama Kurulum**
   - Oyunu seç → "Türkçe Yama" butonuna tıkla
   - Otomatik motor algılama
   - Otomatik runtime kurulumu (BepInEx/XUnity branded)

3. **Merkezi Çeviri Deposu**
   - Topluluk çevirilerini tek noktada topla
   - Versiyon kontrolü
   - Otomatik güncelleme

### 4.3 İş Modeli Farklılaşması

| Model | Rakipler | MakineAI |
|-------|----------|----------|
| Fiyatlama | Freemium/Patreon | Ücretsiz + Premium topluluk |
| Topluluk | Dağınık forumlar | Entegre platform |
| Çevirmen teşviki | Yok | Badge/ranking sistemi |
| Kalite kontrolü | Kullanıcıya bırakılmış | Topluluk onayı sistemi |

---

## 5. Hedef Kullanıcı Segmentleri

### 5.1 Birincil Segment: Türk Oyuncular

**Profil:**
- 18-35 yaş arası
- İngilizce bilgisi orta/zayıf
- AAA ve indie oyunlar oynuyor
- Steam/Epic Games kullanıcısı

**İhtiyaçlar:**
- Kolay kurulum
- Güvenilir çeviriler
- Oyun güncellemelerinde sorun yaşamamak

**Pazar Büyüklüğü:**
- Türkiye'de ~30 milyon aktif oyuncu
- Bunların %60'ı Türkçe tercih ediyor
- Hedef: 500K+ aktif kullanıcı

### 5.2 İkincil Segment: Türk Çeviri Toplulukları

**Profil:**
- Gönüllü çevirmenler
- Forum/Discord toplulukları
- Teknik bilgisi olan oyuncular

**İhtiyaçlar:**
- Kolay çeviri araçları
- Dağıtım platformu
- Tanınma/teşvik

### 5.3 Üçüncül Segment: Oyun Yayıncıları

**Profil:**
- Türkiye'de yayın yapan firmalar
- Indie geliştiriciler

**İhtiyaçlar:**
- Yerelleştirme desteği
- Topluluk çevirileri kullanma

---

## 6. Özgün Çeviri Sistemleri ve Yöntemler

### 6.1 Oyun Motoru Bazlı Yaklaşım

```
┌─────────────────────────────────────────────────────────────────┐
│                  MOTOR ALGILAMA AKIŞI                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Oyun EXE/Klasör                                               │
│        │                                                        │
│        ▼                                                        │
│  ┌─────────────┐                                               │
│  │ İmza Analizi│ ── UnityPlayer.dll → Unity                    │
│  │             │ ── UE4Game*.exe → Unreal                      │
│  │             │ ── data.win → GameMaker                       │
│  │             │ ── .ba2 files → Bethesda                      │
│  │             │ ── renpy/*.rpyc → Ren'Py                      │
│  └─────────────┘                                               │
│        │                                                        │
│        ▼                                                        │
│  ┌─────────────┐                                               │
│  │ Alt-motor   │ ── Unity: Mono vs IL2CPP                      │
│  │ Tespiti     │ ── Unreal: 4.x vs 5.x                         │
│  └─────────────┘                                               │
│        │                                                        │
│        ▼                                                        │
│  ┌─────────────┐                                               │
│  │ Strateji    │ ── Runtime: BepInEx + XUnity                  │
│  │ Seçimi      │ ── File-based: .pak/.locres düzenleme         │
│  │             │ ── Binary: Son çare                           │
│  └─────────────┘                                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 Çeviri Stratejileri

| Motor | Birincil Strateji | Yedek Strateji |
|-------|------------------|----------------|
| Unity Mono | Runtime (XUnity) | Asset düzenleme |
| Unity IL2CPP | Runtime (XUnity) | - |
| Unreal Engine | .pak/.locres düzenleme | - |
| Bethesda | .ba2 string düzenleme | - |
| GameMaker | data.win düzenleme | - |
| RPG Maker | JSON/rvdata düzenleme | - |
| Ren'Py | .rpy script düzenleme | - |

### 6.3 Çeviri Kalite Sistemi

```
┌─────────────────────────────────────────────────────────────────┐
│                  ÇEVİRİ KALİTE PİRAMİDİ                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│                      ┌───────────┐                             │
│                      │ Doğrulanmış│ ← Topluluk onayı           │
│                      │  Çeviriler │   (20+ olumlu oy)          │
│                      └─────┬─────┘                             │
│                            │                                    │
│                    ┌───────┴───────┐                           │
│                    │   İncelenmiş  │ ← Moderatör kontrolü       │
│                    │   Çeviriler   │                            │
│                    └───────┬───────┘                           │
│                            │                                    │
│              ┌─────────────┴─────────────┐                     │
│              │     Topluluk Çevirileri   │ ← Gönüllü katkı     │
│              └─────────────┬─────────────┘                     │
│                            │                                    │
│        ┌───────────────────┴───────────────────┐               │
│        │           MTL Çevirileri              │ ← AI-powered   │
│        │      (DeepL/Google/ChatGPT)          │   (düşük kalite)│
│        └───────────────────────────────────────┘               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Teknik Implementasyon Öncelikleri

### 7.1 Faz 1: Temel Altyapı (0.1.0-alpha)

1. **Oyun Algılama**
   - Steam/Epic/GOG oyun listesi tarama
   - Motor imza tespiti
   - Oyun versiyon takibi

2. **Unity Desteği**
   - BepInEx otomatik kurulum
   - XUnity.AutoTranslator entegrasyonu
   - Branded "MakineAI Translation System"

3. **Basit Dosya Yamaları**
   - JSON (RPG Maker MV/MZ)
   - Metin dosyaları
   - Bethesda .strings

### 7.2 Faz 2: Genişletilmiş Motor Desteği (0.2.0)

1. **Unreal Engine**
   - .pak extract/repack
   - .locres düzenleme
   - Cooked asset işleme

2. **GameMaker**
   - data.win parsing
   - String tablası düzenleme

3. **Çeviri Deposu**
   - Merkezi sunucu
   - Paket imzalama
   - Otomatik güncelleme

### 7.3 Faz 3: Topluluk Platformu (1.0.0)

1. **Çevirmen Araçları**
   - Web tabanlı editör
   - Glossary yönetimi
   - Çeviri hafızası

2. **Kalite Sistemi**
   - Topluluk oylaması
   - Moderatör paneli
   - Otomatik kalite kontrolü

---

## 8. Risk Analizi

### 8.1 Teknik Riskler

| Risk | Olasılık | Etki | Azaltma |
|------|----------|------|---------|
| Anti-cheat algılama | Orta | Yüksek | Sadece singleplayer, whitelist |
| Oyun güncellemeleri | Yüksek | Orta | Versiyon takibi, hızlı güncelleme |
| Motor değişiklikleri | Düşük | Yüksek | Modüler mimari |
| Performans sorunları | Düşük | Orta | C++ core optimizasyonu |

### 8.2 Yasal Riskler

| Risk | Olasılık | Etki | Azaltma |
|------|----------|------|---------|
| DMCA talepleri | Düşük | Orta | Sadece çeviri, asset kopyalamama |
| Lisans ihlali | Düşük | Yüksek | Açık kaynak bileşen denetimi |
| Yayıncı şikayetleri | Düşük | Orta | Yayıncı iletişimi, opt-out |

### 8.3 Piyasa Riskleri

| Risk | Olasılık | Etki | Azaltma |
|------|----------|------|---------|
| Resmi Türkçe desteği artışı | Orta | Yüksek | Değer önerisi genişletme |
| Rakip ürün | Düşük | Orta | İlk hareket avantajı |
| Topluluk benimsememesi | Orta | Yüksek | Erken topluluk katılımı |

---

## 9. Sonuç ve Öneriler

### 9.1 MakineAI'ın Benzersiz Değer Önerisi

**"Türk oyuncular için tek tıklamayla oyun çevirisi"**

1. **Basitlik**: Teknik bilgi gerektirmez
2. **Türkçe Odaklı**: Türkçe karakter ve UX
3. **Kapsamlı**: Birden fazla motor desteği
4. **Topluluk Gücü**: Merkezi çeviri deposu
5. **Güvenilir**: Yedekleme ve versiyon takibi

### 9.2 Kritik Başarı Faktörleri

1. **Unity runtime kurulumunu tek tıklamaya indirme**
2. **Popüler oyunlar için hazır çeviri paketleri**
3. **Aktif topluluk çevirmen ağı oluşturma**
4. **Hızlı oyun güncelleme takibi**

### 9.3 Öncelikli Hedef Oyunlar

| Oyun | Motor | Popülerlik | Zorluk |
|------|-------|------------|--------|
| Elden Ring | Unity IL2CPP | Çok Yüksek | Orta |
| Starfield | Bethesda | Yüksek | Düşük |
| Black Myth: Wukong | Unreal | Yüksek | Orta |
| Baldur's Gate 3 | Custom | Çok Yüksek | Yüksek |
| Hades II | Custom | Yüksek | Orta |
| Undertale/Deltarune | GameMaker | Orta | Düşük |

---

## Kaynaklar

- [XUnity.AutoTranslator - GitHub](https://github.com/bbepis/XUnity.AutoTranslator)
- [Translator++ - Dreamsavior](https://dreamsavior.net/translator-plusplus/)
- [MTool - Official Site](https://mtool.app/?lang=en)
- [Translumo - GitHub](https://github.com/ramjke/Translumo)
- [BaconanaMTLTool - GitHub](https://github.com/Baconana-chan/BaconanaMTLTool)
- [UEExtractor - GitHub](https://github.com/SolicenTEAM/UEExtractor)
- [Gridly - AI Translation Guide](https://www.gridly.com/blog/ai-translation-game-localization/)
- [Alocai](https://www.alocai.com/)
- [Technopat Sosyal - Türkçe Yama](https://www.technopat.net/sosyal/konu/oyunlara-tuerkce-yama-nasil-yapilir.3273999/)
- [Turkce-yama.com](https://turkce-yama.com/turkce-yama-nasil-kurulur)

---

*Bu belge MakineAI projesinin stratejik planlaması için hazırlanmıştır.*
