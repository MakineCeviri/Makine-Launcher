# Tasarım sistemi — Apple tarzı (2026-10-22)

Launcher'ın arayüzü Apple'ın insan arayüzü yönergelerine (macOS / App Store / Apple TV)
göre kuruludur. Yeni bir ekran ya da bileşen bu belgedeki parçalarla yapılır; yeni renk,
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

Apple sistem paleti, koyu ve açık görünüm. Ad → kullanım:

| Token | Kullanım |
|---|---|
| `bgPrimary` | Sayfa zemini (#141416 / #F5F5F7) |
| `surface` | Kart, gruplu liste (koyu: ince çizgili gri; açık: beyaz + yumuşak gölge — `Card.qml`) |
| `surfaceElevated` | Pencere (sheet), ipucu, bildirim |
| `fill`, `fillSecondary`, `fillTertiary`, `fillQuaternary` | Denetimlerin zemini (gri düğme, arama alanı, segment izi, üzerine gelme) |
| `separator` | Satırlar arası ince çizgi |
| `textPrimary` › `textSecondary` › `textMuted` › `textTertiary` › `textDisabled` | Etiket kademeleri: başlık › metin › açıklama › yer tutucu › pasif |
| `primary` | Kullanıcının seçtiği vurgu (varsayılan mavi #0A84FF / #007AFF). Ana düğme, bağlantı, seçim, ilerleme. |
| `success`, `warning`, `error` | Durum. Yalnız renge güvenilmez; yanında her zaman sözcük ya da simge olur. |
| `glassOnArt` | Kapak/görsel üstündeki koyu cam (düğme, rozet) |

Vurgu ön ayarları (`SettingsManager.accentPresets()`, `Theme._accentPresets`): Apple'ın
sistem renkleri, her biri koyu ve açık tonuyla. Eski varsayılan mor, `appearance/designVersion`
2'ye geçerken bir kez maviye taşınır; sonra seçilen kalır.

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

## Bileşenler — `components/`

| Bileşen | Ne zaman |
|---|---|
| `AppButton` | Her düğme. `style`: `filled` (sayfanın tek ana eylemi) · `tinted` · `gray` (gündelik) · `plain` (bağlantı) · `destructive` / `destructiveTinted` · `glass` / `white` (görsel üstünde). `size`: small 28 · regular 34 · large 44. Metinsiz = yuvarlak simge düğmesi. `busy` dönen göstergeyi koyar. |
| `AppSwitch` | Aç/kapa ayarı (vurgu rengi) |
| `SegmentedControl` | Filtre, sıralama, birbirini dışlayan birkaç seçenek |
| `SearchField` | Arama (kapsül, Esc temizler) |
| `PageHeader` | Sayfanın büyük başlığı + ne işe yaradığı + sağda sayfanın denetimleri |
| `ListSection` + `ListRow` | Ayar ve bilgi listeleri: başlık, tek kartta satırlar, altında açıklama. Satır: simge karosu, başlık, açıklama, sağda denetim; `clickable` ok ekler. |
| `Card` | Serbest içerik kartı |
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
