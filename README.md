> **Makine Launcher** is a desktop application that delivers Turkish translations for Windows games.
> Built with Qt 6 / QML and C++23, it detects installed games, manages translation packages, and keeps them up to date.

---

<p align="center">
  <img src="https://cdn.makineceviri.org/assets/images/logo.png" alt="Makine Launcher" width="128">
</p>

<h1 align="center">Makine Launcher</h1>

<p align="center">
  Windows oyunlarını Türkçeye çeviren ve çevirileri güncel tutan masaüstü uygulaması.
</p>

## Gereksinimler

- **Windows** 10/11 x64
- **Qt** 6.10+ (MinGW 13.1.0 veya MSVC 2022)
- **CMake** 3.25+
- **vcpkg** (`dev-ui` preset'i hariç tüm build'ler için)
- **[just](https://github.com/casey/just)** (opsiyonel task runner)

## Derleme

```bash
git clone https://github.com/MakineCeviri/Makine-Launcher.git
cd Makine-Launcher

just dev        # MinGW dev build (Core + UI)
just run        # Build + çalıştır
```

vcpkg olmadan sadece UI geliştirmek için:

```bash
just dev-ui
```

## Katkıda Bulunma

[CONTRIBUTING.md](CONTRIBUTING.md) dosyasına bakınız.

## Lisans

[AGPL-3.0](LICENSE) + Commons Clause
