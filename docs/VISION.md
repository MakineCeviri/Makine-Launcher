# MakineAI - Nihai Vizyon

**Bu dokuman uygulamanin ruhudur. Asla silinmemeli.**

---

## Tek Cumle

> MakineAI, oyun guncellemelerinde cevirilerin kirilmasini onleyen akilli bir adaptasyon motorudur.

---

## Iki Parca, Bir Butun

### Makine — Urun (Kullaniciya Donuk)

Turk oyuncular icin ceviri kutuphanesi ve dagitim platformu.

```
Oyuncu: "Elden Ring'i Turkce oynamak istiyorum"
Makine: Kurulu oyunu tespit et → Ceviri paketini indir → Tek tikla kur → Oyna
```

- Steam, Epic, GOG kutuphanelerini otomatik tara
- Topluluk cevirilerini kesfe, kur, guncelle
- Yedekle, geri yukle, guvenli dagitim
- Katalog deneyimi — oyunlari gorselleriyle goster

### MakineAI — Motor (Arka Plan)

Oyun guncellemelerinden ogrenip cevirileri otomatik uyarlayan sistem.

```
Sorun:  Assassin's Creed guncellendi → Turkce yama oyunu bozdu
Cozum:  MakineAI degisikligi tespit etti → Yamayı otomatik uyarladi → Kullanici fark etmedi bile
```

---

## Gercek Sorun

Oyun ceviri topluluklarinin en buyuk sorunu:

```
1. Cevirmen aylarca calisir, yama yayinlar
2. Oyun studiosu guncelleme cikarir
3. Yama bozulur
4. Cevirmen ya tekrar ugrasir ya birakir
5. Kullanici Turkce oynayamaz
```

Bu dongu kirılmalı. MakineAI bunu otomatize eder:

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Oyun         │────→│ Degisiklik   │────→│ Otomatik     │
│ Guncellendi  │     │ Analizi      │     │ Adaptasyon   │
└──────────────┘     └──────────────┘     └──────────────┘
       │                    │                     │
       ▼                    ▼                     ▼
  Hash degisti        Ne degisti?           Yama uyarlandi
  Versiyon artti      Yeni string?          Kullanici bilgilendirildi
  Dosyalar farkli     Yapi degisti?         Cevirmen sadece yenilere bakar
```

---

## Adaptasyon Motoru

### Tespit
- Yamalanmis oyun dosyalarinin hash'lerini kaydet
- Uygulama acildiginda veya arka planda kontrol et
- Steam API'den versiyon bilgisi al

### Analiz
- Eski ve yeni dosyalari karsilastir (diff)
- Metin tabanli formatlarda structural diff (JSON, XML, INI)
- Degismeyen, tasinan, eklenen, silinen string'leri ayir

### Uyarlama
- Degismeyen string'ler → dokunma
- Tasinan string'ler → fuzzy match ile yeni konuma tasi
- Yeni string'ler → "ceviri gerekli" olarak isaretle
- Catisan dosyalar → akilli merge

### Dogrulama
- Uyarlanan yamanin dosya butunlugunu kontrol et
- Bilinen kaliplarla test et
- Sorun varsa kullaniciyi uyar, yedekten geri don

---

## Kulturel Kimlik

MakineAI Turkce konusuyor — sadece dil degil, **kultur**:

```
Yama bozuldu:     "Sakin ol, guncelleme yamayı bozmus ama ben hallettim."
Yama hazir:       "Turkce yama hazir! Iyi oyunlar."
Ceviri eksik:     "3 yeni string var, cevirmen arkadaslara haber verelim."
```

Soguk teknik mesajlar degil, samimi bilgilendirme.

---

## Tamamlanma Kriterleri

> **Bu ozellikler calistiginda, v1.0 TAMAMLANMISTIR:**
>
> 1. ✅ Oyun kutuphanelerini gercekten tara (Steam/Epic/GOG)
> 2. ✅ Ceviri paketini kur/kaldir
> 3. ⏳ Sunucudan ceviri paketi indir
> 4. ⏳ Oyun guncellemesini tespit et
> 5. ⏳ Degisiklik analizi yap
> 6. ⏳ Cevirileri otomatik uyarla
> 7. ⏳ Uyarlamayi dogrula ve uygula

---

## Ileri Vizyon (v2.0+)

### Topluluk Platformu
- Cevirmenler icin katkı sistemi
- Ceviri kalite puanlama
- Otomatik ceviri onerisi (AI destekli)

### Gaming Companion (Gelecek)
- Oyun sirasinda F12 ile screenshot → AI analiz
- Sahneye uygun kulturel yorum
- "Yaninda oturan arkadas" deneyimi

---

## Son Soz

```
MakineAI sadece bir ceviri araci degil.
Oyun guncellemelerinin cevirileri kirmasini onleyen bir muhendislik cozumu.
Turk oyuncular icin oyunlari oynanabilir kilan bir makine.

Bu vizyon tamamlandiginda, uygulama BITMISTIR.
```

---

*MakineAI — 2026*
*Bu dokuman ASLA silinmemelidir.*
