#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
telemetry_selftest.py -- Send one event and prove it arrived, unredacted-free.

"It builds" has never been evidence that telemetry works. The DSN can be empty,
the transport can fail, the redaction can regress — all of it compiles, starts
and reports success. The only proof is an event that reaches Sentry and reads
the way it should.

This runs the launcher's --selftest-telemetry path, then polls the Sentry API
until the event shows up and checks it:

  * the event exists                      -> DSN, init and transport work
  * its body contains "[redacted]"        -> path redaction ran
  * it never contains the planted name    -> nothing leaked through
  * its breadcrumbs are redacted too      -> the gap found in the beta is closed

Requires a dev build (--selftest-telemetry only exists under MAKINE_DEV_TOOLS).

Usage:
    python scripts/telemetry_selftest.py
    python scripts/telemetry_selftest.py --exe build/dev/Makine-Launcher.exe
    python scripts/telemetry_selftest.py --skip-run   # only verify last event
    python scripts/telemetry_selftest.py --qml        # also the QML error path
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

ROOT = Path(__file__).parent.parent
SENTRY_BASE_URL = "https://sentry.io/api/0"
SENTRY_ORG = "makineceviri"
SENTRY_PROJECT = "makine-launcher"

# Planted in main.cpp's self-test block. If this string ever reaches Sentry,
# redaction is broken and real user names are leaking with it.
PLANTED_NAME = "selftest_user_name"
REDACTED = "[redacted]"


def load_token() -> str:
    token = os.environ.get("SENTRY_AUTH_TOKEN", "")
    if token:
        return token
    env_path = ROOT / ".env"
    if env_path.exists():
        for line in env_path.read_text(encoding="utf-8", errors="replace").splitlines():
            key, _, value = line.strip().partition("=")
            if key.strip() == "SENTRY_AUTH_TOKEN":
                return value.strip().strip("'\"")
    return ""


TOKEN = load_token()


def api(path: str, params: str = "") -> object:
    request = urllib.request.Request(
        f"{SENTRY_BASE_URL}{path}{params}",
        headers={"Authorization": f"Bearer {TOKEN}"},
    )
    try:
        with urllib.request.urlopen(request, timeout=40) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        return {"__error__": f"HTTP {exc.code}: {exc.read().decode('utf-8', 'replace')[:200]}"}
    except Exception as exc:  # noqa: BLE001
        return {"__error__": f"{type(exc).__name__}: {exc}"}


def find_selftest_issue(query: str = "selftest", marker: str = "selftest failed") -> dict | None:
    """Most recent issue whose title carries `marker` (the self-test path's)."""
    issues = api(
        f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/issues/",
        "?query=" + urllib.parse.quote(query) + "&sort=date&limit=10&statsPeriod=",
    )
    if isinstance(issues, dict):
        print(f"  ERROR: issue lookup failed: {issues.get('__error__')}")
        return None
    for issue in issues:
        if marker in (issue.get("title") or "").lower():
            return issue
    return None


# Optional paths (--qml): search words, the title marker of the event
# each one produces, and the environment variable that makes the dev exe do it.
EXTRA_PATHS = {
    "qml": ("qml error", "telemetry self-test qml error", "MAKINE_SELFTEST_QML"),
}


def wait_for_update(query: str, marker: str, before: str | None, timeout: int) -> dict | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidate = find_selftest_issue(query, marker)
        if candidate and candidate.get("lastSeen") != before:
            return candidate
        time.sleep(10)
    return None


def event_text(event: dict) -> tuple[str, str]:
    """Return (message body, breadcrumb text) for a Sentry event."""
    body = event.get("message") or ""
    crumbs: list[str] = []
    for entry in event.get("entries", []):
        if entry.get("type") == "message":
            body = entry.get("data", {}).get("formatted") or body
        elif entry.get("type") == "breadcrumbs":
            for crumb in entry.get("data", {}).get("values", []):
                crumbs.append(str(crumb.get("message") or ""))
    return str(body), "\n".join(crumbs)


