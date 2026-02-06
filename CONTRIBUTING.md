# Katkıda Bulunma Rehberi

MakineAI'ya katkıda bulunmak istediğiniz için teşekkürler! Bu rehber, geliştirme sürecini ve katkı kurallarını açıklar.

---

## Başlamadan Önce

1. Projeyi fork'layın
2. Yeni bir branch oluşturun (`git checkout -b feature/ozellik-adi`)
3. Değişikliklerinizi yapın
4. Commit'leyin (aşağıdaki convention'a uygun)
5. Push'layın (`git push origin feature/ozellik-adi`)
6. Pull Request açın

---

## Geliştirme Ortamı

### Gereksinimler

- Windows 10/11 (64-bit)
- Qt 6.10+ (MinGW 13.1.0 veya MSVC 2022)
- CMake 3.25+
- vcpkg
- [just](https://github.com/casey/just) task runner
- Git

### Kurulum

```bash
git clone https://github.com/<kullanici-adiniz>/MakineAI.git
cd MakineAI

# Bağımlılıkları kur
just setup

# Build & çalıştır
just run

# Testleri çalıştır
just test
```

---

## Commit Convention

[Conventional Commits](https://www.conventionalcommits.org/) standardını kullanıyoruz.

### Format

```
<tip>(<kapsam>): <açıklama>

[isteğe bağlı gövde]

[isteğe bağlı footer]
```

### Tipler

| Tip | Açıklama |
|-----|----------|
| `feat` | Yeni özellik |
| `fix` | Bug düzeltme |
| `docs` | Dokümantasyon değişikliği |
| `style` | Kod formatı (davranış değişikliği yok) |
| `refactor` | Yeniden yapılandırma |
| `perf` | Performans iyileştirmesi |
| `test` | Test ekleme/düzeltme |
| `chore` | Build, CI, bağımlılık güncelleme |

### Kapsamlar

| Kapsam | Açıklama |
|--------|----------|
| `core` | C++ core library |
| `ui` | QML/Qt arayüz |
| `engine` | Oyun motoru desteği |
| `ci` | CI/CD pipeline |
| `docs` | Dokümantasyon |

### Örnekler

```
feat(engine): add Bethesda STRINGS parser
fix(ui): resolve library scan crash on empty path
docs: update build instructions for Qt 6.10
chore(ci): upgrade CodeQL action to v3
```

---

## Pull Request Süreci

1. **Branch adlandırma:** `feature/`, `fix/`, `docs/`, `chore/` prefix'leri kullanın
2. **Açıklama:** PR template'ini doldurun, değişiklikleri net açıklayın
3. **Testler:** Yeni özellikler için test ekleyin, mevcut testlerin geçtiğinden emin olun
4. **Review:** En az 1 review onayı gerekir
5. **CI:** Tüm CI check'leri geçmelidir
6. **Squash merge:** PR'lar squash merge ile birleştirilir

---

## Kod Stili

### C++

- C++23 standardı
- `clang-format` kullanın (proje kökündeki `.clang-format` dosyası)
- Header'larda `#pragma once` kullanın
- Namespace: `makineai`
- Sınıf isimleri: `PascalCase`
- Fonksiyon/değişken: `camelCase`
- Sabitler: `UPPER_SNAKE_CASE`

### QML

- Qt Quick Controls 2 kullanın
- Component dosya isimleri: `PascalCase.qml`
- Property isimleri: `camelCase`

### Format Kontrolü

```bash
# clang-format ile kontrol
just format

# Veya manuel
clang-format -i core/src/**/*.cpp core/include/**/*.hpp
```

---

## Issue Açma

### Bug Raporu

- Sorunu net tanımlayın
- Tekrar üretme adımlarını yazın
- Beklenen vs gerçekleşen davranışı belirtin
- Sistem bilgilerini ekleyin (Windows sürümü, Qt sürümü)

### Özellik Talebi

- Özelliği açıklayın
- Kullanım senaryolarını belirtin
- Varsa alternatif çözümleri tartışın

### Oyun Desteği Talebi

- `game-request` etiketini kullanın
- Oyun adı ve motoru belirtin
- Varsa mevcut çeviri kaynağı linkleyin

---

## Yardıma mı İhtiyacınız Var?

- `good first issue` etiketli issue'lara göz atın
- [Discussions](https://github.com/jlceaser/MakineAI/discussions) bölümünde soru sorun
- [Geliştirici Kılavuzu](docs/developer-guide/) okuyun

---

Katkılarınızla MakineAI'yı daha iyi yapabiliriz. Teşekkürler!
