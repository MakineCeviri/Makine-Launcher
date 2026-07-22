# Kurulum Akışı — Dosyalar Nereye Gider, Nerede Bozulur

> **Araştırma tarihi:** 2026-07-22
> **Kapsam:** indirme · çıkarma · yedekleme · kurulum · kaldırma · antivirüs · izinler

---

## 1. Dosya Akışı — uçtan uca

```
CDN (R2)
  └─ https://cdn.makineceviri.org/data/{appId}.makine        (AES-256-GCM şifreli)
       │
       ▼  indirme
  %TEMP%\makine\downloads\{appId}.makine.part               ← yarım indirme
       │  (tamamlanınca yeniden adlandırma)
  %TEMP%\makine\downloads\{appId}.makine
       │  (GCM auth tag doğrular → zstd → tar)
       ▼  çıkarma
  %LOCALAPPDATA%\MakineCeviri\Makine-Launcher\packages\{dirName}\...
       │
       ├──▶ YEDEK: ...\Makine-Launcher\backups\{gameId}\{backupId}\
       │      (yalnızca üzerine yazılacak dosyalar)
       │
       ▼  kurulum (tipe göre hedef değişir)
  Oyun klasörü / kullanıcı klasörü / yazı tipi klasörü
```

### Kurulum hedefleri — tipe göre

| Tip | Hedef | Not |
|---|---|---|
| `<boş>`, `direct`, `overlay`, `copy`, `file-replace` | **Oyun kökü** | Yapı koruyan kopyalama; sarmalayıcı klasör sıyrılır |
| `script`, `copyDir`, `copyFile` | Adım tarifine göre (oyun kökü göreli) | `steps` dizisi yürütülür |
| `userPath` | `%USERPROFILE%\<target>` | Ör. Sims 4 → `Documents\Electronic Arts\The Sims 4\Mods` |
| `_font:` girdisi | `%LOCALAPPDATA%\Microsoft\Windows\Fonts` | Kullanıcı bazlı yazı tipi, yönetici gerekmez |
| `_desktop:` girdisi | Masaüstü | Kısayol/okuma dosyası |
| `_steamlang:` girdisi | `appmanifest_{appId}.acf` | Steam dil ayarı |
| `paradox-mod` | `Belgeler\Paradox Interactive\<oyun>\mod` | **Handler yok** — kullanıcı elle kopyalar |

### Kalıcı konumlar

