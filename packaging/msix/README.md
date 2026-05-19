# MSIX Packaging — Makine Launcher

Status: **scaffold** (B5-01). The launcher ships as a signed static EXE + ZIP
built **locally** (`just release-zip-signed <version>`). GitHub Actions / the
self-hosted CI runner were removed 2026-05-19 — there is no CI; releases are
produced on the maintainer machine. There is **no** MSIX/Store path yet — this
directory is the code side of it.

## Why the Store shows "MakineAI"

There was never an MSIX manifest in any repo. The name shown in the Microsoft
Store comes from the **app reservation in Microsoft Partner Center**, not code.
Fixing it has two parts:

| Part | Where | Who | Status |
|------|-------|-----|--------|
| Manifest `DisplayName` = `Makine Launcher` | `AppxManifest.xml.in` | code | **done (this scaffold)** |
| Reserved app name + Identity/Publisher | Partner Center (web) | account owner | **pending — only you can do this** |

The Store name only changes once a package whose `Identity` matches a
Partner-Center-reserved name **"Makine Launcher"** is submitted. A correct
manifest alone does not rename an app already reserved as "MakineAI".

## Partner Center steps (web, manual)

1. Partner Center → your app → **Product management → Manage app names**:
   reserve **`Makine Launcher`**. (If the app itself is reserved as "MakineAI",
   reserve the new name, then make it the primary; the old name can be removed
   after the first submission with the new name.)
2. **App identity** page → note exactly:
   - Package/Identity **Name** (e.g. `MakineCeviri.MakineLauncher`)
   - **Publisher** (e.g. `CN=ABCD1234-...`) — must equal the signing cert subject.

## Build-side wiring (next, when identity values are known)

`AppxManifest.xml.in` is a CMake template. Configure with:

```
-DMSIX_IDENTITY_NAME=<Partner Center Identity Name>
-DMSIX_PUBLISHER="<Partner Center Publisher, == cert CN>"
-DMSIX_VERSION=0.1.0.0          # 4-part; Store requires revision .0
```

## Build pipeline (wired — 2026-05-19)

Everything except the two Partner Center values + signing is ready:

```
just release-static                                  # current static EXE
just msix <X.Y.Z.0> <IdentityName> "<CN=Publisher>"  # -> dist/Makine-Launcher-v<ver>.msix
# owner only (cert in scripts/certs/, not in CI):
signtool sign /fd SHA256 /f <cert.pfx> /p <pwd> dist/Makine-Launcher-v<ver>.msix
```

- `scripts/gen_msix_assets.py` (`just msix-assets`) generates the 7 tile
  PNGs into `Assets/` from `qml/resources/images/logo.png` — **done, committed**.
- `scripts/make_msix.ps1` (`just msix`) substitutes `@MSIX_*@` into the
  manifest, stages exe+Assets+manifest, runs `makeappx pack`. It **refuses
  placeholder Identity/Publisher** so a bad package can't reach the Store.
- `Version` is 4-part, Store requires the `.0` revision (e.g. `0.1.0.0`).

## What is still owner-only (hard blockers, not code)

| Need | Where | Why |
|------|-------|-----|
| Identity **Name** + **Publisher** (`CN=…`) | Partner Center → App identity | MSIX `Identity` must match the reservation or submission is rejected |
| Reserve name **`Makine Launcher`** | Partner Center → Manage app names | Store display name comes from the reservation, not the manifest |
| Sign the `.msix` | `scripts/certs/` cert (cert subject **==** Publisher) | unsigned packages are not Store-submittable |

Generated `Assets/` PNGs:
`StoreLogo` 50² · `Square44x44` · `Square71x71` · `Square150x150` ·
`Square310x310` · `Wide310x150` · `SplashScreen` 620×300.
