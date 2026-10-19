# xdelta3 (vendored)

VCDIFF (RFC 3284) decoder behind the `xdelta` install step
(`qml/src/services/xdeltaapply.cpp`). Assassin's Creed Odyssey's translation
ships as xdelta3 deltas against the game's `.forge` archives; the legacy
installer applied them with `c1.exe` (xdelta3 3.0.4). The launcher decodes them
itself so no external tool has to start, survive antivirus, or be trusted with
paths.

| | |
|---|---|
| Upstream | https://github.com/jmacd/xdelta |
| Branch | `release3_1_apl` — the Apache-2.0 relicensed tree (tags such as `v3.1.0` are GPL-2.0; not used) |
| Commit | `7508fd2a823443b1f0173ca361620f21d62a7d37` |
| License | Apache-2.0 (`LICENSE`, copied unchanged) |
| Files | from upstream `xdelta3/`: `xdelta3.c` and the headers it includes for a decoder-only build; nothing else |

Files are byte-identical to upstream. The build (`CMakeLists.txt`) turns the
encoder off and the DJW/FGK secondary decoders on: the legacy deltas carry a
DJW-compressed section (VCDIFF header indicator `0x05`, secondary id 1).

Updating: copy the same file list from a newer commit of the same branch, run
`test_xdeltaapply` (its deltas were made by the legacy `c1.exe`), and record
the commit above.
