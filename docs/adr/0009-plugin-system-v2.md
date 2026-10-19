# ADR-0009: Plugin System v2 — out-of-process plugins, signed registry, host API

## Status

Accepted. This ADR:
- supersedes the plugin parts of ADR-0004 ("no runtime plugin system");
- replaces the unshipped v1 design (spec `7ac3045:docs/superpowers/specs/2026-03-15-plugin-system-accessibility-ocr-design.md`).

Implemented on `dev`. Plugins stay behind `MAKINE_DEV_TOOLS` until the official plugins and the first signed registry are published (see *Rollout*).

## Date

2026-10-08

## Context

The v1 plugin system (March 2026) never reached a user: every release from v0.1.0 to v0.1.5 compiles it out (`#ifdef MAKINE_DEV_TOOLS` in `main.cpp`). Field usage is zero, so v2 needs no compatibility with v1. What v1 left behind:

- **An ABI too thin to build on.**
  - It had five C functions: `GetInfo`, `Initialize`, `Shutdown`, `IsReady`, `GetLastError`.
  - The launcher offered plugins nothing: no logging, events, game data, settings store or UI hooks.
  - So the official plugins grew ad-hoc exports (`makine_capture_ocr_translate`, `get/set_setting`) that the host probed by name.
  - None of Hook's features were reachable from the launcher.
- **In-process DLLs.** Live (ONNX Runtime OCR, network) and Hook (`CreateRemoteThread` injection) ran inside the launcher.
  - A plugin crash killed the launcher.
  - Injecting from the launcher process put the launcher itself on antivirus and anti-cheat radars.
  - Unloading a DLL with live threads is undefined behaviour.
- **No trust chain.**
  - `verifyDllSignature` existed but was never called.
  - The registry's `downloadUrl` fields were empty, and the hash was never checked on the fallback path.
  - Community discovery listed any GitHub repository with the `makine-plugin` topic, and that list included a brand-impersonating malware lure.
- **Promised but missing:**
  - capability consent at install;
  - anti-cheat checks before attaching to a game;
  - secret settings (the Live API key was a plain text field).
- **A public promise.** The website roadmap promises plugins that "arrive without waiting for an app update", plus a live-translation stage (OCR overlay + translator toolkit).

## Decision

### 1. Plugins run out of process

Each enabled plugin runs in its own `makine-plugin-host.exe` (`tools/plugin-host/`), shipped next to the launcher like `makine-elevate.exe`. The host is a small C++ program that:
- uses no Qt;
- is statically linked, so no C++ runtime DLL is in the process before the plugin;
- is a GUI-subsystem program, so it opens no console.

**Transport: the host's stdin/stdout.**
- `QProcess` owns both ends of the anonymous pipes, so there is no pipe name to squat and no token to pass.
- Frames are `uint32` little-endian length + UTF-8 JSON-RPC 2.0, at most 16 MiB.
- Before loading the plugin, the host duplicates the protocol handles, then points fd 0/1 and the std handles at `NUL`, so a stray `printf` cannot corrupt the stream.
- `stderr` stays a pipe, made unbuffered, and each line becomes a plugin log line.

**Loading.**
- `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)`.
- `AddDllDirectory(<plugin folder>)`.
- `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)`.
- The search order is therefore the plugin's folder, the host's folder, then System32. `PATH` and the current directory are never searched.
- A load failure is reported as JSON-RPC `-32000` carrying `data.winError`.

**Process settings (application manifest).** Every plugin inherits:
- per-monitor-v2 DPI awareness, so plugin coordinates are physical pixels;
- `activeCodePage` UTF-8, so narrow path APIs work under profile paths with Turkish letters (Windows 10 1903+);
- `longPathAware`.

**Threads.**
- A reader thread queues requests; the main thread calls the plugin one request at a time.
- A blocking `call` from any plugin thread waits on its own reply.
- On stdin EOF (launcher gone) the plugin gets `shutdown`, with a 5 s watchdog.

**Containment.**
- Every host is assigned to one Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, so hosts never outlive the launcher.
- A crash takes down only the host.
- The launcher restarts it with back-off (1 s, 5 s, 15 s). The third crash within 5 minutes marks the plugin *crashed* until the user restarts it.
- Disable, uninstall and update send `shutdown` with a 3 s grace period, then `TerminateProcess`. No DLL is ever unloaded.

