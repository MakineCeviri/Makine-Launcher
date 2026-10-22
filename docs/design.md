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
| Kenar çubuğu | `components/Sidebar.qml` | macOS kaynak listesi: Ana Sayfa · Tüm Oyunlar · Kütüphanem (bekleyen yama sayısı) · Yardım · Topluluk. Ayak: `SidebarStatus.qml` (launcher güncellemesi, tarama). |
| Pencere düğmeleri | `components/WindowControls.qml` | Sağ üstte yüzer; başlık çubuğu yok. Sayfanın üst 44 px'i pencereyi sürükler (`Main.qml` `dragStrip`), sayfalar bu şeridi boş bırakır (başlık `topMargin: 52`). |
| Uygulama uyarıları | `components/AppNotice.qml` | Bağlantı yok, bütünlük, yarım kalan işlem: sayfanın üstünde kapsül. |
| Bildirim | `components/GameToast.qml` | macOS bildirimi gibi sağ üstten kayar, 8 sn (imleç üstündeyken durur). |

## Renkler — `theme/Theme.qml`

Koyu: neredeyse siyah zemin (#07070A), vurgunun iki yumuşak ışığı (`AppBackground`),
yarı saydam kaplar, sıcak kırık beyaz yazı. Açık görünüm Apple'ın gri-beyazı. Ad → kullanım:

| Token | Kullanım |
|---|---|
| `bgPrimary` | Pencere zemini (#07070A / #F4F4F6); üstünü `AppBackground` aydınlatır |
| `container` | Her kap: kenar çubuğu, bölüm, liste, kart (`Card.qml`; koyu: %82 opak gece) |
| `containerBorder`, `containerHighlight` | Kabın ince çizgisi ve ışık alan üst kenarı |
| `containerSolid` | Kayan içeriğin kabın kenarında kaybolduğu renk (`EdgeFade`) |
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

Inter (SF Pro yerine), 22 px ve üstü başlıklarda **Inter Display** (`Dimensions.fontDisplay`).

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
Yeni simge: `scripts/build_ui_fonts.py` içindeki `ICONS` listesine Lucide adını ekle, betiği
çalıştır, iki çıktıyı (`MakineIcons.ttf`, `Icons.qml`) commit et. Segoe MDL2 kullanılmaz.

## Kaplar ve kenarlar

| Bileşen | Ne zaman |
|---|---|
| `AppBackground` | Pencerenin zemini: iki vurgu ışığı + kenar kararması (GPU degrade, yeniden boyama yok). `Main.qml` her şeyin altına koyar; sayfalar kendi opak zeminini çizmez. |
| `Card` | Kap. İnce çizgi, ışık alan üst kenar (`lit`), istenirse köşede vurgu ışığı (`glow`, `glowCorner`) — sayfanın baştaki kabı ve kenar çubuğu kullanır. |
| `EdgeFade` | Kabın içinde kayan içerik kenara gelince kaybolur: rafın yanları (`left`/`right`), ızgaranın üstü/altı (`top`/`bottom`). `active` ucuna gelince kapanır. |
| `FadeLine` | Uçları kaybolan ayraç: kabın başlığıyla içeriği arası. |
| `TopDissolve` | Kaydırılmış sayfa pencerenin üst şeridinde kesilmez, zemine karışır (sayfa maskelenir; yalnız kaydırılmışken katman açılır). Her kaydırılan sayfa: `TopDissolve { flick: … }`. |

Yapı: sayfa başlığı (`PageHeader`) zeminde; sayfanın her bölümü bir `Card` içinde. Filtre
ve sıralama kabın üst çubuğunda (Tüm Oyunlar, Kütüphanem); raf başlığı kendi kabında
(`Shelf`); ızgara kabın içinde kayar.

## Bileşenler — `components/`

| Bileşen | Ne zaman |
|---|---|
| `AppButton` | Her düğme. `style`: `filled` (sayfanın tek ana eylemi) · `tinted` · `gray` (gündelik) · `plain` (bağlantı) · `destructive` / `destructiveTinted` · `glass` / `white` (görsel üstünde). `size`: small 28 · regular 34 · large 44. Metinsiz = yuvarlak simge düğmesi. `busy` dönen göstergeyi koyar. |
| `AppSwitch` | Aç/kapa ayarı (vurgu rengi) |
| `SegmentedControl` | Filtre, sıralama, birbirini dışlayan birkaç seçenek |
| `SearchField` | Arama (kapsül, Esc temizler) |
| `PageHeader` | Sayfanın büyük başlığı, aynı satırda sağda sayfanın denetimleri; altında tam genişlikte ne işe yaradığı |
| `ListSection` + `ListRow` | Ayar ve bilgi listeleri: başlık, tek kartta satırlar, altında açıklama. Satır: simge karosu, başlık, açıklama, sağda denetim; `clickable` ok ekler. |
| `IconTile` | Renkli karo üstünde beyaz simge (System Settings gibi); `solid: false` hafif zeminli |
| `StatusBadge` | Durum kapsülü (nokta + sözcük); `onArt` kapak üstünde |
| `Spinner` | Apple'ın sekiz çizgili etkinlik göstergesi |
| `dialogs/BaseDialog` + `SheetHeader` | Pencereler: yükseltilmiş kart, karartılmış sayfa; başlıkta simge, başlık, açıklama, kapat. Altta sağa dayalı `AppButton`'lar. |
| `dialogs/ConfirmDialog` | Apple uyarısı: simge, soru, ne olacağı, "Vazgeç" + eylem |

Tıklanabilir bir satırın/kartın içindeki denetimler `MouseArea` kullanır (`AppButton`,
`AppSwitch`, `SegmentedControl` böyle): basışı alır, alttaki satır aynı tıklamada çalışmaz.
İç içe iki `TapHandler` ikisini birden tetikler.

## Ölçüler

Kenar boşluğu `pageMargin` 32 · köşeler: denetim 8, kart 14, liste 12, sayfa penceresi 18,
büyük görsel 20 · kapak köşesi genişliğin %11,8'i (CDN'den önceden yuvarlatılmış gelir;
maske gerekmez) · sayfa genişliği: listeler `contentMaxWidth` 1180, oyun sayfası 1240.

## Hareket

Kısa ve sakin: 150–300 ms, `Easing.OutCubic`; açılan pencere küçük bir yaylanmayla
(`OutBack`, 1.2). Sürekli dönen/atan animasyon yok (yalnız `Spinner`, o da görünürken).
`SettingsManager.enableAnimations` kapalıyken `Dimensions.anim*` sıfırdır.

## Tuzaklar

- Bir layout'un genişliğini kendi `width`'inden türetme (`Math.min(width - 64, …)`): layout'un
  genişliği çocuklarının en büyüğünü izler, değer her geçişte küçülür ve sayfa donar
  (`HeroSection` 2026-10-22). Sayfanın/ebeveynin genişliğinden hesapla.
- Bir öğenin `visible`'ı ebeveyninkini izler: `visible: child.visible || …` kendini hiç
  açamaz. Koşulu ayrı bir `bool` özelliğe koy.
- Hook betiği `id:` sayarken `_gid: x` gibi adları da `id: x` sanar; özellik adını değiştir.
- `MultiEffect` maskesi bir eşiktir, çarpım değil: `maskThresholdMin: 0` ile her sıfır olmayan
  alfa tam opak olur. Yumuşak geçiş için `maskThresholdMin: 0.5` + `maskSpreadAtMin: 1.0`
  (`TopDissolve`). Degradeler `RadialGradient` (QtQuick.Shapes) içinde `parent` göremez: id kullan.
