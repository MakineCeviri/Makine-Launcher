# MakineAI Yol Haritasi

**Son Guncelleme:** 2026-02-03

---

## Mevcut Durum

- **UI:** Qt QML arayuz calisiyor (UI-only mod)
- **Core:** C++ kutuphanesi mevcut, entegrasyon bekliyor
- **Build:** Ayri ayri derleniyor, birlestirilmedi

---

## Oncelik Sirasi

### Faz 1: UI Duzeltmeleri (AKTIF)

UI'da bilinen/tespit edilecek hatalar:

| # | Sorun | Dosya | Durum |
|---|-------|-------|-------|
| 1 | | | |
| 2 | | | |
| 3 | | | |

**Notlar:**
- UI hatalari tespit edilecek
- Tek tek duzeltilecek
- Her duzeltme sonrasi test

### Faz 2: Core Entegrasyonu

UI tamamlandiktan sonra:

1. [ ] `MAKINEAI_UI_ONLY` flag'ini kaldir
2. [ ] Core library'yi QML'e bagla
3. [ ] Gercek oyun taramasi test et
4. [ ] Paket yukleme sistemi test et

### Faz 3: Paket Sistemi

Desteklenen oyunlar icin:

1. [ ] Ceviri paketi formati tanimla
2. [ ] Paket indirme mekanizmasi
3. [ ] Paket yukleme (dil dosyasi kopyalama)
4. [ ] Uyumluluk kontrolu
5. [ ] Geri alma (rollback)

### Faz 4: Test ve Polish

1. [ ] Gercek oyunlarla test
2. [ ] Performans optimizasyonu
3. [ ] Hata ayiklama
4. [ ] Installer/deployment

---

## Calisma Modu Aciklamasi

### Mod 1: Desteklenen Oyunlar
```
Kullanici oyunu secer
    |
    v
Uyumluluk kontrolu
    |
    v
Ceviri paketi indir (eger yoksa)
    |
    v
Dil dosyalarini oyuna yukle
    |
    v
Tamamlandi!
```

### Mod 2: Diger Oyunlar
```
Kullanici oyunu secer
    |
    v
Topluluk ceviri paketi ara
    |
    v
Patch uygula (manuel adimlar gerekebilir)
    |
    v
Tamamlandi!
```

---

## Araclar

### Gelistirme
- Qt 6.10.1 + MinGW
- Visual Studio 2022 (Core icin)
- CMake + vcpkg

### Yardimci
- GitHub Copilot Pro (kod tamamlama, refactoring)
- Claude Code (mimari kararlar, debugging)

---

## Ileri Vizyon (v1.x+)

### Gaming Companion AI

Oyun oynarken yaninda bir arkadas gibi AI asistan.

**Konsept:**
```
F12 ile ekran goruntusu al
        |
        v
AI goruntuyu analiz et
        |
        v
Sahneye uygun yorum yap
        |
        v
Overlay ile ekranda goster
```

**Kisilik Sistemi:**
- Sadece "arastirmis" degil, "yasamis" gibi konussun
- Soguk bilgi yerine samimi deneyim paylasimi
- Duruma gore ton degisimi (komik/duygusal/heyecanli)

**Ornek Senaryolar:**

| Sahne | Klasik AI | MakineAI Companion |
|-------|-----------|-------------------|
| Boss savas | "Bu boss'un 3 fazi var..." | "Ilk oynadigimda burda kaldim, sol taraftan dolan" |
| Duygusal an | "Bu karakterin hikayesi..." | "Dostum... neden boyle olmali ki?" |
| Aksiyon | "X tusuna basin" | "Wubba lubba dub dub! Hadi devam!" |
| Panik ani | "Kacis rotasi kuzeyde" | "A-ah bu iyi fikir miydi?!" |

**Teknik Gereksinimler:**
- [ ] Screenshot capture (F12) - MEVCUT
- [ ] Claude API entegrasyonu
- [ ] Oyun tanimlama sistemi
- [ ] Overlay mesaj UI
- [ ] Kisilik/ton motor

**Hedef:** v1.0 veya sonrasi

---

## Notlar

- UI once, Core sonra
- Her adim test edilmeli
- Kucuk commitler, sik push
- Ileri vizyon icin altyapi simdiden hazirlanmali

---

*Bu dokuman aktif olarak guncellenmektedir.*
*CEDRA Interactive - 2026*
