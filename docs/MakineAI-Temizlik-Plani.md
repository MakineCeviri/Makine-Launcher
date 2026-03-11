# MakineAI — Submodule Temizlik & Commit Planı
**Tarih:** 2026-03-05
**Durum:** 49 dosya staged, ~20K satır, henüz commit edilmedi
**Son commit:** 7bfbc89 feat(ui): update app icon and in-app logo with new branding

---

## Ne Oldu?
`common/` (makine-common) git submodule kaldırıldı ve içeriği `core/` altına inline edildi.
Ama bazı dosyalar kullanılmıyor, bazıları deferred feature stub'ı, CMakeLists.txt formatlama bozuk.

---

## Silinecek Dosyalar (7 adet)

### Hiç kullanılmayan (src ve test'lerde 0 referans):
- `core/include/formats/bethesda_ba2.hpp` (211 satır)
- `core/include/formats/unreal_pak.hpp` (172 satır)
- `core/include/makineai/debug.hpp` (7 satır, sadece forward)
- `core/include/makineai/pimpl.hpp` (322 satır)

### Deferred feature stub'ları (TM, Glossary, Pipeline — ertelenmiş):
- `core/include/makineai/types/tm_types.hpp` (62 satır)
- `core/include/makineai/types/pipeline_types.hpp` (110 satır)
- `core/include/makineai/types/translation_types.hpp` (295 satır)

---

## Düzeltilecek Dosyalar (2 adet)

### core/CMakeLists.txt
Tek satıra sıkışmış bloklar var, düzgün formatlanacak:
- `find_package(mio ...)` ve diğer 6 optional package — her biri ayrı satır
- `if(TARGET mio::mio)` ve diğer 7 optional library bloğu — her biri ayrı if/endif

### core/include/makineai/types.hpp
Deferred include'lar kaldırılacak:
```cpp
// KALDIRILACAK:
#include "makineai/types/translation_types.hpp"
#include "makineai/types/tm_types.hpp"
#include "makineai/types/pipeline_types.hpp"
```

---

## Kalacak Dosyalar (42 adet)

### Submodule kaldırma (2):
- D .gitmodules
- D common

### CMakeLists.txt (1, düzeltilmiş):
- M core/CMakeLists.txt

### src/ tarafından aktif kullanılan header'lar:
| Header | src/ | tests/ |
|--------|:---:|:---:|
| audit.hpp | 4 | 1 |
| config.hpp | 1 | 1 |
| crypto_utils.hpp | 1 | 1 |
| database.hpp | 1 | 2 |
| error.hpp | 2 | 1 |
| features.hpp | 2 | 1 |
| health.hpp | 1 | 1 |
| json_utils.hpp | 1 | 3 |
| logging.hpp | 14 | 0 |
| metrics.hpp | 8 | 1 |
| mio_utils.hpp | 1 | 0 |
| parallel.hpp | 1 | 1 |
| validation.hpp | 2 | 1 |
| fwd.hpp (M) | - | - |

### Sadece testler tarafından kullanılan (testler zaten committed):
| Header | tests/ |
|--------|:---:|
| archive_utils.hpp | 1 |
| async.hpp | 1 |
| cache.hpp | 1 |
| concurrent_queue.hpp | 1 |
| constants.hpp | 1 |
| lazy.hpp | 1 |
| memory.hpp | 1 |
| path_utils.hpp | 2 |
| sandbox.hpp | 1 |
| sqlite_utils.hpp | 1 |
| string_pool.hpp | 1 |
| utf_utils.hpp | 1 |
| version.hpp | 1 |

### Detail header'lar (diğerleri tarafından include ediliyor):
- detail/debug.hpp
- detail/mio_utils.hpp
- detail/string_pool.hpp

### Format header'lar (test/referans var):
- formats/gamemaker_data.hpp
- formats/renpy_rpa.hpp (test_rpa_parser.cpp)
- formats/unity_bundle.hpp

### Type header'lar (kalacak):
- types.hpp (aggregator)
- types/common.hpp
- types/game_types.hpp
- types/patch_types.hpp

### Kaynak dosyalar:
- src/config/config.cpp (561 satır)
- src/database/database.cpp (2950 satır)

---

## Commit Planı

### Commit 1: `refactor(core): remove makine-common submodule and inline dependencies`
- .gitmodules silme
- common/ silme
- Tüm kullanılan header'lar + config.cpp + database.cpp ekleme
- CMakeLists.txt güncelleme (düzeltilmiş format)
- types.hpp'den deferred include'ları kaldırma

### Commit 2 (opsiyonel): `chore(core): remove unused headers from former submodule`
- 7 dead/deferred dosyayı silme
- (Veya Commit 1 ile birleştir — temiz tek commit)

---

## Komutlar (sırayla çalıştır)

```bash
cd /c/cedra/MakineAI

# 1. Dead dosyaları unstage et ve sil
git reset HEAD core/include/formats/bethesda_ba2.hpp
git reset HEAD core/include/formats/unreal_pak.hpp
git reset HEAD core/include/makineai/debug.hpp
git reset HEAD core/include/makineai/pimpl.hpp
git reset HEAD core/include/makineai/types/tm_types.hpp
git reset HEAD core/include/makineai/types/pipeline_types.hpp
git reset HEAD core/include/makineai/types/translation_types.hpp
rm core/include/formats/bethesda_ba2.hpp
rm core/include/formats/unreal_pak.hpp
rm core/include/makineai/debug.hpp
rm core/include/makineai/pimpl.hpp
rm core/include/makineai/types/tm_types.hpp
rm core/include/makineai/types/pipeline_types.hpp
rm core/include/makineai/types/translation_types.hpp

# 2. types.hpp'den deferred include'ları kaldır (manual edit)

# 3. CMakeLists.txt'yi formatla (manual edit)

# 4. Commit
git commit -m "refactor(core): remove makine-common submodule and inline dependencies"
```

---

## Notlar
- Test dosyaları ZATEN committed (ed720be) — dokunma
- Format header'ların 3'ü (gamemaker, renpy, unity) birbirini referans ediyor — koru
- database.cpp 2950 satır — en büyük tek dosya, aktif kullanılıyor
- Deferred feature'lar (TM, Glossary, QA, Pipeline) ileride gerekirse sıfırdan yazılacak