| Ne | Yol |
|---|---|
| Uygulama verisi | `%LOCALAPPDATA%\MakineCeviri\Makine-Launcher\` |
| Yedekler | `...\backups\` |
| Çıkarılmış paketler | `...\packages\` |
| Önbellek (katalog, görsel) | `...\cache\` |
| Kurulum durumu | `...\data\installed_packages.json` |
| Günlükler | `...\logs\` |
| Sentry kuyruğu | `...\sentry-db\` |
| Geçici indirme | `%TEMP%\makine\downloads\` (7 günden eski dosyalar otomatik silinir) |

---

## 2. Doğru Çalışan Korumalar

Araştırmada halihazırda sağlam bulunan noktalar — bunlara dokunulmamalı:

| Koruma | Nerede |
|---|---|
| **Uzun yol desteği** (`longPathAware: true`) | `qml/resources/app.manifest` — 260 karakter sınırı sorun değil |
| **Unicode yollar** | `fromLocal8Bit` yalnızca env/process çıktısında; dosya yollarında UTF-16 kullanılıyor. Türkçe karakterli kullanıcı adları sorun çıkarmaz |
| **Bütünlük** | AES-256-GCM auth tag; bozuk/eksik inen paket şifre çözmede başarısız olur |
| **Kısmi yedek reddi** | Yedek yarıda kalırsa kurulum tamamen iptal edilir |
| **Oyun çalışıyor kontrolü** | Kurulumdan önce `tasklist` ile exe taranır |
| **Disk alanı ön kontrolü** | İndirme öncesi paket boyutuna göre ölçeklenir |
| **Kilitli dosya yeniden deneme** | `CopyError::FileLocked` → gecikmeli tekrar |
| **Hata sınıflandırma** | `PermissionDenied` / `DiskFull` / `FileLocked` ayrı ayrı ele alınır |
| **Geçici dosya temizliği** | 7 günden eski `.part` ve `.makine` dosyaları başlangıçta silinir |

---

## 3. Antivirüs — en sinsi başarısızlık

### Sorun

`QFile::copy` başarı döndürmesi yalnızca **yazmanın kabul edildiği** anlamına gelir.
Gerçek zamanlı antivirüs, kopyalamadan milisaniyeler sonra dosyayı tarar ve oyun
yamalarının tipik içeriğini karantinaya alır:

- `dinput8.dll` (proxy DLL — ME Andromeda, RDR2)
- `*.asi` eklentileri (`rdr2-translator.asi`, `fontfix.asi`)
- `ScriptHookRDR2.dll`

Sonuç: kopyalama başarılı, dosya birkaç milisaniye sonra yok, kurulum **"başarılı"**
diyor, oyun açılmıyor. Kullanıcı açısından bu, bozuk yamadan ayırt edilemez —
*"yamayı yükledim, Steam çalışıyor gösteriyor ama hiçbir tepki yok"*.

### Çözüm (2026-07-22'de eklendi)

Kurulum ve güncelleme yollarının ikisinde de, kopyalama bittikten sonra yazılan her
dosyanın diskte **var olduğu** doğrulanıyor (`missingAfterInstall`). Eksik varsa
kurulum başarısız sayılıyor ve kullanıcıya karantina odaklı, adım adım yönerge
gösteriliyor (Windows Güvenliği → Dışlamalar → Koruma geçmişinden geri yükleme).

Kayıp dosyalar yine de `installedFiles`'a yazılıyor; böylece sonraki kaldırma işlemi
kalanları temizleyebiliyor.

### Geliştirme ortamı uyarısı

> Bu makinede Windows Defender **kapalı** (`RealTimeProtectionEnabled: False`).
> Antivirüs kaynaklı hataların geliştirmede hiç görünmemesinin sebebi budur.
> Yama kurulumuna dokunan değişiklikler, Defender'ın **açık** olduğu bir makinede
> de denenmelidir.

---

## 4. İzinler — Program Files sorunu

Launcher bilinçli olarak `asInvoker` çalışır (`app.manifest`): yükseltme istemek
antivirüs yanlış pozitiflerini artırır. Bunun bedeli, `C:\Program Files` altındaki
oyunlara yazamamasıdır — Epic Games varsayılan olarak oraya kurar
(*"C:\Program Files\Epic Games\TWDTTDS"*).

**Eklenen ön kontrol:** Kurulum başlamadan önce oyun klasörüne gerçekten dosya
yazılabildiği sınanıyor (`isGameDirWritable`). Yazılamıyorsa kullanıcı, indirme ve
çıkarma için beklemeden önce net bir yönerge alıyor.

Sınama, `QFileInfo::isWritable()` yerine **gerçekten dosya oluşturarak** yapılıyor:
Windows'ta o bayrak salt-okunur özniteliğini yansıtır, etkin ACL'yi değil — yani
Program Files "yazılabilir" görünüp yazma anında başarısız olabilir.

---

## 5. Açık Riskler

| Risk | Etki | Durum |
|---|---|---|
| **Paket önbelleği sınırsız büyür** | Çıkarılmış paketler `...\packages\` altında kalıyor; boyut sınırı yok. Ayarlardaki "önbellek temizle" yalnızca **görsel** önbelleği siliyor. Çok yama kuran kullanıcıda GB'larca yer | 🔴 Açık |
| **Oyun güncellemesi yamayı bozar** | ScriptHook/ASI yükleyiciler oyun sürümüne bağlı; Steam güncellemesi sonrası oyun açılmaz. Pakette hedef sürüm bilgisi yok | 🔴 Açık (şema işi) |
| **Steam dosya doğrulaması yamayı siler** | Kullanıcı "bütünlüğü doğrula" yaparsa yama dosyaları kaldırılır — beklenen davranış ama kurulum durumu güncellenmez | 🟠 İzlenmeli |
| **`paradox-mod` handler'ı yok** | Stellaris elle kurulum gerektiriyor (mesaja klasör yolu eklendi) | 🟠 Kısmi |

---

## 6. Kurulum Öncesi Kontrol Listesi (mevcut sıra)

1. Oyun çalışıyor mu? → `tasklist` taraması
2. **Oyun klasörü yazılabilir mi?** → gerçek yazma sınaması *(yeni)*
3. Paket indirilmiş mi? → yoksa indir (boş dosya koruması *(yeni)*)
4. Üzerine yazılacak dosya var mı? → yedek al (sarmalayıcı hizalı *(düzeltildi)*)
5. Yedek tam mı? → değilse kurulumu iptal et
6. Kurulum tipi destekleniyor mu? → değilse dürüstlük kapısı
7. Dosyaları kopyala
8. **Yazılan dosyalar diskte mi?** → değilse karantina uyarısı *(yeni)*
9. Kurulum durumunu kaydet

## 7. Kaldırma Sırası

1. Yedek var mı?
   - **Var** → geri yükle, sonra eklenen dosyaları sil
   - **Yok + orijinal dosya değiştirilmiş** → **kaldırmayı reddet**, mağaza doğrulaması öner *(düzeltildi)*
   - **Yok + yalnızca dosya eklenmiş** → güvenle sil
2. Geri yükleme başlatılamazsa → kaldırma yapılmaz *(düzeltildi)*
3. Her başarısızlık Sentry'ye `uninstall` etiketiyle raporlanır *(yeni)*
