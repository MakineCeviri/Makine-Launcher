#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""Generate the MSIX tile/logo PNG assets from the app logo.

Each asset is the brand logo, aspect-preserved and centred on a fully
transparent canvas of the exact size the AppxManifest requires (the
manifest sets BackgroundColor="transparent", so the tile takes the
system accent behind these). Run from the repo root:

    python scripts/gen_msix_assets.py
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "qml" / "resources" / "images" / "logo.png"
OUT = ROOT / "packaging" / "msix" / "Assets"

# name -> (width, height, logo coverage fraction of the shorter side)
ASSETS = {
    "StoreLogo.png":        (50, 50, 0.90),
    "Square44x44Logo.png":  (44, 44, 0.90),
    "Square71x71Logo.png":  (71, 71, 0.85),
    "Square150x150Logo.png": (150, 150, 0.75),
    "Square310x310Logo.png": (310, 310, 0.70),
    "Wide310x150Logo.png":  (310, 150, 0.62),
    "SplashScreen.png":     (620, 300, 0.55),
}


def main() -> int:
    if not SRC.exists():
        print(f"ERROR: source logo not found: {SRC}")
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    logo = Image.open(SRC).convert("RGBA")
    lw, lh = logo.size
    for name, (w, h, cover) in ASSETS.items():
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        target = int(min(w, h) * cover)
        scale = min(target / lw, target / lh)
        nw, nh = max(1, round(lw * scale)), max(1, round(lh * scale))
        resized = logo.resize((nw, nh), Image.LANCZOS)
        canvas.alpha_composite(resized, ((w - nw) // 2, (h - nh) // 2))
        dst = OUT / name
        canvas.save(dst, "PNG")
        print(f"  {name:24} {w}x{h}")
    print(f"OK: {len(ASSETS)} assets -> {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
