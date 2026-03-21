# Güvenlik Politikası

Makine-Launcher projesinin güvenliği bizim için son derece önemlidir. Bu belge,
güvenlik açıklarının nasıl bildirileceğini ve sürecin nasıl işlediğini açıklar.

## Desteklenen Sürümler

| Sürüm | Destek Durumu |
|--------|---------------|
| v0.1.0-pre-alpha | :white_check_mark: Aktif geliştirme |
| Önceki sürümler | :x: Desteklenmiyor |

> **Not:** Proje henüz pre-alpha aşamasındadır. Kararlı sürüm yayınlandığında
> bu tablo güncellenecektir.

## Güvenlik Açığı Bildirme

Bir güvenlik açığı keşfettiyseniz, lütfen bunu **sorumlu açıklama** (responsible
disclosure) ilkelerine uygun şekilde bildirin.

### Bildirme Yöntemi

**GitHub Security Advisories** kullanarak özel olarak bildirimde bulunun:

1. [Makine-Launcher deposuna](https://github.com/MakineCeviri/Makine-Launcher) gidin.
2. **Security** sekmesine tıklayın.
3. **Advisories** bölümünden **New draft security advisory** seçin.
4. Açığı mümkün olduğunca ayrıntılı şekilde açıklayın.

> **Lütfen güvenlik açıklarını public issue olarak açmayın.** Bu, açığın kötüye
> kullanılma riskini artırır.

### Bildiriminizde Bulunması Gerekenler

- Açığın kısa ve net bir açıklaması
- Açığı tetikleyen adımlar (tekrarlanabilir senaryo)
- Etkilenen bileşen veya dosya (biliniyorsa)
- Olası etki ve ciddiyet değerlendirmesi
- Varsa düzeltme önerisi

## Süreç ve Yanıt Süreleri

| Aşama | Süre |
|-------|------|
| İlk onay (acknowledgement) | 72 saat içinde |
| İlk değerlendirme | 7 gün içinde |
| Düzeltme veya azaltma planı | Ciddiyete göre değişir |
| Güvenlik danışma yayını | Düzeltme yayınlandıktan sonra |

1. **Onay:** Bildiriminizi aldığımızı 72 saat içinde onaylarız.
2. **Değerlendirme:** Açığın ciddiyetini ve etkisini değerlendiririz.
3. **Düzeltme:** Kritik açıklar için öncelikli düzeltme geliştiririz.
4. **Yayın:** Düzeltme yayınlandıktan sonra güvenlik danışması (security advisory)
   yayınlarız ve sizi katkınız için kredilendiriz (isterseniz).

## Sorumlu Açıklama Politikası

- Güvenlik açıklarını yalnızca yukarıda belirtilen yöntemle bildirin.
- Açığı, düzeltme yayınlanana kadar kamuya açıklamayın.
- Açığı istismar etmeyin veya başkalarının verilerine erişmeyin.
- İyi niyetli güvenlik araştırmacılarına karşı yasal işlem başlatılmayacaktır.

Sorumlu açıklama politikasına uyan araştırmacıları güvenlik danışmamızda
kredilendirir ve teşekkürlerimizi sunarız.

## Kapsam Dışı

Aşağıdaki durumlar bu güvenlik politikasının kapsamı dışındadır:

- Sosyal mühendislik saldırıları (phishing vb.)
- Hizmet reddi (DoS/DDoS) saldırıları
- Fiziksel erişim gerektiren saldırılar
- Desteklenmeyen sürümlerdeki açıklar
- Üçüncü taraf bağımlılıklarda bilinen ve yukarı akış (upstream) tarafından
  henüz düzeltilmemiş açıklar
- Kullanıcının kendi sistemindeki yapılandırma hataları

## İletişim

Güvenlik ile ilgili genel sorularınız için
[GitHub Discussions](https://github.com/MakineCeviri/Makine-Launcher/discussions)
sayfasını kullanabilirsiniz.

Hassas güvenlik konuları için yalnızca GitHub Security Advisories üzerinden
iletişime geçin.

---

Bu politika, projenin gelişimine paralel olarak güncellenecektir.
