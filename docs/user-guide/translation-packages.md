# Çeviri Paketi Kurulumu — Kapsamlı Kılavuz

Bu kılavuz, Makine-Launcher çeviri paketlerini nasıl kurduğunu, güncellediğini ve kaldırdığını
detaylı olarak açıklar. Olası hata durumlarını ve çözüm yollarını da kapsar.

---

## İçindekiler

1. [Genel Bakış](#genel-bakış)
2. [Kurulum Öncesi Kontroller](#kurulum-öncesi-kontroller)
3. [Kurulum Türleri](#kurulum-türleri)
4. [Kurulum Süreci](#kurulum-süreci)
5. [Güncelleme](#güncelleme)
6. [Kaldırma](#kaldırma)
7. [Yedekleme ve Kurtarma](#yedekleme-ve-kurtarma)
8. [Hata Mesajları ve Çözümleri](#hata-mesajları-ve-çözümleri)
9. [Sık Sorulan Sorular](#sık-sorulan-sorular)

---

## Genel Bakış

Makine-Launcher, topluluk tarafından hazırlanan Türkçe çeviri paketlerini oyunlara uygular.
Her çeviri paketi bir oyuna özeldir ve şunları içerebilir:

- **Metin çevirileri** — diyalog, menü, alt yazı dosyaları
- **Font dosyaları** — Türkçe karakter desteği (ş, ç, ğ, ı, ö, ü)
- **Ses dosyaları** — Türkçe seslendirme (dubbing)
- **Yama dosyaları** — Motor düzeyinde düzeltmeler

### Paket Bilgileri

Her paketin detay sayfasında şu bilgileri görebilirsiniz:

| Bilgi | Açıklama |
|-------|----------|
| **Oyun Adı** | Çevirinin ait olduğu oyun |
| **Motor** | Unity, Unreal Engine, RPG Maker, vb. |
| **Sürüm** | Çeviri paketinin sürümü |
| **Boyut** | İndirme boyutu |
| **Dosya Sayısı** | Paketteki toplam dosya sayısı |
| **Son Güncelleme** | Son değişiklik tarihi |
| **Katılımcılar** | Çevirmenlerin isimleri ve rolleri |

---

## Kurulum Öncesi Kontroller

Makine-Launcher, dosyaları kopyalamaya başlamadan önce birkaç güvenlik kontrolü yapar.
Bu kontroller kurulumun güvenli bir şekilde tamamlanmasını sağlar.

### 1. Oyun Klasörü Doğrulaması

Oyun klasörünün geçerli ve erişilebilir olduğu kontrol edilir.

> **Hata:** Oyun klasörü belirtilmedi
>
> **Çözüm:** Oyunu yeniden tespit edin veya klasör yolunu manuel olarak seçin.

### 2. Disk Alanı Kontrolü

Kurulum başlamadan önce yeterli disk alanı olup olmadığı kontrol edilir.
Gerekli alan, paket boyutunun **2 katı** olarak hesaplanır (yedekleme için ek alan).

> **Hata:** Yetersiz disk alanı: X MB gerekli, Y MB mevcut
>
> **Çözüm:** Diskinizdeki gereksiz dosyaları silerek yer açın, ardından tekrar deneyin.

### 3. Yazma İzni Kontrolü

Oyun klasörüne yazma izni olup olmadığı test edilir. Özellikle Program Files gibi
korumalı dizinlerde bu kontrol önemlidir.

> **Hata:** Bu klasöre yazma izni yok. Uygulamayı yönetici olarak çalıştırmayı deneyin.
>
> **Çözüm:**
> - Makine-Launcher simgesine sağ tık ve "Yönetici olarak çalıştır" ile açın
> - Veya oyun klasörünün güvenlik izinlerini kontrol edin

### 4. Çeviri Dosyaları Kontrolü

İndirilmiş çeviri paketinin fiziksel olarak mevcut olup olmadığı kontrol edilir.

> **Hata:** Çeviri dosyaları bulunamadı: [Oyun Adı]
>
> **Çözüm:** Paketi yeniden indirin. İndirme sırasında internet bağlantınızda sorun olmuş olabilir.

---

## Kurulum Türleri

Makine-Launcher, farklı oyun motorları ve çeviri türleri için çeşitli kurulum yöntemleri kullanır.
Doğru yöntem otomatik olarak seçilir — kullanıcının bir şey yapmasına gerek yoktur.

### Overlay (Varsayılan)

En yaygın kurulum türüdür. Çeviri dosyaları oyun klasörüne kopyalanır:

- Var olan dosyalar yedeklenir, ardından üzerine yazılır
- Yeni dosyalar doğrudan oluşturulur
- Her dosyanın kaydı tutulur (kaldırma için)

**Destekleyen motorlar:** Unity, Unreal Engine, RPG Maker, RenPy, GameMaker, Godot, Source ve daha fazlası.

### Seçenekli Kurulum (Options)

Bazı çeviriler birden fazla bileşen içerir. Kurulum sırasında size seçim sunulur:

- **Türkçe Yama** — Metin çevirisi ve fontlar
- **Türkçe Seslendirme** — Ses dosyaları (dubbing)
- **İkisi birlikte** — Kombine kurulum

Her seçenek farklı dosyalar içerir ve ayrı ayrı veya birlikte kurulabilir.

### Varyant Seçimi

Bazı oyunlar için farklı varyantlar mevcuttur:

- **Platform varyantları:** Steam, Game Pass, Epic sürümleri farklı çeviri dosyaları gerektirebilir
- **Sürüm varyantları:** Oyunun farklı versiyonları (ör. 1.00, 1.04) için ayrı çeviriler

Kurulum sırasında doğru varyantı seçmeniz istenir.

### Kullanıcı Dizini (userPath)

Bazı oyunlar çeviri dosyalarını oyun klasörü yerine kullanıcı dizinine bekler
(ör. %APPDATA%, Belgelerim). Bu durumda Makine-Launcher dosyaları doğru konuma yönlendirir.

### Script (Recipe)

Karmaşık kurulumlar için özel adımlar tanımlanabilir:

| Adım Türü | Açıklama |
|------------|----------|
| **copy** | Dosya kopyalama |
| **copyDir** | Klasör kopyalama (alt klasörlerle birlikte) |
| **run** | Program çalıştırma (ör. patcher) |
| **delete** | Dosya silme |
| **installFont** | Windows font yükleme |
| **copyToDesktop** | Masaüstüne kısayol/dosya kopyalama |
| **rename** | Dosya yeniden adlandırma |
| **setSteamLanguage** | Steam dil ayarını değiştirme |

---

## Kurulum Süreci

### Adım Adım Akış

```
1. Ön kontroller (disk, izin, dosya varlığı)
        |
2. Kurtarma günlüğü başlatılır (journal kaydı)
        |
3. Mevcut dosyalar yedeklenir (.makine_backup)
        |
4. Çeviri dosyaları kopyalanır
        |
5. İlerleme çubuğu güncellenir
        |
6. Kurulum durumu kaydedilir
        |
7. Tamamlandı bildirimi
```

### İlerleme Çubuğu

Kurulum sırasında her dosya için ilerleme durumu gösterilir:

```
Kopyalanıyor 15/42: turkish_font.ttf
[============--------] %36
```

Seçenekli kurulumlarda hangi seçeneğin işlendiği de belirtilir:

```
Türkçe Yama — Adım 3/7: localization/tr.json
[========------------] %43
```

### İptal Etme

Kurulum sırasında **İptal** butonuna basarak işlemi durdurabilirsiniz.
İptal edilen kurulum yarıda kesilir, ancak o ana kadar kopyalanan dosyalar kalır.
Kaldırma işlemiyle temizleyebilirsiniz.

### Dosya Takibi

Makine-Launcher, kurduğu her dosyanın kaydını iki kategoride tutar:

- **Eklenen dosyalar** — Oyunda bulunmayan, yeni oluşturulan dosyalar
- **Değiştirilen dosyalar** — Oyunda zaten var olan, yedeklenip üzerine yazılan dosyalar

Bu ayrım, kaldırma sırasında doğru davranışı sağlar:
- Eklenen dosyalar **silinir**
- Değiştirilen dosyalar **yedekten geri yüklenir**

---

## Güncelleme

Çeviri paketi güncellendiğinde (yeni sürüm), Makine-Launcher akıllı güncelleme yapar:

1. Eski kurulumun dosya listesi kontrol edilir
2. Artık gerekmeyen dosyalar temizlenir
3. Yeni dosyalar kopyalanır
4. Kayıtlar güncellenir

**Güncelleme, kaldırıp yeniden kurma işlemine göre daha hızlıdır** çünkü
yalnızca değişen dosyaları işler.

> **Not:** İlk kez kuruluyorsa güncelleme yapılamaz.
>
> **Hata:** Yama kurulu değil, güncelleme yapılamaz
>
> **Çözüm:** Önce normal kurulum yapın.

---

## Kaldırma

Çeviriyi kaldırmak oyunu orijinal haline döndürür:

1. **Eklenen dosyalar** silinir
2. **Değiştirilen dosyalar** yedekten geri yüklenir
3. **Fontlar** Windows font dizininden kaldırılır
4. **Masaüstü dosyaları** temizlenir
5. **Steam dil ayarı** orijinal değerine döndürülür
6. **Yeniden adlandırmalar** eski haline çevrilir

### Güvenlik

- **Yol geçişi koruması:** Dosya silme işlemi oyun klasörü dışına çıkamaz
- **Yedek doğrulama:** Yalnızca Makine-Launcher tarafından oluşturulan yedekler geri yüklenir

---

## Yedekleme ve Kurtarma

### Otomatik Yedekleme

Her kurulumda, üzerine yazılacak orijinal dosyalar otomatik olarak yedeklenir
(.makine_backup uzantısıyla). Bu yedekler kaldırma sırasında kullanılır.

### Crash Recovery (Çökme Kurtarma)

Makine-Launcher, beklenmedik kapanmalara karşı **Operation Journal** sistemi kullanır:

**Nasıl Çalışır:**

1. Kurulum başladığında bir journal kaydı oluşturulur
2. Her dosya kopyalandığında journal güncellenir
3. Kurulum tamamlandığında journal kapatılır
4. Hata durumunda journal **korunur** (silinmez)

**Kurtarma Senaryoları:**

| Durum | Ne Olur |
|-------|---------|
| Kurulum sırasında bilgisayar kapandı | Sonraki açılışta kurtarma önerilir |
| Kurulum sırasında uygulama çöktü | Sonraki açılışta kurtarma önerilir |
| Kullanıcı iptal etti | Journal temizlenir, dosyalar kalır |
| Disk doldu (kurulum sırasında) | Journal korunur, kurtarma mümkün |

**Kurtarma, yarıda kalan dosyaları temizler ve oyunu tutarlı bir duruma getirir.**

---

## Hata Mesajları ve Çözümleri

### Kurulum Hataları

| Hata Mesajı | Nedeni | Çözüm |
|-------------|--------|-------|
| Oyun klasörü belirtilmedi | Oyun yolu boş veya geçersiz | Oyunu yeniden tespit edin |
| Yükleme paketi bulunamadı | Paket kataloğunda oyun yok | Katalog güncellemesini bekleyin |
| Yetersiz disk alanı: X MB gerekli, Y MB mevcut | Diskte yeterli alan yok | Yer açın ve tekrar deneyin |
| Bu klasöre yazma izni yok... | Korumalı dizin (ör. Program Files) | Yönetici olarak çalıştırın |
| Çeviri dosyaları bulunamadı: [Oyun] | Paket indirilmemiş veya bozuk | Paketi yeniden indirin |

### Kopyalama Hataları

Bu hatalar dosya kopyalama sırasında oluşabilir:

| Hata Mesajı | Nedeni | Çözüm |
|-------------|--------|-------|
| Disk alanı doldu, kurulum durduruluyor | Kopyalama sırasında disk doldu | Yer açın, kaldırıp tekrar kurun |
| Dosya yazma izni yok... | Tek bir dosyaya yazma izni yok | Klasör izinlerini kontrol edin veya yönetici olarak çalıştırın |
| X/Y dosya kopyalanamadı | Bazı dosyalar kopyalanamadı | Oyunu kapatın ve tekrar deneyin |
| X/Y adımda hata oluştu | Script adımlarında hata | Oyunu kapatın, antivirüsü kontrol edin |
| Kurulum iptal edildi | Kullanıcı iptal butonuna bastı | Normal durum, tekrar kurabilirsiniz |

### Güncelleme Hataları

| Hata Mesajı | Nedeni | Çözüm |
|-------------|--------|-------|
| Yama kurulu değil, güncelleme yapılamaz | Çeviri hiç kurulmamış | Önce normal kurulum yapın |
| X/Y dosya güncellenemedi | Güncelleme sırasında hata | Kaldırıp yeniden kurun |

### Yaygın Çözümler

Yukarıdaki çözümler işe yaramazsa bu adımları deneyin:

1. **Oyunu tamamen kapatın** — Oyun çalışırken dosyalar kilitli olabilir.
   Steam overlay, launcher gibi arka plan işlemlerini de kapatın.

2. **Antivirüs istisnası ekleyin** — Bazı antivirüsler dosya kopyalamayı engelleyebilir.
   Oyun klasörünü ve Makine-Launcher klasörünü istisna olarak ekleyin.

3. **Yönetici olarak çalıştırın** — Makine-Launcher simgesine sağ tık, ardından
   "Yönetici olarak çalıştır" ile açın. Özellikle C:\Program Files\ altındaki
   oyunlar için gereklidir.

4. **Steam dosya doğrulaması** — Oyun dosyaları bozuksa Steam uygulamasında
   "Oyun dosyalarının bütünlüğünü doğrula" seçeneğini kullanın,
   ardından çeviriyi tekrar kurun.

5. **Kaldırıp yeniden kurun** — Sorun devam ediyorsa çeviriyi kaldırın ve baştan kurun.

---

## Sık Sorulan Sorular

### Çeviri oyunumu bozar mı?

**Hayır.** Makine-Launcher, üzerine yazacağı her dosyayı önceden yedekler. Çeviriyi kaldırdığınızda
oyun orijinal haline döner. Ayrıca Steam "dosya doğrulama" özelliği her zaman orijinali
geri yükleyebilir.

### Çeviri kuruluyken oyun güncellenirse ne olur?

Oyun güncellemesi çeviri dosyalarını silebilir veya üzerine yazabilir. Bu durumda
çeviriyi tekrar kurmanız yeterlidir. Makine-Launcher güncelleme gerekip gerekmediğini
tespit edebilir.

### Birden fazla çeviri seçeneği ne anlama geliyor?

Bazı oyunlarda (ör. Elden Ring) hem metin çevirisi hem de seslendirme ayrı ayrı sunulur.
İstediğiniz bileşenleri seçerek kurabilirsiniz. Sadece metin istiyorsanız yalnızca
"Türkçe Yama" seçeneğini işaretleyin.

### Varyant nedir?

Bazı oyunların farklı sürümleri veya platform versiyonları farklı çeviri dosyaları
gerektirir. Örneğin bir oyunun Steam ve Game Pass sürümleri farklı dosya yapısına
sahip olabilir. Makine-Launcher doğru varyantı otomatik tespit etmeye çalışır; edemezse
size sorar.

### Kurulum ne kadar sürer?

Çoğu çeviri 10-30 saniye içinde tamamlanır. Büyük seslendirme paketleri
(500 MB+) daha uzun sürebilir. İlerleme çubuğundan durumu takip edebilirsiniz.

### Kaldırdığım çeviriyi tekrar kurabilir miyim?

**Evet.** Çeviriyi kaldırdıktan sonra istediğiniz zaman tekrar kurabilirsiniz.
Paket dosyaları yerel önbellekte saklanır.

### Makine-Launcher çöktü, oyunuma bir şey oldu mu?

Makine-Launcher crash recovery (çökme kurtarma) sistemine sahiptir. Bir sonraki açılışta
yarıda kalan işlemi tespit eder ve oyunu tutarlı bir duruma geri getirir.
Orijinal dosyalarınız yedekte korunur.

### Dosya kilitli hatası alıyorum, ne yapmalıyım?

Bu hata genellikle oyun veya ilgili bir program (Steam overlay, launcher) dosyayı
kullanırken oluşur. Oyunu ve ilgili tüm programları kapatıp tekrar deneyin.
Makine-Launcher kilitli dosyalarda otomatik olarak bir kez yeniden deneme yapar.

### Oyunum kurulu ama listede görünmüyor, ne yapmalıyım?

Makine-Launcher; Steam, Epic Games ve GOG gibi dijital mağazalardan yüklenen oyunları
otomatik olarak tespit eder. Ancak CD/DVD ile kurulan veya mağaza dışı yollarla
edinilen oyunlar otomatik taramada görünmez.

Bu durumda ana ekrandaki **"Oyun Ekle"** butonuna basarak oyun klasörünü manuel
olarak seçebilirsiniz. Makine-Launcher, klasörü tarayarak oyun motorunu ve uyumlu
çeviri paketini otomatik olarak tespit edecektir.

---

## Sonraki Adımlar

- [Hızlı Başlangıç](getting-started.md) — İlk çevirinizi yapın
- [Kurulum](installation.md) — Makine-Launcher kurulumu
- [Discord Topluluğu](https://discord.com/invite/QDezpy4QtV) — Yardım ve destek
