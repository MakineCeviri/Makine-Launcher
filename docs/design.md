# Tasarım sistemi — Apple yapısı, Makine tonu (2026-10-22)

Launcher'ın arayüzü Apple'ın insan arayüzü yönergelerine (macOS / App Store / Apple TV)
göre kuruludur; renk tonu ve kaplar 0.1.5'ten gelir (kullanıcı o tonu ve kenarlarını
istedi): ışıklı gece zemini, her bölüm kendi kabında, kabın üst kenarı ışık alır, kayan
içerik kenarlarda kesilmez, kaybolur. Yeni bir ekran ya da bileşen bu belgedeki parçalarla yapılır; yeni renk,
yazı boyu ya da düğme biçimi uydurulmaz. Her eylem ne yapacağını, her durum ne demek
olduğunu düz Türkçe bir cümleyle söyler (`HelpScreen.qml` davranışla aynı kalır).

## Kabuk

| Parça | Dosya | Not |
|---|---|---|
| Kenar çubuğu | `components/Sidebar.qml` | macOS kaynak listesi, pencerenin çerçevesi üstünde (kart değil): en üstte ad, **oyun araması** (yazınca "Tüm Oyunlar" açılır ve süzülür; Ctrl+F, Aşağı/Enter sonuçlara), Ana Sayfa · Tüm Oyunlar · Kütüphanem (bekleyen yama sayısı vurgu kapsülünde) · Yardım · Topluluk (Topluluk sayfası, Discord, Destek ol); gruplar ışık alan ince çizgiyle (`FadeLine { lit: true }`) ayrılır. Seçili öğe vurgunun kendisi, üstünde beyaz; simgeler vurgu renginde ve üzerine gelince canlanır (`NavIcon`). Ayak: `SidebarStatus.qml`. Sayfalar kendi arama kutusunu koymaz. |
| Pencere düğmeleri | `components/WindowControls.qml` | Windows 11 başlık düğmeleri (46 × 32, ince 10 px işaretler): tepsiye gizle, küçült, kapat; sayfa panelinin sağ üst köşesine bitişik (hoş geldin akışında pencerenin), kapat düğmesinin dış köşesi panelin köşesini izler, üstüne gelince Windows kırmızısı. Pencere arkadayken işaretler soluklaşır. Başlık çubuğu yok. Sayfanın üst 44 px'i pencereyi sürükler (`Main.qml` `dragStrip`), sayfalar bu şeridi boş bırakır (başlık `topMargin: 52`). |
| Pencereye sığdırma | `Main.qml` `stage`, `Dimensions.uiZoom` | Sayfalar `Dimensions.designWidth` (1180) genişliğe göre dizilir; daha dar pencere hepsini orantılı küçültür (en az %72), daha geniş pencere büyütmez, yer açar. Pencerenin gösterdiği her şey `stage` üstündedir; pencereler, menüler, ipuçları aynı katsayıyla ölçeklenir (`scale: Dimensions.uiZoom`). En küçük pencere 1000 × 689 (900:620 oranı, `main.cpp` kilitler), ilk açılış 1240 geniş; ekranın çalışma alanı küçükse en küçük boyut ona iner. |
| Sayfa paneli | `Main.qml` `mainContent` + `panelFrame` | Pencere bir çerçevedir (`Theme.chrome`, en koyu ton): kenar çubuğu doğrudan onun üstünde durur, sayfalar ise kenar çubuğunun kenarına bitişik tek bir yuvarlak, ışıklı panelde (üst/sağ/alt 8 px boşluk, köşe `radiusSection`). `AppBackground` panelin zeminidir; köşeler çerçeve renginin içeriğin köşelerine boyanmasıyla yuvarlanır (katman/maske yok), üstüne ince çizgi, ışık alan üst kenar ve daha soluk ışıklı alt kenar. |
| Geçmiş | `Main.qml` `historyBack()` / `historyForward()` | Tarayıcı gibi: sayfalar ve açılan oyunlar sırayla kaydedilir; farenin yan tuşları (geri/ileri), Alt+← / Alt+→, Esc ve oyun sayfasının geri düğmesi bu geçmişte yürür. Geri düğmesi gideceği yerin adını yazar (`window.backTitle`). |
| Logo | `components/AppLogo.qml` | Discord sunucusunun simgesiyle aynı (yuvarlatılmış uygulama simgesi, `resources/images/logo.png`, exe simgesi `app_icon.ico`). Durgun; üstüne gelince (`playing` ile her zaman) renkleri Discord'daki gibi akar (`logo_anim.webp`, WebP eklentisi yoksa durgun kalır). |
| Uygulama uyarıları | `components/AppNotice.qml` | Bağlantı yok, bütünlük, yarım kalan işlem: sayfanın üstünde kapsül. |
| Bildirim | `components/GameToast.qml` | macOS bildirimi gibi sağ üstten kayar, 8 sn (imleç üstündeyken durur). |
| Makine (yoldaş) | `Sidebar.qml` `companionDock` · `home/MakineCorner.qml` → `CompanionService` (Makine kapalı kaynak: `Makine-Companion` deposu) | Ana sayfada öne çıkan görselin yanındaki kendi köşesinde (bir `Card`, görselle aynı yükseklik, `radiusHero`), başka sayfalarda kenar çubuğunun boş yerinde süzülen robot; iki yerde de aynı boydadır; QML değil, kendi penceresinde C++ ile çizilir (ayrıntısı `Makine-Companion` deposunda). Köşesine ışınlanır (bir ışık çizgisine incelip orada yeniden belirir), başka her yere uçar; launcher açılınca köşesinde başlar. İstenirse masaüstünde ya da en küçük, elsiz haliyle sağ altta saatin üstünde durur (görev çubuğuna çıkmaz). Rengi logonun paletinden akar, yeni renk almaz. Kenar çubuğuna öğe eklenirse boşluk (`companionDock`) küçülür, Makine de onunla küçülür: boşluk kalmalı. Bir pencere (`BaseDialog`) açıkken görünür kalır; pencerenin kartı köşesini örterse kenar çubuğuna geçer, odasını örterse solar. Nefes alması ve uyuması "sonsuz atım yok" kuralının bilinçli istisnasıdır; 45 sn ilgilenilmezse uyur. |

