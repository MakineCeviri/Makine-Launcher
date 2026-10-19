# Makine plugins (ABI 2)

A Makine plugin is a 64-bit Windows DLL plus a `manifest.json`, shipped as a
`.mkplugin` package. Makine never loads plugin code into the launcher: every
enabled plugin runs in its own `makine-plugin-host.exe`, so a crash or a hang
takes down only that plugin. Design and rationale: [ADR-0009](../adr/0009-plugin-system-v2.md).

```
manifest.json        what the launcher shows: name, capabilities, settings, actions
bin/my-plugin.dll    exports one function, makine_plugin_entry
...                  anything else the plugin needs (models, DLLs, data)
```

## Quick start

1. Start from [Makine-LauncherPlugin-Template](https://github.com/MakineCeviri/Makine-LauncherPlugin-Template).
   It carries copies of the SDK headers and of the two tools below (`tools/`).
2. Build: `cmake -B build -G Ninja && cmake --build build` → `build/package/`.
3. Run it without the launcher:
   `python tools/plugin_dev.py build/package --host <Makine folder>/makine-plugin-host.exe --invoke hello`
   (or set `MAKINE_PLUGIN_HOST`). `--invoke` and `--event` run in the order given.
4. Pack: `cmake --build build --target pack`, or `python tools/plugin_registry.py pack build/package`
   → `<id>-<version>.mkplugin`. `pack` applies the launcher's own manifest rules, so a
   package that packs also installs.
5. Try it in Makine: Ayarlar → Eklentiler → *Geliştirici modu* → *Dosyadan yükle*.

## Writing a plugin

Include `makine/plugin/sdk.hpp` (header-only, C++20; it includes the C ABI in
`makine/plugin/abi.h`). Copy both headers into your repository. The ABI only ever
grows at the end of its structs, and a launcher loads every plugin from
`MAKINE_PLUGIN_ABI_MIN_VERSION` (2) up to its own ABI, so a plugin built against
ABI 2 keeps working.

```cpp
#include <makine/plugin/sdk.hpp>

using makine::plugin::Result;

class HelloPlugin : public makine::plugin::Plugin {
public:
    MakineStatus initialize(std::string_view initJson) override
    {
        host().info("ready");
        return MAKINE_OK;
    }

    Result invoke(std::string_view method, std::string_view paramsJson) override
    {
        if (method == "hello") {
            host().call("ui.notify", R"({"title":"Merhaba","message":"Eklenti çalışıyor"})");
            return Result::ok(R"({"message":"Selam verildi"})");
        }
        return Result::notFound(method);
    }
};

MAKINE_PLUGIN(HelloPlugin)
```

Payloads are UTF-8 JSON text. The SDK only writes JSON (`jsonString`); to read
`init` and params use a JSON library — [nlohmann/json](https://github.com/nlohmann/json)
(MIT, header-only) is what the template and the official plugins use.
`Result::ok(json)` answers with a result, `Result::error(status, message)` with a
failure the user sees.

### Rules the host enforces or relies on

| Rule | Why |
|------|-----|
| `initialize`, `invoke`, `on_event`, `shutdown` run on one host thread, one at a time | No locking needed between them. Long work belongs on your own thread. |
| `host()` functions are thread-safe; `call` blocks until the launcher answers | Calls that wait for the user (`overlay.pickRegion`, `games.requestAttach`) can take seconds; make them from your worker thread. |
| Do no work in `DllMain` or `makine_plugin_entry` | Start in `initialize`; stop threads in `shutdown` (3 s budget, then the host is killed). |
| `stdout` goes nowhere; `stderr` lines appear in the plugin log | `stdout` carries the protocol; the host points it at `NUL` before loading you. |
| Paths in JSON are UTF-8: convert with `MultiByteToWideChar(CP_UTF8, …)` and use wide APIs | `dataDir` sits under the Windows user profile (`C:\Users\Şükrü\…`). The host's manifest sets the process code page to UTF-8, so narrow `fopen`/`ifstream`/`LoadLibraryA` take UTF-8 too — but only on Windows 10 1903 and later. That does not save a library that widens a `char*` path byte by byte (RapidOcrOnnx does): pass it a path relative to a working directory you set, and don't count on 8.3 short names, which are off on many volumes. |
| Screen coordinates are physical pixels of the virtual desktop | The host is per-monitor DPI aware, like the launcher: `overlay.*` rectangles match DXGI Desktop Duplication and `BitBlt` on the screen DC, unscaled. The first frame after `DuplicateOutput` is all zeros (`LastPresentTime` 0); wait for the next one. |
| A failed `initialize` reports the last `host().error(...)` line | Log why before returning a failure status; the user sees that line, otherwise only "plugin initialize failed". |
| DLL search: your folder, the launcher folder, System32 — never `PATH` or the current directory | Link the C++ runtime statically (MinGW: `-static -static-libgcc -static-libstdc++`), or ship the runtime DLLs in your folder. Otherwise you get the launcher's copy, whatever version it is. |
| Exceptions must not cross the ABI | `sdk.hpp` catches them and returns `MAKINE_E_FAILED`. |

Timeouts: `initialize` 15 s, `invoke` 30 s (actions 120 s). Three crashes within five
minutes stop the plugin until the user restarts it.

## manifest.json

```json
{
  "schema": 2,
  "id": "org.example.hello",
  "name": "Merhaba",
  "version": "1.0.0",
  "description": "Örnek eklenti",
  "publisher": "Örnek Geliştirici",
  "license": "MIT",
  "homepage": "https://github.com/example/hello",
  "abi": 2,
  "minLauncher": "0.2.0",
  "entry": "bin/hello.dll",
  "capabilities": [
    { "id": "network", "reason": "Çeviri servisine metin gönderir." }
  ],
  "contributes": {
    "settings": [
      { "key": "provider", "type": "select", "label": "Çeviri servisi",
        "options": [ { "value": "deepl", "label": "DeepL" }, "google" ], "default": "deepl" },
      { "key": "apiKey", "type": "secret", "label": "API anahtarı" }
    ],
    "actions": [
      { "id": "hello", "label": "Selam ver", "description": "Bir bildirim gösterir" }
    ],
    "events": [ "game.started", "settings.changed" ]
  }
}
```

| Field | Rule |
|-------|------|
| `schema` | `2` |
| `id` | reverse-DNS, lowercase (`org.example.hello`); also the install folder name |
| `version`, `minLauncher` | semver (`1.2.0`, `0.2.0-beta`); `minLauncher: "0.2.0"` accepts Makine `0.2.0-beta`, since every Makine release carries that tag |
| `abi` | `2` |
| `entry` | the DLL, relative to the package root; no `..` |
| `publisher` | required |
| `capabilities` | each `{id, reason}`; the Turkish `reason` is shown when the user installs |
| `contributes.settings` | rendered by Makine; values reach the plugin in `init.settings` and `settings.*` |
| `contributes.actions` | buttons on the plugin page; pressing one calls `invoke(method)` (`method` defaults to `id`) |
| `contributes.events` | the events the plugin receives |

**Capabilities.** A plugin does not run until the user has agreed to every
capability it declares; an update that adds one asks again. Native code cannot be
sandboxed by a manifest: the guarantee is review plus the signed registry. Where
the launcher is in the path, it enforces them — host methods below check them.

| id | Meaning | Enforced by Makine |
|----|---------|--------------------|
| `network` | talks to the internet | consent only |
| `screen-capture` | reads screen pixels | consent only |
| `overlay` | draws over the game through the launcher overlay | `overlay.*` |
| `game-files-read` | reads game folders | `games.list` |
| `game-files-write` | changes game files | consent only |
| `process-inject` | attaches to a running game | `games.requestAttach` |

**Setting types.** `toggle` (bool), `select` (`options`: strings or `{value,label}`),
`text`, `secret` (stored with Windows DPAPI, never shown again, no `default`),
`number` (`min`, `max`), `path` (`mode`: `folder` or `file`). Every type takes
`label`, optional `description` and `default`.

## Host API (`host().call(method, params)`)

| Method | Params → result | Needs |
|--------|-----------------|-------|
| `settings.get` | `{key}` → value | |
| `settings.all` | → `{key: value}` (secrets decrypted) | |
| `settings.set` | `{key, value}` → `true`; no `settings.changed` back to you | |
| `ui.notify` | `{title, message, kind: info\|success\|warning\|error}` | |
| `ui.setStatus` | `{text}`: one line under the plugin's name | |
| `games.running` | → `{gameId, name}` or `null` | |
| `games.list` | → `[{id, name, engine, installPath}]` | `game-files-read` |
| `games.requestAttach` | `{gameId}` → `{allowed, gameId, name, installPath}` | `process-inject` |
| `overlay.pickRegion` | → `{x, y, width, height}`; `MAKINE_E_DENIED` if cancelled | `overlay` |
| `overlay.show` | `{text, sourceText?, x?, y?, width?, height?}` | `overlay` |
| `overlay.hide` | | `overlay` |

`ui.*` and `overlay.show/hide` also work fire-and-forget through
`host().emitEvent(method, params)`, which suits frequent overlay updates.

`games.requestAttach` refuses games with a detected anti-cheat, whatever the user
says, and otherwise asks the user once per game (or remembers their "always").

## Events (`onEvent`)

| Event | Payload |
|-------|---------|
| `game.started` | `{gameId, name}` |
| `game.stopped` | `{gameId}` |
| `translation.installed` | `{gameId}` |
| `translation.uninstalled` | `{gameId}` |
| `settings.changed` | `{key, value}`, when the user changes a setting |

`init` JSON passed to `initialize`: `pluginId`, `version`, `launcherVersion`, `abi`,
`locale`, `dataDir` (persistent, survives updates), `pluginDir`, `settings`.

## Status codes

`MAKINE_OK` 0 · `E_FAILED` 1 · `E_INVALID_ARGUMENT` 2 · `E_NOT_FOUND` 3 ·
`E_DENIED` 4 · `E_UNSUPPORTED` 5 · `E_BUSY` 6 · `E_TIMEOUT` 7.

## Publishing

Official plugins are listed in the signed registry
`https://cdn.makineceviri.org/assets/plugins/index.json` (+ `index.json.sig`).
Makine refuses an index whose Ed25519 signature does not verify, and a package
whose SHA-256 differs from the index.

1. Open a PR to [Makine-LauncherPlugins](https://github.com/MakineCeviri/Makine-LauncherPlugins)
   with your repository and release tag.
2. A maintainer reviews the code, builds it, then:
   ```
   python scripts/plugin_registry.py pack <package dir>
   wrangler r2 object put makineai-translations/data/plugins/<id>/<version>.mkplugin --file <id>-<version>.mkplugin --remote
   python scripts/plugin_registry.py add <id>-<version>.mkplugin --index <Assets>/assets/plugins/index.json
   python scripts/plugin_registry.py sign --index <Assets>/assets/plugins/index.json
   ```
   and pushes Makine-Launcher-Assets, whose workflow syncs `assets/` to R2.
   The signing key lives outside every repository (`MAKINE_PLUGIN_SIGNING_KEY`).

Anything else installs only from a file, in developer mode, marked *Doğrulanmamış*.

## Wire protocol (for reference)

Launcher ↔ host over the host's stdin/stdout: `uint32` little-endian length +
UTF-8 JSON-RPC 2.0. Launcher → host: `initialize {init}`, `invoke {method, params}`,
`shutdown`, notification `event {event, payload}`. Host → launcher: `call {method,
params}`, notifications `log {level, message}` and `emit {event, payload}`.
Implementations: `tools/plugin-host/plugin_host.cpp`, `qml/src/services/pluginprocess.cpp`.