# "Users\<name>" anywhere in the event that is neither the launcher's
# "[redacted]" nor the server-side backstop's "[scrubbed]". The body and
# breadcrumb checks above never looked at the module list, and that is where
# 0.1.6 leaked: every event carries the path of every loaded module, and a ZIP
# run from the Desktop is "C:\Users\<name>\Desktop\…\Makine-Launcher.exe".
USER_PATH = re.compile(r"(?i)users(?:\\\\|\\|/)+(?!\[redacted\]|\[scrubbed\])([^\\/\"'\s]+)")


def unredacted_user_paths(event: dict) -> list[str]:
    raw = json.dumps(event, ensure_ascii=False)
    # Shown with the name masked: the report itself must not repeat the leak.
    return [raw[max(0, m.start() - 40):m.start(1)] + m.group(1)[:2] + "***"
            for m in USER_PATH.finditer(raw)]


def runtime_env(exe: Path) -> dict:
    """Environment with the DLL directories the launcher needs to start.

    Read from the build tree's CMakeCache.txt rather than hardcoded: the
    project moved from Qt 6.10.1 to 6.11.1, and a stale path fails with exit
    code 3221225781 (STATUS_DLL_NOT_FOUND) — which looks like a crash, not
    like "wrong Qt directory". Deriving it from the build that produced the
    exe means this cannot drift again.
    """
    env = dict(os.environ)
    cache = exe.parent / "CMakeCache.txt"
    if not cache.exists():
        return env

    extra: list[str] = []
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        key, _, value = line.partition("=")
        value = value.strip()
        if key.startswith("CMAKE_PREFIX_PATH:") and value:
            extra.append(str(Path(value) / "bin"))          # Qt
        elif key.startswith("VCPKG_INSTALLED_DIR:") and value:
            extra.append(str(Path(value) / "x64-mingw-dynamic" / "bin"))
        elif key.startswith("CMAKE_CXX_COMPILER:") and value:
            extra.append(str(Path(value).parent))           # MinGW runtime

    env["PATH"] = os.pathsep.join(extra + [env.get("PATH", "")])
    return env


