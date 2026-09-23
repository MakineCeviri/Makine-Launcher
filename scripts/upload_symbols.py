#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
upload_symbols.py -- Give Sentry the symbols of the binary we are about to ship.

Every 0.1.4 crash arrived with our own frames as "?": the only symbol upload
lived in deploy.py (the translation-package pipeline), and the ZIP and MSIX
release flows never ran it. Diagnosing the top crash meant resolving raw
offsets by hand against a local copy of the shipped exe.

This runs as part of `just release-zip-dynamic` and `just msix-dynamic`, and
fails the release when the symbols cannot be matched or uploaded:
  0. the DSN compiled into the build must belong to the Sentry project this
     repo is configured for — 0.1.5 moved to a new organization, and a build
     configured from a stale .env would otherwise ship reporting to the old one;
  1. the .sym copy (taken before strip) and the shipped exe must carry the
     same, non-zero Debug ID — otherwise Sentry cannot pair them;
  2. both are uploaded with sentry-cli, org/project passed explicitly —
     sentry-cli also reads .env, and a stale SENTRY_ORG there outranks
     .sentryclirc (the first 0.1.5 upload went to the old org and got 403);
  3. the Debug ID is read back from Sentry — "upload succeeded" has not meant
     "it is there" often enough in this project.

Usage:
    python scripts/upload_symbols.py                        # build/release-mingw
    python scripts/upload_symbols.py --build-dir build/dev --dry-run
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

import sentry_triage as st

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

ROOT = Path(__file__).resolve().parent.parent
ZERO_ID = "00000000-0000-0000-0000-000000000000"


def debug_id(path: Path) -> str:
    out = subprocess.run(["sentry-cli", "debug-files", "check", str(path)],
                         capture_output=True, text=True, cwd=str(ROOT))
    m = re.search(r"Debug ID:\s*([0-9a-f-]+)", out.stdout + out.stderr)
    return m.group(1) if m else ""


def embedded_dsn_project(build: Path) -> str:
    """Project id of the DSN baked in at configure time (from build.ninja)."""
    ninja = build / "build.ninja"
    if not ninja.exists():
        return ""
    m = re.search(r'SENTRY_DSN=\\"https://[^@"]+@[^/"]+/(\d+)', ninja.read_text(errors="replace"))
    return m.group(1) if m else ""


def configured_project_id(token: str) -> str:
    url = f"{st.SENTRY_BASE_URL}/projects/{st.SENTRY_ORG}/{st.SENTRY_PROJECT}/"
    req = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return str(json.load(resp).get("id", ""))


def uploaded(token: str, did: str) -> bool:
    url = (f"{st.SENTRY_BASE_URL}/projects/{st.SENTRY_ORG}/{st.SENTRY_PROJECT}"
           f"/files/dsyms/?query={did}")
    req = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        files = json.load(resp)
    # Compare the UUID part only, so a difference in the age suffix ("…-1")
    # cannot turn a successful upload into a false miss.
    return any((f.get("debugId") or "").lower()[:36] == did.lower()[:36] for f in files)


def main() -> int:
    parser = argparse.ArgumentParser(description="Upload release symbols to Sentry")
    parser.add_argument("--build-dir", default=str(ROOT / "build" / "release-mingw"))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    build = Path(args.build_dir)
    exe = build / "Makine-Launcher.exe"
    sym = build / "Makine-Launcher.exe.sym"
    for f in (exe, sym):
        if not f.exists():
            print(f"HATA: {f} yok — Release MinGW yapısı .sym kopyasını strip'ten önce üretir.")
            return 1

    exe_id, sym_id = debug_id(exe), debug_id(sym)
    print(f"Debug ID  exe: {exe_id or '-'}  sym: {sym_id or '-'}")
    if not exe_id or exe_id.startswith(ZERO_ID) or exe_id != sym_id:
        print("HATA: exe ile .sym aynı, sıfır olmayan Debug ID'yi taşımıyor — "
              "Sentry çökmeleri bu sembollerle eşleştiremez.")
        return 1

    if args.dry_run:
        print(f"[DRY RUN] {exe.name} + {sym.name} yüklenecekti.")
        return 0

    token = st.load_token()
    if not token:
        print("HATA: SENTRY_AUTH_TOKEN yok — semboller yüklenmeden yayın çıkarsa "
              "çökme raporları okunamaz.")
        return 1

    baked, expected = embedded_dsn_project(build), configured_project_id(token)
    print(f"DSN projesi  yapıda: {baked or '-'}  beklenen ({st.SENTRY_ORG}/{st.SENTRY_PROJECT}): {expected}")
    if not baked or baked != expected:
        print("HATA: yapıya gömülü DSN yapılandırılmış Sentry projesine ait değil — "
              ".env'deki MAKINE_SENTRY_DSN'i güncelleyip yeniden yapılandırın.")
        return 1

    env = dict(os.environ, SENTRY_AUTH_TOKEN=token)
    r = subprocess.run(["sentry-cli", "debug-files", "upload", "--include-sources",
                        "--org", st.SENTRY_ORG, "--project", st.SENTRY_PROJECT,
                        str(sym), str(exe)],
                       capture_output=True, text=True, cwd=str(ROOT), env=env)
    if r.returncode != 0:
        print(f"HATA: sentry-cli yükleme başarısız: {(r.stderr or r.stdout)[:300]}")
        return 1

    # Sentry assembles uploads asynchronously: right after sentry-cli returns,
    # the file is often not listed yet (seen on the first run of this script).
    for _ in range(18):
        if uploaded(token, exe_id):
            print(f"Semboller Sentry'de: {exe_id}")
            return 0
        time.sleep(5)
    print(f"HATA: yükleme 'başarılı' dedi ama {exe_id} 90 sn içinde Sentry'de görünmedi.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
