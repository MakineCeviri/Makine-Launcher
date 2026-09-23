#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
set_sentry_token.py -- Replace the Sentry auth token in .env and in CI.

The token is used by symbol upload (release flow) and by the daily telemetry
watchdog (GitHub secret SENTRY_AUTH_TOKEN). When it is revoked or rotated,
the watchdog fails with 401 the next morning; this is the fix.

Sentry issues tokens only from a browser session (the API refuses to create
them), so the owner copies one — preferably from the org-owned "Makine CI"
internal integration, Settings > Developer Settings — and runs:

    python scripts/set_sentry_token.py          # token read from the clipboard

The token is never printed. It must open the configured project before
anything is written; then .env's SENTRY_AUTH_TOKEN and the GitHub secret are
replaced and the clipboard is cleared. .env is written by the owner's own
process because assistant tooling is not allowed to touch it.
"""

import os
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

import sentry_triage as st

ROOT = Path(__file__).resolve().parent.parent
ENV = ROOT / ".env"
REPO = "MakineCeviri/Makine-Launcher"


def powershell(cmd: str) -> str:
    return subprocess.run(["powershell", "-NoProfile", "-Command", cmd],
                          capture_output=True, text=True).stdout


def status(path: str, token: str) -> int:
    req = urllib.request.Request(st.SENTRY_BASE_URL + path,
                                 headers={"Authorization": "Bearer " + token})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return resp.status
    except urllib.error.HTTPError as exc:
        return exc.code


def main() -> int:
    token = powershell("Get-Clipboard -Raw").strip()
    if len(token) < 40 or any(c.isspace() for c in token):
        print("Panoda token yok — Sentry'de token'ı kopyalayıp tekrar çalıştırın.")
        return 1

    checks = {
        "proje": f"/projects/{st.SENTRY_ORG}/{st.SENTRY_PROJECT}/",
        "istatistik": f"/organizations/{st.SENTRY_ORG}/stats_v2/?field=sum(quantity)"
                      "&groupBy=outcome&category=error&statsPeriod=24h&interval=1h",
        "semboller": f"/projects/{st.SENTRY_ORG}/{st.SENTRY_PROJECT}/files/dsyms/",
    }
    for name, path in checks.items():
        code = status(path, token)
        print(f"  {name}: HTTP {code}")
        if code != 200:
            print(f"Token {st.SENTRY_ORG}/{st.SENTRY_PROJECT} için yetkisiz — hiçbir şey yazılmadı.")
            return 1

    lines = ENV.read_text(encoding="utf-8").splitlines() if ENV.exists() else []
    out = [line for line in lines if line.partition("=")[0].strip() != "SENTRY_AUTH_TOKEN"]
    out.append("SENTRY_AUTH_TOKEN=" + token)
    tmp = ENV.with_name(".env.tmp")
    tmp.write_text("\n".join(out) + "\n", encoding="utf-8")
    os.replace(tmp, ENV)
    print("  .env: SENTRY_AUTH_TOKEN güncellendi")

    r = subprocess.run(["gh", "secret", "set", "SENTRY_AUTH_TOKEN", "-R", REPO],
                       input=token, text=True, capture_output=True)
    print("  GitHub secret:", "güncellendi" if r.returncode == 0 else r.stderr.strip()[:200])

    powershell("Set-Clipboard -Value ' '")
    print("  Pano temizlendi. Doğrulama: just telemetry-watch")
    return 0 if r.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
