#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
telemetry_watchdog.py -- Fail loudly when either telemetry channel goes blind.

Both channels went dark without anyone noticing:
  * Sentry's 5,000-event monthly quota filled 6-9 days after each reset; in the
    30 days to 2026-09-23 it dropped 6,592 of 11,617 events and 37 of 61 days
    had no data at all. Nothing alerted — dropped events are not issues.
  * The counting endpoint (/api/v2/telemetry) had been writing nothing since
    March: requests were routed to a handler that read the wrong fields.

This checks the two numbers that would have shown it, and exits non-zero:
  1. Sentry outcomes for the last 24 h — any `rate_limited` means events are
     being thrown away right now.
  2. The counting channel's /health — fewer than --min-24h records in 24 h
     means the pipeline (or the launcher side of it) is dead.

Runs daily from .github/workflows/telemetry-watchdog.yml, so a failure arrives
as a GitHub notification instead of waiting for someone to look.

Usage:
    python scripts/telemetry_watchdog.py
    python scripts/telemetry_watchdog.py --min-24h 50
"""

import argparse
import json
import sys
import urllib.error
import urllib.request

import sentry_triage as st

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

HEALTH_URL = "https://makineceviri.org/api/v2/telemetry/health"
# Cloudflare's WAF answers python-urllib's default User-Agent with 403.
USER_AGENT = "makine-telemetry-watchdog/1.0"


def sentry_outcomes_24h(token: str) -> dict:
    url = (f"{st.SENTRY_BASE_URL}/organizations/{st.SENTRY_ORG}/stats_v2/"
           "?field=sum(quantity)&groupBy=outcome&category=error&statsPeriod=24h&interval=1h")
    req = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = json.load(resp)
    return {g["by"]["outcome"]: g["totals"]["sum(quantity)"] for g in data.get("groups", [])}


def counting_health() -> dict:
    req = urllib.request.Request(HEALTH_URL, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def main() -> int:
    parser = argparse.ArgumentParser(description="Fail when a telemetry channel goes blind")
    parser.add_argument("--min-24h", type=int, default=10,
                        help="Minimum counting-channel records expected in 24 h (default 10)")
    args = parser.parse_args()

    failures = []

    token = st.load_token()
    if not token:
        failures.append("Sentry: SENTRY_AUTH_TOKEN yok — kontrol edilemedi")
    else:
        try:
            out = sentry_outcomes_24h(token)
            accepted = out.get("accepted", 0)
            dropped = out.get("rate_limited", 0)
            print(f"Sentry (24 sa): kabul {accepted} · kota yüzünden düşen {dropped} · "
                  f"filtrelenen {out.get('filtered', 0)}")
            if dropped > 0:
                failures.append(f"Sentry son 24 saatte {dropped} olayı kota yüzünden düşürdü — "
                                "şu an kör")
        except (urllib.error.URLError, KeyError, ValueError) as exc:
            failures.append(f"Sentry istatistiği okunamadı: {exc}")

    try:
        health = counting_health()
        last24h = int(health.get("last24h", 0))
        print(f"Sayım kanalı (24 sa): {last24h} kayıt · son kayıt {health.get('lastReceivedAt')}")
        for row in health.get("byVersion24h", [])[:5]:
            print(f"    {row.get('version') or '-'}: {row.get('n')}")
        if not health.get("ok"):
            failures.append("Sayım kanalı sağlık ucu ok=false döndü")
        elif last24h < args.min_24h:
            failures.append(f"Sayım kanalı son 24 saatte yalnız {last24h} kayıt aldı "
                            f"(beklenen ≥ {args.min_24h}) — hat ölü olabilir")
    except (urllib.error.URLError, ValueError) as exc:
        failures.append(f"Sayım kanalı sağlık ucu okunamadı: {exc}")

    if failures:
        print("\nKÖR NOKTA:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\nİki kanal da görüyor.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
