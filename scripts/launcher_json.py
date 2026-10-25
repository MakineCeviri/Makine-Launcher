#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
launcher_json.py -- Write the launcher's update manifest for the CDN.

The launcher carries its notes and plan in qml/resources/whatsnew.json. The
copy on cdn.makineceviri.org/assets/launcher.json is the same file plus the
newest version of each channel; launchers (0.2.3+) read it to say a newer
version is out and to show its notes (qml/src/services/releasenotes.h):

  latest  newest GitHub release (ZIP) — "İndir" opens `page`
  store   what the Microsoft Store serves now — "Store'da güncelle"

Set `store` only once Partner Center has published the package: a Store
launcher told about a version still in certification opens a Store page
with nothing to update.

  python scripts/launcher_json.py --latest 0.2.3-beta --store 0.1.5-beta \
      --out ../Makine-Launcher-Assets/assets/launcher.json

The Assets repo publishes the file to R2 when pushed to main.
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NOTES = ROOT / "qml" / "resources" / "whatsnew.json"
PAGE = "https://github.com/MakineCeviri/Makine-Launcher/releases/latest"
VERSION = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--latest", required=True, help="newest GitHub release, e.g. 0.2.3-beta")
    ap.add_argument("--store", required=True, help="version the Store serves now, e.g. 0.1.5-beta")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    for name in ("latest", "store"):
        if not VERSION.match(getattr(args, name)):
            print(f"--{name} {getattr(args, name)!r} is not X.Y.Z[-suffix]", file=sys.stderr)
            return 1

    notes = json.loads(NOTES.read_text(encoding="utf-8"))
    if args.latest not in {r["version"] for r in notes["releases"]}:
        print(f"--latest {args.latest} has no entry in {NOTES.name}", file=sys.stderr)
        return 1

    manifest = {"latest": args.latest, "store": args.store, "page": PAGE, **notes}
    args.out.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8",
                        newline="\n")
    print(f"{args.out}: latest {args.latest}, store {args.store}, {len(notes['releases'])} releases, "
          f"{len(notes['roadmap'])} plan items")
    return 0


if __name__ == "__main__":
    sys.exit(main())