**Timeouts.** `initialize` 15 s, `invoke` 30 s, actions 120 s.

### 2. ABI v2: one entry point, JSON payloads, no cross-module allocation

```c
MAKINE_PLUGIN_EXPORT MakineStatus makine_plugin_entry(uint32_t host_abi, MakinePluginApi* out);
```

**Structs (`core/include/makine/plugin/abi.h`).**
- `MakinePluginApi` and `MakineHostApi` are versioned structs (`struct_size`, `abi_version`) of function pointers.
- Fields are only ever appended. `struct_size` tells each side what the other knows.

**Data.**
- Data crosses the boundary as UTF-8 JSON text.
- Results come back through a caller-supplied `write(ctx, bytes, len)` callback, so no memory is ever freed by a module that did not allocate it.
- `MakineStatus` is an `int32`: `OK` 0, `E_FAILED` 1, `E_INVALID_ARGUMENT` 2, `E_NOT_FOUND` 3, `E_DENIED` 4, `E_UNSUPPORTED` 5, `E_BUSY` 6, `E_TIMEOUT` 7.

**Plugin → launcher (`MakineHostApi`).**
- `log`.
- `call(method, params) → result`: settings, UI, games, overlay; see §7.
- `emit_event(event, payload)`: fire-and-forget. The field is not named `emit` because that is a Qt macro.

**Launcher → plugin (`MakinePluginApi`).**
- `initialize(host, init_json)`.
- `shutdown`.
- `invoke(method, params) → result`: actions from the UI.
- `on_event(event, payload)`.

**Events.**
- `game.started` `{gameId, name}`
- `game.stopped` `{gameId}`
- `translation.installed` / `translation.uninstalled` `{gameId}`
- `settings.changed` `{key, value}`. It is sent only for changes the user makes, never as an echo of the plugin's own `settings.set`.

**SDK.** `makine/plugin/sdk.hpp` is a header-only C++20 layer. It provides:
- the `Plugin` base class;
- `Host` with `log`, `call` and `emitEvent`;
- `Result`;
- the `MAKINE_PLUGIN(Class)` macro.

It catches exceptions at the boundary.

### 3. Manifest v2 declares everything the launcher shows

**Fields.**
- `schema: 2`.
- A reverse-DNS `id`, which is also the install folder name.
- Semver `version` and `minLauncher`.
- `abi`.
- `entry`: a `.dll` inside the package.
- `publisher`.
- `capabilities`: each one carries a Turkish reason the user reads before agreeing.
- `contributes`:
  - typed `settings`: `toggle`, `select`, `text`, `secret`, `number`, `path`;
  - `actions`: buttons on the plugin page that `invoke` a method;
  - the `events` the plugin wants.

**UI.** The launcher renders the plugin UI from the manifest; plugins ship no QML.

**Settings storage.**
- Settings live in `plugin-data/<id>/settings.json`.
- Secrets are encrypted with DPAPI (`CryptProtectData`, current user, the plugin id as entropy).
- Secrets are decrypted only into the plugin's `init.settings` and `settings.*` answers.

### 4. Signed registry on the CDN; no GitHub-topic discovery

**The index.**
- `https://cdn.makineceviri.org/assets/plugins/index.json` lists the official plugins with:
  - version, `abi` and `minLauncher`;
  - capabilities;
  - SHA-256 and size;
  - the package URL.
- The URL must be exactly `https://cdn.makineceviri.org/data/plugins/<id>/<version>.mkplugin`. An entry pointing anywhere else is dropped.

**The signature.**
- `index.json.sig` is a base64 Ed25519 signature over the exact bytes of the index.
- The launcher embeds the public key (`pluginregistry.cpp`) and refuses an index that does not verify.
- Package hashes in a verified index complete the chain.
- The private key lives outside every repository; `scripts/plugin_registry.py` reads it from `MAKINE_PLUGIN_SIGNING_KEY`.

**Packages.** A package is MKPK v2 (`"MKPK" 0x02` + zstd(tar)), the container the launcher already reads. Encrypted v1 packages are refused for plugins.

