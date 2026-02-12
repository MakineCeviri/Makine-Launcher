# MakineAI - Nihai Vizyon

**Bu doküman uygulamanın ruhudur. Asla silinmemeli.**

---

## Tek Cümle

> MakineAI, oyun güncellemelerinde çevirilerin kırılmasını önleyen akıllı bir adaptasyon motorudur.

---

## İki Parça, Bir Bütün

### Makine — Ürün (Kullanıcıya Dönük)

Türk oyuncular için çeviri kütüphanesi ve dağıtım platformu.

```
Oyuncu: "Elden Ring'i Türkçe oynamak istiyorum"
Makine: Kurulu oyunu tespit et → Çeviri paketini indir → Tek tıkla kur → Oyna
```

- Steam, Epic, GOG kütüphanelerini otomatik tara
- Topluluk çevirilerini keşfe, kur, güncelle
- Yedekle, geri yükle, güvenli dağıtım
- Katalog deneyimi — oyunları görselleriyle göster

### MakineAI — Motor (Arka Plan)

Oyun güncellemelerinden öğrenip çevirileri otomatik uyarlayan sistem.

```
Sorun:  Assassin's Creed güncellendi → Türkçe yama oyunu bozdu
Çözüm:  MakineAI değişikliği tespit etti → Yamayı otomatik uyarladı → Kullanıcı fark etmedi bile
```

---

## Gerçek Sorun

Oyun çeviri topluluklarının en büyük sorunu:

```
1. Çevirmen aylarca çalışır, yama yayınlar
2. Oyun stüdyosu güncelleme çıkarır
3. Yama bozulur
4. Çevirmen ya tekrar uğraşır ya bırakır
5. Kullanıcı Türkçe oynayamaz
```

Bu döngü kırılmalı. MakineAI bunu otomatize eder:

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Oyun         │────→│ Değişiklik   │────→│ Otomatik     │
│ Güncellendi  │     │ Analizi      │     │ Adaptasyon   │
└──────────────┘     └──────────────┘     └──────────────┘
       │                    │                     │
       ▼                    ▼                     ▼
  Hash değişti        Ne değişti?           Yama uyarlandı
  Versiyon arttı      Yeni string?          Kullanıcı bilgilendirildi
  Dosyalar farklı     Yapı değişti?         Çevirmen sadece yenilere bakar
```

---

## Adaptasyon Motoru

### Tespit
- Yamalanmış oyun dosyalarının hash'lerini kaydet
- Uygulama açıldığında veya arka planda kontrol et
- Steam API'den versiyon bilgisi al

### Analiz
- Eski ve yeni dosyaları karşılaştır (diff)
- Metin tabanlı formatlarda structural diff (JSON, XML, INI)
- Değişmeyen, taşınan, eklenen, silinen string'leri ayır

### Uyarlama
- Değişmeyen string'ler → dokunma
- Taşınan string'ler → fuzzy match ile yeni konuma taşı
- Yeni string'ler → "çeviri gerekli" olarak işaretle
- Çatışan dosyalar → akıllı merge

### Doğrulama
- Uyarlanan yamanın dosya bütünlüğünü kontrol et
- Bilinen kalıplarla test et
- Sorun varsa kullanıcıyı uyar, yedekten geri dön

---

## Kültürel Kimlik

MakineAI Türkçe konuşuyor — sadece dil değil, **kültür**:

```
Yama bozuldu:     "Sakin ol, güncelleme yamayı bozmuş ama ben hallettim."
Yama hazır:       "Türkçe yama hazır! İyi oyunlar."
Çeviri eksik:     "3 yeni string var, çevirmen arkadaşlara haber verelim."
```

Soğuk teknik mesajlar değil, samimi bilgilendirme.

---

## Tamamlanma Kriterleri

> **Bu özellikler çalıştığında, v1.0 TAMAMLANMIŞTIR:**
>
> 1. ✅ Oyun kütüphanelerini gerçekten tara (Steam/Epic/GOG)
> 2. ✅ Çeviri paketini kur/kaldır
> 3. ⏳ Sunucudan çeviri paketi indir
> 4. ⏳ Oyun güncellemesini tespit et
> 5. ⏳ Değişiklik analizi yap
> 6. ⏳ Çevirileri otomatik uyarla
> 7. ⏳ Uyarlamayı doğrula ve uygula

---

## İleri Vizyon (v2.0+)

### Topluluk Platformu
- Çevirmenler için katkı sistemi
- Çeviri kalite puanlama
- Otomatik çeviri önerisi (AI destekli)

### Gaming Companion (Gelecek)
- Oyun sırasında F12 ile screenshot → AI analiz
- Sahneye uygun kültürel yorum
- "Yanında oturan arkadaş" deneyimi

---

## Son Söz

```
MakineAI sadece bir çeviri aracı değil.
Oyun güncellemelerinin çevirileri kırmasını önleyen bir mühendislik çözümü.
Türk oyuncular için oyunları oynanabilir kılan bir makine.

Bu vizyon tamamlandığında, uygulama BİTMİŞTİR.
```

---

*MakineAI — 2026*
*Bu doküman ASLA silinmemelidir.*
