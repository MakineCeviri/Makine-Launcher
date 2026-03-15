# MakineAI Plugin Template

A template for creating MakineAI Launcher plugins.

## Quick Start

1. **Use this template** — Click "Use this template" on GitHub
2. **Edit manifest.json** — Set your plugin id, name, description
3. **Write your plugin** — Edit `plugin.cpp`, add your logic
4. **Build** — `cmake -B build -G Ninja && cmake --build build`
5. **Package** — `python makine-pack.py ./build/release/`
6. **Release** — Create a GitHub Release, attach the `.makine` file
7. **Discover** — Add `makineai-plugin` topic to your repo

## Project Structure

```
├── manifest.json    — Plugin metadata (id, name, version, entry DLL)
├── plugin.cpp       — Plugin implementation (C ABI exports)
├── CMakeLists.txt   — Build configuration
└── include/         — (optional) Local copy of plugin API headers
    └── makineai/plugin/
        ├── plugin_api.h
        └── plugin_types.h
```

## Plugin API

Every plugin DLL must export these 5 functions:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `makineai_get_info` | `MakineAiPluginInfo (void)` | Return plugin metadata |
| `makineai_initialize` | `MakineAiError (const char* dataPath)` | Initialize plugin |
| `makineai_shutdown` | `void (void)` | Clean up resources |
| `makineai_is_ready` | `bool (void)` | Check if plugin is ready |
| `makineai_get_last_error` | `const char* (void)` | Get last error message |

## Building

Requirements: CMake 3.25+, MinGW GCC 13.1+ (or any C++23 compiler)

```bash
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

Output: `build/release/my-plugin.dll` + `build/release/manifest.json`

## Packaging

Install the packager: `pip install zstandard`

```bash
python makine-pack.py ./build/release/
```

Output: `com-github-username-my-plugin-0.1.0.makine`

## Publishing

1. Create a GitHub Release (e.g., `v0.1.0`)
2. Attach the `.makine` file as a release asset
3. Add `makineai-plugin` topic to your repository
4. Your plugin will appear in MakineAI Launcher's community section

## Getting Approved

To get the "Approved" badge in the launcher:
1. Keep your plugin open source
2. Follow the [Plugin Guidelines](https://makineceviri.net/docs/plugin-guidelines)
3. Submit for review at [MakineCeviri/makineai-plugins](https://github.com/MakineCeviri/makineai-plugins/issues/new)

## License

GPL-3.0 — See [LICENSE](LICENSE)
