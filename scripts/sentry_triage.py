#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
sentry_triage.py -- Turn the Sentry project into a ranked work list.

The telemetry answers two questions the project exists to answer:
  * which operation fails most, and
  * which game fails most.

Reading that off the Sentry web UI means remembering to look, remembering the
right filters, and eyeballing counts. This does it from the API instead, so
the answer is a command rather than a habit.

Beyond the ranking it flags the three states that are easy to miss by eye:
  * regressions   — a resolved issue still collecting events
  * new fatals    — crashes that appeared since the last run
  * dead alerts   — alert rules with no action (they notify nobody)

Usage:
    python scripts/sentry_triage.py                 # full report
    python scripts/sentry_triage.py --json          # machine-readable
    python scripts/sentry_triage.py --fail-on-regression   # non-zero exit for CI
"""

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections import Counter
from pathlib import Path

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

# The org's region (organization.links.regionUrl); sentry.io proxies most of
# these, but the region host is the documented one for region endpoints.
SENTRY_BASE_URL = "https://de.sentry.io/api/0"
SENTRY_ORG = "makineceviri"
SENTRY_PROJECT = "makine-launcher"

# Failure texts that identify a missing install handler. These are the product
# signal buried in the noise: every event here is a user who wanted a patch the
# launcher cannot apply yet, so the counts are a demand ranking for what to
# build next rather than a list of defects to fix.
HANDLER_PATTERNS = [
    (".forge injection", r"\.forge"),
    ("script install", r"kurulum y[öo]ntemi: script"),
    ("paradox mod", r"[Pp]aradox modu"),
    ("external tool", r"kurulum y[öo]ntemi: external"),
]


def load_token() -> str:
    """Read SENTRY_AUTH_TOKEN from the environment, falling back to .env.

    Same reason the CMake side reads .env directly: only `just` loads that
    file, so a bare `python scripts/...` run would otherwise fail with a
    confusing auth error.
    """
    token = os.environ.get("SENTRY_AUTH_TOKEN", "")
    if token:
        return token
    env_path = Path(__file__).parent.parent / ".env"
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
        with urllib.request.urlopen(request, timeout=45) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")[:200]
        return {"__error__": f"HTTP {exc.code}: {body}"}
    except Exception as exc:  # noqa: BLE001 - network layer, any failure is fatal here
        return {"__error__": f"{type(exc).__name__}: {exc}"}


def fetch_issues(period: str) -> list[dict]:
    """Open production issues seen in `period`, most users first.

    The org endpoint, not the project one: that only accepts '', '24h' or
    '14d' and, with '', ranked all-time counts — issues fixed months ago and
    dev-build self-tests sat at the top of the list.
    """
    pid = fetch_project_id()
    if not pid:
        print("ERROR: project id unavailable", file=sys.stderr)
        return []
    issues = api(
        f"/organizations/{SENTRY_ORG}/issues/",
        f"?project={pid}&environment=production&query=is%3Aunresolved"
        f"&sort=user&limit=100&statsPeriod={period}",
    )
    if isinstance(issues, dict):
        print(f"ERROR: issue list unavailable: {issues.get('__error__')}", file=sys.stderr)
        return []
    return issues


def fetch_tag_values(tag: str) -> list[dict]:
    values = api(f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/tags/{tag}/values/")
    return [] if isinstance(values, dict) else values


def issue_top_tag(issue_id: str, tag: str) -> str:
    """The most frequent value of `tag` among an issue's events ('' when none)."""
    values = api(f"/organizations/{SENTRY_ORG}/issues/{issue_id}/tags/{tag}/values/")
    if isinstance(values, dict) or not values:
        return ""
    return str(max(values, key=lambda v: as_int(v.get("count"))).get("value") or "")