**Discovery and the path into the registry.**
- GitHub-topic community discovery is removed.
- A third-party plugin installs only from a file, after the user turns on *Geliştirici modu*. It is marked *Doğrulanmamış*.
- The path into the official registry is a PR to `Makine-LauncherPlugins`. A maintainer reviews, builds, packs, uploads and signs it (`docs/plugins/README.md`, *Publishing*).

### 5. Capabilities are consent plus review, with enforcement where the launcher can

**The capabilities:** `network`, `screen-capture`, `overlay`, `game-files-read`, `game-files-write`, `process-inject`.

**Consent.**
- A plugin does not start until the user has agreed to every capability it declares.
- An update that adds one puts the plugin back into *needs-consent*.

**Enforcement.** Native code cannot be sandboxed by a manifest, so the guarantee is the signed registry plus review. Where the launcher mediates, it enforces:
- `overlay.*` needs `overlay`;
- `games.list` needs `game-files-read`;
- `process-inject` plugins must ask the launcher for a game with `games.requestAttach`:
  - the launcher refuses games with a detected anti-cheat (`GameService::checkAntiCheat`, plus a running-process scan), whatever the user says;
  - otherwise it asks the user once per game, or remembers their "always".

**Plugin states:** `disabled`, `needs-consent`, `starting`, `running`, `stopping`, `error`, `crashed`, `incompatible` (ABI or `minLauncher`), `invalid` (broken manifest or folder).

### 6. Install, update and removal are transactional

**Install.**
1. Download.
2. Verify size and SHA-256 against the signed index.
3. Extract to `plugins/.staging/<random>` (path traversal checked).
4. Validate the manifest: schema, ABI, `minLauncher`, the entry file exists, and the id and version match the index.
5. Stop the running version.
6. Rename `plugins/<id>` to `plugins/.previous/<id>`.
7. Rename the staging folder to `plugins/<id>`.

**Update.** If the new version fails to start, the previous one is renamed back and started.

**Renames.** Every rename retries for 2 s, so a scanner's brief handle doesn't fail the update.

**Removal.** Removal renames the folder into `plugins/.trash` before deleting it.

**Plugin data.** Data lives in `plugin-data/<id>`, survives updates, and is removed on uninstall only if the user asks.

### 7. The launcher draws; plugins ask

**Plugin UI.** Plugins have no UI of their own in the launcher. What they show goes through host methods:
- `ui.notify`: a toast;
- `ui.setStatus`: one line under the plugin's name;
- `overlay.pickRegion`: the launcher's full-screen region selector. The call blocks until the user picks or cancels.
- `overlay.show` / `overlay.hide`: the launcher's click-through translation overlay. One plugin owns it at a time.

**Coordinates.** All coordinates crossing the plugin API are physical pixels of the virtual desktop, the space of DXGI Desktop Duplication. The launcher converts to and from Qt's logical coordinates per screen.

**The OCR loop.** Live's capture → OCR → translate loop runs inside the plugin. Only the resulting text crosses the pipe.

## Consequences

- There is one more helper executable to build, ship (ZIP + MSIX: staged when present) and sign.
- IPC adds sub-millisecond latency. That is fine for UI actions, events and overlay text, and wrong for per-frame data, which stays inside the plugin.
- The official plugins must be ported to ABI v2. Live and Hook become ordinary plugins with no special-cased exports in the launcher.
- Plugin authors get a dev loop without the launcher: `scripts/plugin_dev.py` runs a plugin in the real host and answers its host calls.

## Rollout

| Step | State |
|------|-------|
| Host, ABI, SDK, manager, signed registry, installer, tests | done (`dev`) |
| Template and Live ported to ABI 2 | in progress |
| Hook | **not** published. Its in-game text hooks patch API prologues without instruction decoding (crash on first draw). It injects with no bitness check and does loader-lock work in `DllMain`. It needs a MinHook-style rewrite on top of `games.requestAttach`. Its memory-scanning scope is an owner decision. |
| First signed `index.json` on the CDN, then remove the `MAKINE_DEV_TOOLS` gate and add `makine-plugin-host` to the release build | v0.2.0 |

## Out of scope (later)

- OS-level sandboxing (AppContainer) for plugins that need only the network.
- Plugin-provided QML UI.
- A website plugin catalogue (`/eklentiler`).
