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

Then package the existing static EXE:

```
makeappx pack /d <staging-with-exe+Assets> /p Makine-Launcher.msix
# sign with the SAME cert as scripts/sign_exe.ps1 (Publisher must match):
signtool sign /fd SHA256 /a Makine-Launcher.msix
```

Add this as a step in the local release tooling (a `just msix` recipe / a
packaging script) after `just release-zip-signed`. The manifest lives in the
canonical dev repo and is cherry-picked to public like the rest of the code.

## Required logo assets (not yet created)

`makeappx` needs PNGs under `Assets\` referenced by the manifest:

| File | Size |
|------|------|
| `StoreLogo.png` | 50×50 |
| `Square44x44Logo.png` | 44×44 |
| `Square71x71Logo.png` | 71×71 |
| `Square150x150Logo.png` | 150×150 |
| `Square310x310Logo.png` | 310×310 |
| `Wide310x150Logo.png` | 310×150 |
| `SplashScreen.png` | 620×300 |

Generate from `qml/resources/images/logo.png` / `resources/app_icon.ico`.