def fetch_project_id() -> str:
    """Numeric project id — the detectors endpoint filters by it, not the slug."""
    data = api(f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/")
    if isinstance(data, dict) and "__error__" not in data:
        return str(data.get("id", ""))
    return ""


def analyze_alerts() -> dict:
    """Which alerts actually notify someone — via the workflow engine.

    Sentry retired the per-project `/rules/` endpoint (it now 404s) and moved
    alerting to org-level *workflows* fired by project *detectors*. The old
    check turned that 404 into `[]` and silently passed, so a project whose
    crash alert had lost its action would look healthy. This reads the real
    graph instead and reports two failure modes:
      * an enabled workflow wired to one of this project's detectors but with
        no action — it triggers and notifies nobody;
      * the project's issue_stream detector (the one that fires on new issues)
        with no workflow attached at all — new crashes alert nobody.
    On any API error it returns the error rather than an empty, healthy-looking
    result.
    """
    pid = fetch_project_id()
    if not pid:
        return {"error": "proje kimliği alınamadı (yetki/ağ?)", "dead": [], "orphan_detectors": []}

    detectors = api(f"/organizations/{SENTRY_ORG}/detectors/", f"?project={pid}")
    workflows = api(f"/organizations/{SENTRY_ORG}/workflows/")
    errors = [d.get("__error__") for d in (detectors, workflows) if isinstance(d, dict)]
    if errors:
        return {"error": "; ".join(e for e in errors if e), "dead": [], "orphan_detectors": []}

    det_ids = {str(d.get("id")) for d in detectors}
    # issue_stream detectors with no workflow — new issues alert nobody.
    orphan = [d.get("name") or d.get("id") for d in detectors
              if d.get("type") == "issue_stream" and not d.get("workflowIds")]

    dead = []
    for w in workflows:
        if not w.get("enabled"):
            continue
        if not ({str(x) for x in (w.get("detectorIds") or [])} & det_ids):
            continue  # not connected to this project
        actions = [a for af in (w.get("actionFilters") or []) for a in (af.get("actions") or [])]
        if not actions:
            dead.append(w.get("name") or str(w.get("id")))

    return {"error": None, "dead": dead, "orphan_detectors": orphan}


def as_int(value) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return 0


def build_report(period: str = "14d") -> dict:
    issues = fetch_issues(period)

    # Sentry reopens a resolved issue that receives a newer event and marks it
    # "regressed". (This used to test status=resolved with count > 0, which is
    # every resolved issue there is — its old events are its count.)
    regressions = [i for i in issues if i.get("substatus") == "regressed"]
    fatals = [i for i in issues if i.get("level") == "fatal"]
    unresolved_fatals = [i for i in fatals if i.get("status") == "unresolved"]

    # Handler demand: group failure titles by the missing capability they name.
    handler_demand: Counter = Counter()
    handler_games: dict[str, set] = {}
    for issue in issues:
        title = issue.get("title") or ""
        for label, pattern in HANDLER_PATTERNS:
            if re.search(pattern, title):
                handler_demand[label] += as_int(issue.get("count"))
                subject = re.search(r"\[(.+?)\]", title)
                handler_games.setdefault(label, set()).add(
                    subject.group(1) if subject else "?"
                )
                break

    # Recipe failures titled only "N adımda hata oluştu" (until the failed step went into
    # the title). Their events still say why in fail.reason and fail.step, so those are
    # read here; only the ones without a reason are truly opaque.
    stepped = [
        i for i in issues
        if re.search(r"\d+(/\d+)? ad[ıi]mda hata olu[şs]tu$", (i.get("title") or "").strip())
    ]
    for i in stepped:
        i["_reason"] = issue_top_tag(i.get("id"), "fail.reason")
        i["_step"] = issue_top_tag(i.get("id"), "fail.step")
    opaque = [i for i in stepped if i["_reason"] in ("", "other", "step_failed")]

    alerts = analyze_alerts()

    operations = {v.get("value"): as_int(v.get("count")) for v in fetch_tag_values("operation")}
    sides = {v.get("value"): as_int(v.get("count")) for v in fetch_tag_values("failure.side")}
    games = {v.get("value"): as_int(v.get("count")) for v in fetch_tag_values("game.name")}

    return {
        "period": period,
        "issue_count": len(issues),
        "event_total": sum(as_int(i.get("count")) for i in issues),
        "operations": operations,
        "sides": sides,
        "top_games": dict(sorted(games.items(), key=lambda kv: -kv[1])[:10]),
        "handler_demand": {
            label: {"events": count, "games": sorted(handler_games.get(label, []))[:12]}
            for label, count in handler_demand.most_common()
        },
        "regressions": [
            {
                "id": i.get("id"),
                "title": (i.get("title") or "")[:90],
                "events": as_int(i.get("count")),
                "last_seen": str(i.get("lastSeen"))[:10],
            }
            for i in regressions
        ],
        "unresolved_fatals": [
            {
                "id": i.get("id"),
                "title": (i.get("title") or "")[:90],
                "events": as_int(i.get("count")),
                "users": as_int(i.get("userCount")),
                "last_seen": str(i.get("lastSeen"))[:10],
            }
            for i in unresolved_fatals
        ],
        "opaque_issues": [
            {
                "title": (i.get("title") or "")[:90],
                "events": as_int(i.get("count")),
                "users": as_int(i.get("userCount")),
            }
            for i in opaque
        ],
        "step_failures": [
            {
                "title": (i.get("title") or "")[:90],
                "events": as_int(i.get("count")),
                "users": as_int(i.get("userCount")),
                "reason": i["_reason"],
                "step": i["_step"],
            }
            for i in stepped if i not in opaque
        ],
        "dead_alert_rules": alerts["dead"],
        "orphan_detectors": alerts["orphan_detectors"],
        "alert_error": alerts["error"],
        "top_issues": [
            {
                "id": i.get("id"),
                "title": (i.get("title") or "")[:100],
                "events": as_int(i.get("count")),
                "users": as_int(i.get("userCount")),
                "level": i.get("level"),
                "status": i.get("status"),
            }
            for i in issues[:15]
        ],
    }


def print_report(report: dict) -> None:
    line = "=" * 78

    print(line)
    print(f"  SENTRY TRIAGE — {SENTRY_ORG}/{SENTRY_PROJECT}")
    print(line)
    print(f"  production, son {report['period']}: {report['issue_count']} açık issue · "
          f"{report['event_total']} olay (kullanıcı sayısına göre)")

    sides = report["sides"]
    system, user = sides.get("system", 0), sides.get("user", 0)
    total = system + user
    if total:
        print(f"  failure.side: system={system} ({system * 100 // total}%) · user={user}")

    print(f"\n  Operasyon dagilimi:")
    for op, count in sorted(report["operations"].items(), key=lambda kv: -kv[1]):
        print(f"    {op:22s} {count:>5d}")

    if report.get("alert_error"):
        print(f"\n  {line[:40]}")
        print("  !! ALARM DURUMU OKUNAMADI — saglikli sayilmaz")
        print(f"    {report['alert_error']}")
    if report["dead_alert_rules"]:
        print(f"\n  {line[:40]}")
        print("  !! BILDIRIMSIZ ALARM — eylemi olmayan is akislari")
        for name in report["dead_alert_rules"]:
            print(f"    - {name} — tetiklense de kimseye ulasmiyor")
        print("     Duzeltme: Sentry > Alerts > Workflows (eyleme e-posta/entegrasyon ekle)")
    if report.get("orphan_detectors"):
        print(f"\n  {line[:40]}")
        print("  !! IS AKISI OLMAYAN DEDEKTOR — yeni sorunlar kimseyi uyarmiyor")
        for name in report["orphan_detectors"]:
            print(f"    - {name} — hicbir is akisina bagli degil")

    if report["handler_demand"]:
        print(f"\n  Eksik handler talebi (olay sayisina gore):")
        for label, data in report["handler_demand"].items():
            print(f"    {label:20s} {data['events']:>5d} olay · "
                  f"{len(data['games'])} oyun")
            for game in data["games"][:5]:
                print(f"        {game}")

    if report["regressions"]:
        print(f"\n  !! REGRESYON — kapatilmis ama olay almaya devam eden issue:")
        for r in report["regressions"]:
            print(f"    [{r['id']}] {r['events']:>4d} olay · son {r['last_seen']} · {r['title']}")

    if report["unresolved_fatals"]:
        print(f"\n  Acik fatal (cokme):")
        for f in report["unresolved_fatals"]:
            print(f"    [{f['id']}] {f['events']:>4d} olay · {f['users']} kullanici · "
                  f"son {f['last_seen']} · {f['title']}")

    if report.get("step_failures"):
        print(f"\n  Tarif adimi hatalari (basligi soylemiyor, etiketi soyluyor):")
        for o in report["step_failures"]:
            print(f"    {o['events']:>4d} olay · {o['users']} kullanici · "
                  f"{o['reason']} / {o['step'] or '-'} · {o['title']}")
    if report["opaque_issues"]:
        print(f"\n  Teshis edilemeyen mesajlar (Sentry'ye ulasiyor, is yaramiyor):")
        for o in report["opaque_issues"]:
            print(f"    {o['events']:>4d} olay · {o['users']} kullanici · {o['title']}")

    print(f"\n  En çok kullanıcıya dokunan 15 issue:")
    for i in report["top_issues"]:
        flag = "*" if i["status"] == "resolved" else " "
        print(f"  {flag} {i['events']:>4d}x {i['users']:>3d}usr {i['level']:<8s} {i['title']}")

    print()
    print(line)


def main() -> None:
    parser = argparse.ArgumentParser(description="Ranked Sentry triage report")
    parser.add_argument("--json", action="store_true", help="Machine-readable output")
    parser.add_argument("--period", default="14d",
                        help="Window for open production issues (24h, 14d, 30d, 90d; default 14d)")
    parser.add_argument(
        "--fail-on-regression",
        action="store_true",
        help="Exit non-zero when a resolved issue is still collecting events",
    )
    parser.add_argument(
        "--fail-on-dead-alert",
        action="store_true",
        help="Exit non-zero when an alert rule has no action",
    )
    args = parser.parse_args()

    if not TOKEN:
        print("ERROR: SENTRY_AUTH_TOKEN not set (check .env or environment)", file=sys.stderr)
        sys.exit(1)

    report = build_report(args.period)

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print_report(report)

    if args.fail_on_regression and report["regressions"]:
        sys.exit(2)
    # A read error means we could not confirm the alerts are live — with the
    # gate on, that is a failure, not a pass.
    if args.fail_on_dead_alert and (report["dead_alert_rules"]
                                    or report.get("orphan_detectors")
                                    or report.get("alert_error")):
        sys.exit(3)


if __name__ == "__main__":
    main()