## Renkler — `theme/Theme.qml`

Koyu: neredeyse siyah zemin (#07070A, vurgudan %3,5 renk alır; kaplar %5), vurgunun iki yumuşak ışığı (`AppBackground`),
yarı saydam kaplar, sıcak kırık beyaz yazı. Açık görünüm Apple'ın gri-beyazı. Ad → kullanım:

| Token | Kullanım |
|---|---|
| `chrome` | Pencerenin çerçevesi: kenar çubuğunun durduğu, sayfa panelini saran en koyu ton (açıkta #E7E7EC) |
| `bgPrimary` | Sayfa panelinin zemini (#07070A / #F4F4F6); üstünü `AppBackground` aydınlatır |
| `container` | Her kap: kenar çubuğu, bölüm, liste, kart (`Card.qml`; koyu: %82 opak gece) |
| `containerBorder`, `containerHighlight` | Kabın ince çizgisi ve ışık alan üst kenarı |
| `containerSolid` | Kabın yarı saydam olmayan hâli (`container` ışığı geçirir) |
| `surface`, `surfaceLight` | Opak yüzey (saydamlığın olmayacağı yer), kap içindeki iç öğe |
| `surfaceElevated` | Pencere (sheet), ipucu, bildirim |
| `fill`, `fillSecondary`, `fillTertiary`, `fillQuaternary` | Denetimlerin zemini; koyuda beyaz ışık (gri dolgu geceyi bulandırır) |
| `separator` | Satırlar arası ince çizgi |
| `textPrimary` › `textSecondary` › `textMuted` › `textTertiary` › `textDisabled` | Etiket kademeleri (koyuda sıcak: #ECE9E3 …) |
| `primary` | Vurgunun dolgusu: ana düğme, seçim, ilerleme (varsayılan mor #8B5CF6) |
| `primaryText` | Vurgu yazı/simge olarak: bağlantı, seçili kenar çubuğu öğesi (#A78BFA) |
| `success`, `warning`, `error` | Durum. Yalnız renge güvenilmez; yanında her zaman sözcük ya da simge olur. |
| `glassOnArt` | Kapak/görsel üstündeki koyu cam (düğme, rozet) |

Vurgu ön ayarları (`SettingsManager.accentPresets()`, `Theme._accentPresets`): Makine'nin
0.1.5 paleti; her biri [koyuda dolgu, koyuda yazı, açıkta dolgu]. `appearance/designVersion`
3'e geçerken 0.1.7'nin varsayılanı mavi bir kez mora döner; sonra seçilen kalır.

## Yazı — `theme/Dimensions.qml`

Uygulamanın yazısı **Inter Display** (SF Pro yerine; Inter'in metin kesiminden sıkı, SF'ye yakın; `main.cpp` varsayılan font, eksik karakter Inter'den). 11 px ve altı (`caption1`/`caption2`) Inter'in metin kesimi: `font.family: Dimensions.fontText` — Display orada sıkışır.

| Stil | px | Ağırlık |
|---|---|---|
| `largeTitle` | 30 | Bold, Display, `trackingTitle` |
| `title1` / `title2` / `title3` | 26 / 21 / 18 | Bold |
| `headline` | 15 | DemiBold |
| `body` | 14 | Regular/Medium |
| `callout` | 13 | — |
| `footnote` | 12 | Açıklamalar |
| `caption1` / `caption2` | 11 / 10 | Rozet, küçük etiket |

## Simgeler — `theme/Icons.qml`

Lucide (ISC) alt kümesi, `resources/fonts/MakineIcons.ttf`. `Icon { glyph: Icons.house }`.
Renkli karodaki beyaz simge dolgulu çizilir: Phosphor "fill" (MIT), aynı kod noktalarında
`resources/fonts/MakineSymbols.ttf` (`Icons.symbolsFamily`, hangi simgelerin dolgulusu var: `Icons.filled`;
`IconTile` kendisi seçer). Karşılığı olmayan simge karoda çizgi hâliyle kalır.
Yeni simge: `scripts/build_ui_fonts.py` içindeki `ICONS` listesine Lucide adını (karoya çıkabilecekse
`FILLED`'a Phosphor adını) ekle, betiği çalıştır, çıktıları (`MakineIcons.ttf`, `MakineSymbols.ttf`,
`Icons.qml`) commit et; betiğin yeniden yazdığı `InterDisplay-*.ttf` yalnız zaman damgası değiştiyse geri al.
Segoe MDL2 kullanılmaz.

Kenar çubuğunun simgeleri `NavIcon`: aynı Lucide çizgileri `Shape` olarak (24 birimlik tuval)
parça parça çizilir, imleç gelince (klavyeyle seçilince de) bir kez oynar — Resend'in
panelindeki gibi: ev zıplar, kapısı bir adım geriden; ızgaranın dört karesi sırayla söner ve
yaylanır; kol sallanır, düğmeleri yanıp söner; soru işareti başını sallar, noktası düşer;
büyük yıldız çeyrek tur döner, küçükler ardından parlar; çark bir diş döner; öndeki kişi
zıplar, arkadaki cevap verir; kalp iki kez atar. Çizimi olmayan (Discord) glif olarak zıplar.
600–800 ms, `OutBack`/`OutBounce`; animasyonlar kapalıyken oynamaz. Yeni simge: Lucide SVG'sinin
yollarını `Part { d: "…" }` olarak ekle, hareketi `Hop`/`Pop`/`Wiggle` ile kur.

## Kaplar ve kenarlar

| Bileşen | Ne zaman |
|---|---|
| `AppBackground` | Pencerenin zemini: iki vurgu ışığı + kenar kararması (GPU degrade, yeniden boyama yok). `Main.qml` her şeyin altına koyar; sayfalar kendi opak zeminini çizmez. |
| `Card` | Kap. İnce çizgi, ışık alan üst kenar (`lit`), istenirse köşede vurgu ışığı (`glow`, `glowCorner`) — sayfanın baştaki kabı ve kenar çubuğu kullanır. |
| `EdgeFade` | Kabın içinde kayan içerik kenara gelince kaybolur: rafın yanları (`leftFade`/`rightFade`), ızgaranın üstü/altı (`topFade`/`bottomFade`); ucuna gelince o kenar 0 olur. İçeriğin kendisi maskelenir (`target`), arkadaki yarı saydam kap ve ışık olduğu gibi görünür: kabın renginde boyanmış bir şerit, ışıklı kapta koyu bir bant gibi duruyordu. |
| `FadeLine` | Uçları kaybolan ayraç: kabın başlığıyla içeriği arası. |
| `TopDissolve` | Kaydırılmış sayfa pencerenin üst şeridinde, altında daha içerik varken panelin ayağında kesilmez, zemine karışır (sayfa `EdgeFade` ile maskelenir; sığan sayfa en üstteyken katman açılmaz). Her kaydırılan sayfa: `TopDissolve { flick: … }`. |

Yapı: sayfa başlığı (`PageHeader`) zeminde; sayfanın her bölümü bir `Card` içinde. Filtre
ve sıralama kabın üst çubuğunda (Tüm Oyunlar); raf başlığı kendi kabında (`Shelf`); ızgara
kabın içinde kayar. Tüm Oyunlar'ın ızgarasında satırlar arasında Makine kesiği durur (uçları sönen ışıklı çizgi, ortasında vurgu parıltısı; tek piksel satırında, adların altındaki boşluğun ortasında); liste görünümü tablo değil: harf (ya da yama günü) bölümleri, her oyun kendi küçük kartında, üzerine gelince kart kalkar ve kapağının renkleri içinden süzülür. Hoş geldin akışı (`OnboardingWizard`) uygulamanın kendisinden yapılır: pencere çerçevesi ve içinde ışıklı sayfa paneli, solda uygulamanın adı kenar çubuğundaki gibi, ortada adımlar kayan vurgu düğmesinde (geçilen adım tikli, tıklanınca geri dönülür), ayakta solda Geri, sağda adımın tek eylemi; adımlar geldiği yönden kayar. Hoş geldin: büyük başlık (ikinci satırı vurgu renginde), cümle, üç güvence tek bir `ListSection`'da, sağda eğik kapak duvarı. Oyunların: tarama bir kapta, sonra oyunlar sırayla yükselir. Tercihler: Ayarlar'ın kendi satırları (`ListRow` + `AppSwitch`), vurgu rengi halkalı ve tikli. Kütüphanem bir oyun rafıdır: her oyun Steam görselinin kendisi olan bir
kart (yoksa kapak bulanık zeminle), başında durumu, ayağında adı, bulunduğu yer ve tek
eylemi; altında eklenmemiş oyun için kesik çizgili bırakma alanı. Nasıl çalışır? göz
gezdirilecek biçimde: başlığın altında bölümlere atlayan haplar, cümle başına tek adım,
ekrandaki her işaret tek satır, sorular üç kümede kapalı, kısayollar tuş olarak.

## Bileşenler — `components/`

| Bileşen | Ne zaman |
|---|---|
| `GlassButton` | Apple'ın sıvı cam geri düğmesi (oyun sayfası): dururken küçük, sessiz, yuvarlak bir damla ve oku; üzerine gelince (ya da odakta) açılıp nereye döneceğini yazar. Altındaki sayfa bulanık görünür (`backdrop`), üst kenarı ışık alır; ok gittiği yöne eğilir, basınca sıkışır. |
| `PagerButton` | Görsel üstünde sayfalayan yuvarlak ok (kahraman şerit, raflar): dumanlı cam, yumuşak gölge, iki çizgiyle çizilen ok; üzerine gelince büyür ve ok gittiği yöne eğilir. |
| `AppButton` | Her düğme. `style`: `filled` (sayfanın tek ana eylemi) · `tinted` · `gray` (gündelik) · `plain` (bağlantı) · `destructive` / `destructiveTinted` · `glass` / `white` (görsel üstünde). `size`: small 28 · regular 34 · large 44. Metinsiz = yuvarlak simge düğmesi. `busy` dönen göstergeyi koyar. |
| `AppSwitch` | Aç/kapa ayarı: açıkken kapsül vurgu renginde, yukarıdan ışıklı ve hafif parlar; topuz yaylanarak geçer (biraz öteye, sonra yerine), basılıyken gideceği yöne uzar, açıkken üstünde küçük bir tik taşır. |
| `MotionIcon` | Kendi hareketi olan simge (`play()`): yenile döner, indir düşer, ok gittiği yöne gider, çark bir diş döner, klasör/zil sallanır, kalp/tik büyür. Düğme imleç gelince, satır üzerine gelince, pencere açılınca başlığındaki karo oynatır; her yeni düğmenin bir simgesi olur. |
| `SegmentedControl` | Filtre, sıralama, birbirini dışlayan birkaç seçenek. Seçili parça vurgu renginde kayan topuz (beyaz yazı), seçili olmayanlar arasında ince çizgi. Yalnız simgeli parça: `{ value, icon, tip }` (sözcük ipucunda) — Tüm Oyunlar başlığındaki filtre ve kapak/liste seçimi. |
| `WhatsNewScreen` zaman çizelgesi | Yenilikler tek bir dallı çizelgedir (her aşama ve her sürüm başlığından açılıp kapanır: sağda madde sayısı ve dönen ok; ilk açılışta yalnız "Üzerinde çalışıyoruz" ile son sürümler açık): üstte kesik çizgide plan (Daha sonra → Sırada → Üzerinde çalışıyoruz; her aşama gövdede kendi simgesini taşır), parlayan nokta kullanılan sürüm, aşağı indikçe eski sürümler; her değişiklik gövdeden türünün renginde bir dalla ayrılır. |
| `MenuButton` | macOS açılır düğmesi: kullanılan seçenek düğmede ("⇅ A–Z ⌄"), altında tikli kısa menü — sıralama. |
| `SearchField` | Arama (kapsül, Esc temizler). Oyun araması kenar çubuğunda tektir |
| `PageHeader` | Sayfanın büyük başlığı, aynı satırda sağda sayfanın denetimleri; altında tam genişlikte ne işe yaradığı |
| `ListSection` + `ListRow` | Ayar ve bilgi listeleri: başlık, tek kartta satırlar, altında açıklama. Satır: simge karosu, başlık, açıklama, sağda denetim; `clickable` ok ekler. |
| `IconTile` | Renkli karo üstünde beyaz, dolgulu simge (System Settings gibi): yukarıdan ışık alan degrade, ince açık kenar; `solid: false` hafif zeminli, çizgi simge. `play()` karoyu hafifçe zıplatır, simgesini oynatır (`ListRow` üzerine gelince çağırır). |
| `StatusBadge` | Durum kapsülü (nokta + sözcük), listelerde. Kapakta yazı ya da işaret yok: `HomeCover` durumu adın önüne dolgulu küçük bir simgeyle koyar (kurulu ✓ yeşil, güncelleme ↑ sarı, yama hazır ↓ vurgu, bilgisayarında ekran vurgu, dış sitede ↗ soluk); sözcüğü üstüne gelince ve ekran okuyucuda. Raftaki tarih adın altında. |
| `Spinner` | Apple'ın sekiz çizgili etkinlik göstergesi |
| `dialogs/BaseDialog` + `SheetHeader` | Pencereler: yükseltilmiş kart, karartılmış sayfa; başlıkta simge, başlık, açıklama, kapat. Altta sağa dayalı `AppButton`'lar. |
| `dialogs/ConfirmDialog` | Apple uyarısı: simge, soru, ne olacağı, "Vazgeç" + eylem |
| `dialogs/AntiCheatWarningDialog` | Riskli eylemin onayı, iki bölme: solda oyunun görseli ve bulunanlar, sağda ton etiketi, risk, tonda kutuda işaretlenecek cümleler. Ana düğme hepsi işaretlenene dek kapalıdır ve sayar ("1 / 3 onaylandı"); odak "Vazgeç"te. |

Tıklanabilir bir satırın/kartın içindeki denetimler `MouseArea` kullanır (`AppButton`,
`AppSwitch`, `SegmentedControl` böyle): basışı alır, alttaki satır aynı tıklamada çalışmaz.
İç içe iki `TapHandler` ikisini birden tetikler.

## Ölçüler

Kenar boşluğu `pageMargin` 32 · köşeler: denetim 8, kart 14, liste 12, sayfa penceresi 18,
büyük görsel 20 · kapak köşesi genişliğin %11,8'i (CDN'den önceden yuvarlatılmış gelir;
maske gerekmez) · sayfa genişliği: listeler `contentMaxWidth` 1180, oyun sayfası 1240.

## Hareket

Sayfa kendini yukarıdan aşağı kurar: her `Card` görünür olduğunda hafif bir solma ve
yükselmeyle yerine oturur (aşağıdaki biraz sonra; `reveal: false` durdurur). Kenar
çubuğunda seçim tek bir vurgu hapıdır, seçilen yere kayar. Düğmeye basınca hafifçe çöker ve
yaylanarak döner. Kapak görseli geldiğinde yer tutucunun üstünde belirir. Topluluk
paylaşımları ve Yenilikler'deki zaman çizelgesi düğümleri sırayla yükselerek gelir; beğenilen
kalp bir an büyür.

Açılış ekranı (`main.cpp` `SplashWindow`) launcher'ın kendi görünümünde: ayarlarından okunan
koyu ya da açık mod ve kullanıcının vurgu rengi; sayfanın gecesi, `AppBackground`'un iki vurgu
ışığı aynı yerlerde, kenarı biraz koyu, ince çizgi ve üst/alt kenar ışığı. Ortada logo (renkleri
akar, `logo_anim.webp`) vurgu renginde yumuşak bir ışıkta, altında ad sayfa başlığı gibi; ayakta
vurgu renginde ince bir ilerleme çizgisi, solunda açılış adımı (yukarı kayarak değişir), sağında
sürüm. Katmanlı pencere (gerçek yuvarlak köşe, gölge), kendi iş parçacığında ~60 fps; kapanınca
çizgi dolar ve açılmış pencerenin üstünde solar (`close()` beklemez), en az 1,4 sn görünür.

Kısa ve sakin: 150–300 ms, `Easing.OutCubic`; açılan pencere küçük bir yaylanmayla
(`OutBack`, 1.2). Sürekli dönen/atan animasyon yok (yalnız `Spinner`, o da görünürken).
`SettingsManager.enableAnimations` kapalıyken `Dimensions.anim*` sıfırdır.

## Tuzaklar

- Topluluğa launcher'dan yazmak sitenin üye girişine dayanır (`CommunityService`). E-posta,
  sitedeki gibi: adres → 6 haneli kod (`/api/uye/launcher/eposta`, `…/eposta/dogrula`);
  Turnstile tarayıcı ister, launcher'ın ucu onun yerine IP başına saatte 5 kodla sınırlı.
  İlk kez giren e-posta üyesi önce adını seçer (`/api/uye/launcher/profil`). Discord ise
  cihaz kodu: `/api/uye/launcher/baslat` → tarayıcıda `/topluluk/baglan` →
  `/api/uye/launcher/durum`. Hepsinin sonu bir taşıyıcı jeton (`Authorization: Bearer`,
  sitede yalnız SHA-256'sı); DPAPI ile şifreli tutulur (`community/token`). Rotalar
  Makine-WEB'de (`src/community/members.ts`, migration `0012_launcher_login`).
- Discord gibi Lucide'de olmayan marka işaretleri `build_ui_fonts.py` `EXTRA` ile özel kod
  noktalarında yalnız `MakineSymbols.ttf`'tedir; `Icon` onları kendisi o fonttan çizer
  (`Icons.symbolsOnly`).

- Kök pencere (`Main.qml`) C++'ın: `JavaScriptOwnership` onu çöp toplayıcıya bırakıyordu, bir
  toplama açılıştan saniyeler sonra pencereyi yok etti, süreç penceresiz sürdü (2026-10-26).
  `main.cpp` `CppOwnership` verir, olay döngüsünden sonra siler.
- `stage` üstündeki öğeler (`parent: stage`) birbirine **anchor ile bağlanmaz** (anchor,
  `parent` yerleşmeden pencereye göre çözülür) ve sıralarını **z** belirler, yazılış sırası değil.
- Sayfanın üst 44 px'ini pencereyi sürükleyen şerit kaplar; altındaki denetim tıklanmaz. Şeride
  denk gelen denetim için şerit kısalır (oyun sayfasının geri düğmesi: `dragStrip.x`).

- Bir layout'un genişliğini kendi `width`'inden türetme (`Math.min(width - 64, …)`): layout'un
  genişliği çocuklarının en büyüğünü izler, değer her geçişte küçülür ve sayfa donar
  (`HeroSection` 2026-10-22). Sayfanın/ebeveynin genişliğinden hesapla.
- Bir öğenin `visible`'ı ebeveyninkini izler: `visible: child.visible || …` kendini hiç
  açamaz. Koşulu ayrı bir `bool` özelliğe koy.
- Hook betiği `id:` sayarken `_gid: x` gibi adları da `id: x` sanar; özellik adını değiştir.
- `MultiEffect` maskesi bir eşiktir, çarpım değil: `maskThresholdMin: 0` ile her sıfır olmayan
  alfa tam opak olur. Yumuşak geçiş için `maskThresholdMin: 0.5` + `maskSpreadAtMin: 1.0`
  (`TopDissolve`). Degradeler `RadialGradient` (QtQuick.Shapes) içinde `parent` göremez: id kullan.
- `lucide.ttf` her simgenin sol kenarını 0 yazar, çizimi ise 24'lük ızgaradaki yerinde durur;
  fontTools kaydederken sınır kutusunu düzeltir ama yan boşluğu 0 bırakır ve her simge sola kayar
  (ok genişliğinin üçte biri kadar). `build_ui_fonts.py` yan boşluğu çizimin sol kenarına eşitler —
  fontu elle alt kümelersen aynısını yap.
- `Repeater.itemAt()` bir şey bildirmez: ona bağlı bağlama bir kez, öğeler yokken hesaplanır ve
  `null` kalır. `onItemAdded`'da artan bir sayacı bağlamaya kat (`SegmentedControl` `_made`).