def run_selftest(exe: Path, extra_env: dict | None = None) -> bool:
    if not exe.exists():
        print(f"  ERROR: executable not found: {exe}")
        print("         Build a dev build first: just dev")
        return False
    print(f"  Running: {exe.name} --selftest-telemetry")
    env = runtime_env(exe)
    env.update(extra_env or {})
    env["QT_FORCE_STDERR_LOGGING"] = "1"   # the exe is a GUI-subsystem binary
    result = subprocess.run(
        [str(exe), "--selftest-telemetry"],
        capture_output=True, text=True, timeout=120, env=env,
    )
    # The same run also proves the counting channel (/api/v2/telemetry): the
    # exe exits 1 when that POST does not come back 2xx.
    for line in result.stderr.splitlines():
        if "counting channel" in line:
            print(f"  {line.strip()}")
    if result.returncode != 0:
        print(f"  ERROR: self-test exited {result.returncode} "
              "(Sentry dispatch or counting channel failed)")
        print(f"         {result.stderr[:300]}")
        return False
    print("  Self-test process exited cleanly")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="End-to-end telemetry verification")
    parser.add_argument(
        "--exe",
        default=str(ROOT / "build" / "dev" / "Makine-Launcher.exe"),
        help="Path to a dev build of the launcher",
    )
    parser.add_argument("--skip-run", action="store_true",
                        help="Verify the last self-test event without sending a new one")
    parser.add_argument("--timeout", type=int, default=180,
                        help="Seconds to wait for the event to appear (default 180)")
    parser.add_argument("--qml", action="store_true",
                        help="Also prove the QML script-error path (planted TypeError)")
    args = parser.parse_args()
    extras = [name for name in EXTRA_PATHS if getattr(args, name)]

    if not TOKEN:
        print("ERROR: SENTRY_AUTH_TOKEN not set (check .env or environment)", file=sys.stderr)
        sys.exit(1)

    print("=" * 78)
    print("  TELEMETRY SELF-TEST")
    print("=" * 78)

    before = None
    extra_before = {}
    if not args.skip_run:
        existing = find_selftest_issue()
        before = existing.get("lastSeen") if existing else None
        for name in extras:
            query, marker, _ = EXTRA_PATHS[name]
            seen = find_selftest_issue(query, marker)
            extra_before[name] = seen.get("lastSeen") if seen else None
        if not run_selftest(Path(args.exe), {EXTRA_PATHS[n][2]: "1" for n in extras}):
            sys.exit(1)

    print(f"\n  Waiting for the event to reach Sentry (max {args.timeout}s)...")
    issue = None
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        candidate = find_selftest_issue()
        if candidate and (args.skip_run or candidate.get("lastSeen") != before):
            issue = candidate
            break
        time.sleep(10)

    if issue is None:
        print("  FAIL: no self-test event arrived.")
        # An exhausted quota drops the event exactly like an empty DSN would, and
        # for most of every month that is the real cause — say which one it is.
        try:
            from telemetry_watchdog import sentry_outcomes_24h
            dropped = sentry_outcomes_24h(load_token()).get("rate_limited", 0)
        except Exception:  # diagnosis only; the FAIL stands either way
            dropped = 0
        if dropped:
            print(f"        Sentry dropped {dropped} events to quota in the last 24 h —")
            print("        the event was most likely rate limited, not lost to a bad DSN.")
        else:
            print("        The DSN is probably empty — check the configure output for")
            print("        'SENTRY DSN BULUNAMADI' and verify MAKINE_SENTRY_DSN in .env.")
        sys.exit(1)

    print(f"  Event arrived: issue {issue.get('id')} (events={issue.get('count')})")

    event = api(f"/organizations/{SENTRY_ORG}/issues/{issue.get('id')}/events/latest/")
    if isinstance(event, dict) and "__error__" in event:
        print(f"  FAIL: could not read the event back: {event['__error__']}")
        sys.exit(1)

    body, crumbs = event_text(event)
    leaks = unredacted_user_paths(event)
    checks = [
        ("message body redacted", REDACTED in body),
        ("message body has no raw user name", PLANTED_NAME not in body),
        ("breadcrumbs redacted", REDACTED in crumbs),
        ("breadcrumbs have no raw user name", PLANTED_NAME not in crumbs),
        ("no user path anywhere in the event (module list included)", not leaks),
    ]
    for name in extras:
        query, marker, _ = EXTRA_PATHS[name]
        if args.skip_run:
            arrived = find_selftest_issue(query, marker)
        else:
            print(f"  Waiting for the {name} event (max {args.timeout}s)...")
            arrived = wait_for_update(query, marker, extra_before.get(name), args.timeout)
        checks.append((f"{name} event arrived", arrived is not None))
        if arrived is not None:
            extra = api(f"/organizations/{SENTRY_ORG}/issues/{arrived.get('id')}/events/latest/")
            tags = {t.get("key"): t.get("value") for t in extra.get("tags", [])} \
                if isinstance(extra, dict) else {}
            checks.append((f"{name} event filed under development, operation={name}",
                           tags.get("environment") == "development"
                           and tags.get("operation") == name))
            checks.append((f"{name} event has no user path",
                           not unredacted_user_paths(extra if isinstance(extra, dict) else {})))

    print()
    failed = 0
    for label, ok in checks:
        print(f"  {'PASS' if ok else 'FAIL'}  {label}")
        if not ok:
            failed += 1

    if failed:
        for where in leaks[:5]:
            print(f"  leak: …{where}…")
        print("\n  --- message body ---")
        for line in body.splitlines():
            print(f"  | {line}")
        print("  --- breadcrumbs ---")
        for line in crumbs.splitlines()[-8:]:
            print(f"  | {line}")

    print()
    print("=" * 78)
    if failed:
        print(f"  TELEMETRY SELF-TEST FAILED ({failed} check(s))")
        print("=" * 78)
        sys.exit(1)
    print("  TELEMETRY VERIFIED — event delivered, paths redacted")
    print("=" * 78)


if __name__ == "__main__":
    main()
