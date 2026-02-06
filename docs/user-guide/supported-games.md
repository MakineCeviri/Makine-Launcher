# Desteklenen Oyunlar (Mod 1)

Bu sayfada MakineAI'nin **direkt dil dosyasi yukleme** ozelligi ile tam destek verdigi oyun motorlari listelenmistir.

## Nasil Calisir?

Desteklenen oyunlarda MakineAI:

1. Oyunu otomatik algilar
2. Motor turunu tespit eder
3. Dil dosyasini direkt yukler
4. Oyun dosyalarini **degistirmez**
5. Aninda geri alma saglar

Bu yontem en guvenli ve hizli ceviri metodudur.

---

## Desteklenen Motorlar

### Unity (Mono + IL2CPP)

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- BepInEx/XUnity.AutoTranslator entegrasyonu
- Metin hooking ile canli ceviri
- Font degisikligi gerektirmez

**Desteklenen Surum:** Unity 2018+

**Ornek Oyunlar:**
- Hollow Knight
- Cuphead
- Genshin Impact (PC)
- Valheim

**Teknik Detaylar:**
- Mono build: IL kodunu runtime'da yakalar
- IL2CPP build: Native hook kullanir
- TextMeshPro ve Legacy Text destegi

---

### Unreal Engine

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- .pak dosyalarina lokalizasyon ekleme
- Orijinal .pak dosyalari korunur
- Oncelik sistemi ile ustune yazma

**Desteklenen Surum:** Unreal Engine 4.x, 5.x

**Ornek Oyunlar:**
- Fortnite
- PUBG
- Dead by Daylight
- Final Fantasy VII Remake

**Teknik Detaylar:**
- Lokalizasyon .pak dosyasi olusturur
- Engine oncelik sistemi kullanilir
- Orijinal dosyalar dokunulmaz

---

### RPG Maker

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- JSON/Ruby dil dosyasi yukleme
- System.json ve Actors.json ceviri
- Plugin bazli lokalizasyon

**Desteklenen Surum:**
- RPG Maker MV (JavaScript/JSON)
- RPG Maker MZ (JavaScript/JSON)
- RPG Maker VX Ace (Ruby/RGSS3)

**Ornek Oyunlar:**
- Omori
- To the Moon
- OneShot
- Corpse Party

**Teknik Detaylar:**
- MV/MZ: www/data/ altindaki JSON dosyalari
- VX Ace: Data/ altindaki .rvdata2 dosyalari
- Karakter encoding: UTF-8

---

### Ren'Py

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- tl/ klasorune dil dosyasi yukleme
- Ren'Py native lokalizasyon sistemi
- Oyun icinden dil degistirme

**Desteklenen Surum:** Ren'Py 7.x, 8.x

**Ornek Oyunlar:**
- Doki Doki Literature Club
- Katawa Shoujo
- Long Live the Queen

**Teknik Detaylar:**
- game/tl/turkish/ klasoru olusturulur
- .rpy dosyalari UTF-8 encoding
- Translate bloklari kullanilir

---

### GameMaker

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- data.win string tablosu duzenleme
- JSON lokalizasyon dosyasi
- GML script injection

**Desteklenen Surum:** GameMaker Studio 2

**Ornek Oyunlar:**
- Undertale
- Deltarune
- Hotline Miami
- Hyper Light Drifter

**Teknik Detaylar:**
- data.win icindeki STRG chunk
- String index tablosu
- UTF-8 karakter destegi

---

### Bethesda (Creation Engine)

**Destek Durumu:** Tam Destek

**Nasil Calisir:**
- .ba2 arsiv dosyalarina lokalizasyon
- ESP/ESM plugin destegi
- STRINGS dosyalari

**Desteklenen Surum:** Creation Engine (Skyrim, Fallout)

**Ornek Oyunlar:**
- Skyrim (SE/AE)
- Fallout 4
- Starfield

**Teknik Detaylar:**
- Data/Strings/ klasorune .STRINGS/.ILSTRINGS/.DLSTRINGS
- BA2 arsivine paketleme
- Load order onceligi

---

## Planlanan Motorlar

| Motor | Durum | Beklenen Tarih |
|-------|-------|----------------|
| Godot 3.x/4.x | Planlanan | Q2 2026 |
| Source Engine | Arastirma | TBD |
| CryEngine | Arastirma | TBD |

---

## Oyun Ekleme Talepleri

Desteklenmesini istediginiz bir oyun motoru var mi?

1. [GitHub Issues](https://github.com/jlceaser/MakineAI/issues) uzerinden talep acin
2. Oyun adi ve motor bilgisini ekleyin
3. Topluluk oylariyla onceliklendirme yapilir

---

## Sonraki Adimlar

- [Desteklenmeyen oyunlar icin](other-games.md)
- [Sorun giderme](troubleshooting.md)
- [Ana sayfaya don](getting-started.md)
