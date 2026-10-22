#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
sentry_setup.py -- Sentry project configuration via API, read back from the server.

Sets up (idempotent — repairs what drifted, creates what is missing):
  1. GitHub integration (code mapping for stack traces; installing it is a browser step)
  2. Project settings: fingerprint rules (one issue per operation + reason + game),
     a server-side scrub of Windows account names, IP scrubbing
  3. Alert workflows (workflow engine): new crash, regression, widespread and
     spreading failures, crash spreading
  4. The crash-free session rate detector's thresholds

Prerequisites:
    SENTRY_AUTH_TOKEN env var (with org:admin, project:admin scopes)
    pip install requests

Usage:
    python scripts/sentry_setup.py              # Full setup
    python scripts/sentry_setup.py --dry-run    # Preview only
"""

import argparse
import json
import os
import sys
from pathlib import Path

# The Windows console defaults to cp1252, which cannot encode the check mark
# this script prints on success. That raised UnicodeEncodeError mid-run and
# killed the process right after the GitHub integration check — so the alert
# rule setup below never executed and nobody noticed, because the failure
# looked like a crash in an unrelated step.
for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

try:
    import requests
except ImportError:
    print("ERROR: requests not installed. Run: pip install requests", file=sys.stderr)
    sys.exit(1)

# The org lives in the EU region (organization.links.regionUrl). Integrations
# are a control-silo endpoint: the region host answers 404 there, which read
# as "not installed".
SENTRY_BASE_URL = "https://de.sentry.io/api/0"
SENTRY_CONTROL_URL = "https://sentry.io/api/0"
SENTRY_ORG = "makineceviri"
# Project slug as it exists in Sentry (see .sentryclirc). Not the repo name —
# pointing this at "makine-launcher" silently targets a project that does not
# exist, so every API call 404s and no rule is ever created.
SENTRY_PROJECT = "makine-launcher"
GITHUB_ORG = "MakineCeviri"
GITHUB_REPO = "Makine-Launcher"


def load_env():
    """Load .env if SENTRY_AUTH_TOKEN not already set."""
    if os.environ.get("SENTRY_AUTH_TOKEN"):
        return
    env_path = Path(__file__).parent.parent / ".env"
    if env_path.exists():
        for line in env_path.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            if key.strip() == "SENTRY_AUTH_TOKEN":
                os.environ["SENTRY_AUTH_TOKEN"] = value.strip()
                return


def sentry_headers() -> dict:
    token = os.environ.get("SENTRY_AUTH_TOKEN", "")
    return {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json",
    }


def check_auth() -> bool:
    """Verify Sentry auth token works."""
    # The region API has no root endpoint (404); reading the project proves
    # both the token and its access.
    r = requests.get(f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/",
                     headers=sentry_headers(), timeout=10)
    if r.status_code == 200:
        print(f"  Authenticated; project id {r.json().get('id')}")
        return True
    print(f"  ERROR: Auth failed (HTTP {r.status_code})")
    return False


def list_integrations() -> list:
    """List installed organization integrations."""
    r = requests.get(
        f"{SENTRY_CONTROL_URL}/organizations/{SENTRY_ORG}/integrations/",
        headers=sentry_headers(), timeout=10
    )
    if r.status_code == 200:
        return r.json()
    return []


def check_github_integration() -> dict | None:
    """Check if GitHub integration is already installed."""
    integrations = list_integrations()
    for i in integrations:
        if i.get("provider", {}).get("key") == "github":
            return i
    return None


def setup_code_mapping(integration_id: int, dry_run: bool = False) -> bool:
    """Create code mapping: stack prefix → repo path."""
    print("\n  Setting up code mappings...")

    # List existing code mappings
    r = requests.get(
        f"{SENTRY_BASE_URL}/organizations/{SENTRY_ORG}/code-mappings/",
        headers=sentry_headers(), timeout=10
    )
    existing = r.json() if r.status_code == 200 else []

    # Check if mapping already exists
    for m in existing:
        repo = m.get("repoName", "")
        if GITHUB_REPO in repo:
            print(f"  Code mapping already exists: {m.get('stackRoot', '')} → {m.get('sourceRoot', '')}")
            return True

    # Find repository ID
    r = requests.get(
        f"{SENTRY_BASE_URL}/organizations/{SENTRY_ORG}/repos/",
        headers=sentry_headers(), timeout=10
    )
    repos = r.json() if r.status_code == 200 else []
    repo_id = None
    for repo in repos:
        if GITHUB_REPO.lower() in repo.get("name", "").lower():
            repo_id = repo["id"]
            break

    if not repo_id:
        print(f"  WARNING: Repository {GITHUB_ORG}/{GITHUB_REPO} not found in Sentry")
        print("  (GitHub integration may need to be configured in Sentry dashboard first)")
        return False

    if dry_run:
        print(f"  [DRY RUN] Would create code mapping: qml/src/ → qml/src/")
        return True

    # Create code mapping
    mapping_data = {
        "repositoryId": repo_id,
        "projectId": None,  # Will be resolved from project slug
        "stackRoot": "qml/src/",
        "sourceRoot": "qml/src/",
        "defaultBranch": "main",
        "integrationId": integration_id,
    }

    # Get project ID
    r = requests.get(
        f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/",
        headers=sentry_headers(), timeout=10
    )
    if r.status_code == 200:
        mapping_data["projectId"] = r.json()["id"]

    r = requests.post(
        f"{SENTRY_BASE_URL}/organizations/{SENTRY_ORG}/code-mappings/",
        headers=sentry_headers(), json=mapping_data, timeout=10
    )
    if r.status_code in (200, 201):
        print("  ✓ Code mapping created: qml/src/ → qml/src/")
        return True
    else:
        print(f"  WARNING: Code mapping creation failed: {r.status_code} {r.text[:200]}")
        return False


def api(method: str, path: str, body=None) -> tuple[int, object]:
    """One API call; returns (status, json-or-text). Never raises on HTTP errors."""
    r = requests.request(method, f"{SENTRY_BASE_URL}{path}", headers=sentry_headers(),
                         json=body, timeout=30)
    try:
        data = r.json()
    except ValueError:
        data = r.text[:300]
    return r.status_code, data


# ============================================================================
# Project settings — grouping, scrubbing, privacy
# ============================================================================

# Launcher-reported failures carry fail.reason (failurereasons.h). Their
# default grouping is the message, and the message quotes the folder name of
# the user's copy: one Diablo II defect was eight issues in 30 days
# ("Diablo II: Resurrected", "... [FitGirl Repack]", "Diablo"…), so no issue
# ever reached the user-count alert thresholds. Keyed on operation + reason +
# game id instead.
#
# game.id is trustworthy on every release only for install/uninstall: 0.1.5
# and 0.1.6 wrote it into the session scope right before those reports, and
# other operations could inherit an earlier game. From 0.1.7 every failure
# carries its own (telemetryrules::failureTags), so the rest join in there.
# "other" keeps the default: an unclassified message is its own question.
FINGERPRINT_RULES = """\
# Managed by scripts/sentry_setup.py — edit there, not in the UI.
tags.fail.reason:other -> {{ default }}
tags.operation:install tags.fail.reason:* tags.game.id:* -> launcher-failure, install, {{ tags.fail.reason }}, {{ tags.game.id }}
tags.operation:uninstall tags.fail.reason:* tags.game.id:* -> launcher-failure, uninstall, {{ tags.fail.reason }}, {{ tags.game.id }}
!release:makine-launcher@0.1.5* !release:makine-launcher@0.1.6* tags.fail.reason:* tags.game.id:* -> launcher-failure, {{ tags.operation }}, {{ tags.fail.reason }}, {{ tags.game.id }}
"""

# Server-side backstop for the Windows account name. The launcher redacts
# paths itself (pathredaction.h), but two things never pass through it: the
# module list Sentry rebuilds from a crash minidump, and events from releases
# that predate a client fix (0.1.6 sent "C:\\Users\\<name>\\Desktop\\…" in every
# event's module list). Only the name is replaced, and with a different marker
# than the client's "[redacted]" — so telemetry_selftest.py still notices when
# the client stops redacting and the server is quietly covering for it.
# The name may not start with "[": an already-redacted path is left alone.
USERPATH_RULE = {
    "type": "pattern",
    "pattern": r"(?i)\busers[\\/]+([^\\/\[\]\"'\r\n][^\\/\"'\r\n]*)",
    "redaction": {"method": "replace", "text": "[scrubbed]"},
    "replaceGroups": [1],
}
PII_CONFIG = {
    "rules": {"0": USERPATH_RULE},
    # $string covers messages, breadcrumbs, tags, exception values. Module
    # paths are "maybe PII" in Relay's schema and only reached by name.
    "applications": {
        "$string": ["0"],
        "debug_meta.images.*.code_file": ["0"],
        "debug_meta.images.*.debug_file": ["0"],
    },
}

PROJECT_SETTINGS = {
    "fingerprintingRules": FINGERPRINT_RULES,
    "relayPiiConfig": json.dumps(PII_CONFIG, indent=2),
    "scrubIPAddresses": True,       # the org setting forces it too; keep both
    "dataScrubber": True,
    "dataScrubberDefaults": True,
}


def _same(key: str, have, want) -> bool:
    if key == "relayPiiConfig":
        try:
            return json.loads(have or "{}") == json.loads(want)
        except ValueError:
            return False
    if isinstance(want, str):
        return (have or "").strip() == want.strip()
    return have == want


def setup_project(dry_run: bool) -> bool:
    print("\n  Project settings (grouping, scrubbing, privacy)...")
    status, project = api("GET", f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/")
    if status != 200:
        print(f"  ERROR: project unreadable (HTTP {status})")
        return False
    changes = {k: v for k, v in PROJECT_SETTINGS.items() if not _same(k, project.get(k), v)}
    for key in PROJECT_SETTINGS:
        print(f"  {'~' if key in changes else '='} {key}")
    if not changes or dry_run:
        return True
    status, data = api("PUT", f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/", changes)
    if status != 200:
        print(f"  ERROR: update refused (HTTP {status}): {str(data)[:300]}")
        return False
    return True


def verify_project() -> bool:
    status, project = api("GET", f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/")
    if status != 200:
        print(f"  ERROR: project unreadable (HTTP {status})")
        return False
    ok = True
    for key, want in PROJECT_SETTINGS.items():
        good = _same(key, project.get(key), want)
        ok &= good
        print(f"  {'OK  ' if good else 'DIFF'} {key}")
    # Spike protection is on unless this option says otherwise.
    spike_off = (project.get("options") or {}).get("quotas:spike-protection-disabled")
    print(f"  {'OK  ' if not spike_off else 'DIFF'} spike protection on")
    return ok and not spike_off


# ============================================================================
# Alerts — Sentry's workflow engine
# ============================================================================
#
# The per-project /rules/ endpoint 404s since alerting moved to org-level
# workflows fired by project detectors; this script used to POST there, fail,
# and leave whatever the UI had. Every workflow below needs an action that
# reaches a person: mail needs no integration. (The old NotifyEventAction was
# accepted and silently dropped — three rules fired into the void for months.)

EMAIL = {
    "type": "email",
    "config": {"targetType": "issue_owners", "targetIdentifier": None, "targetDisplay": None},
    "data": {"fallthroughType": "ActiveMembers"},
}

# Only real users page anyone; dev builds and self-tests report "development".
ENVIRONMENT = "production"

SYSTEM_SIDE = {"type": "tagged_event", "comparison": {"key": "failure.side", "match": "eq",
                                                      "value": "system"}}
FATAL = {"type": "level", "comparison": {"level": 50, "match": "gte"}}


def _cond(c: dict) -> dict:
    return {"type": c["type"], "comparison": c["comparison"], "conditionResult": True}


def desired_workflows() -> list[dict]:
    return [
        # Every new crash signature. Not every new error: message issues were
        # ~4 new a day, and Sentry's own "high priority issues" workflow
        # already mails each new error-level issue — the two together sent
        # every new failure twice.
        {"name": "New crash (fatal) → Email",
         "legacy": ["New issue (error+) → Email", "New Crash → GitHub Issue"],
         "triggers": [{"type": "first_seen_event", "comparison": True}],
         "filters": [FATAL], "frequency": 30},
        # A resolved issue receives events again. RtlpHpSegReAlloc was closed
        # and collected 43 events over two months with nobody informed.
        {"name": "Regression Detected", "legacy": [],
         "triggers": [{"type": "regression_event", "comparison": True}],
         "filters": [], "frequency": 30},
        # A defect reaching several users at once — a bad package, a broken
        # release — not one user's full disk. 3, not 10: the worst defect of
        # the 0.1.2 beta peaked at 8 users an hour.
        {"name": "Widespread Failure", "legacy": [],
         "triggers": [{"type": "event_unique_user_frequency_count",
                       "comparison": {"value": 3, "interval": "1h"}}],
         "filters": [SYSTEM_SIDE], "frequency": 60},
        # The slow kind the hourly gate misses: Valhalla's backup refusal
        # reached ~30 users a day (326 in 11 days), about one an hour.
        {"name": "Spreading Failure (24h, 10+ users)", "legacy": [],
         "triggers": [{"type": "event_unique_user_frequency_count",
                       "comparison": {"value": 10, "interval": "1d"}}],
         "filters": [SYSTEM_SIDE], "frequency": 1440},
        # One crash signature hitting several users within an hour — a new
        # release crashing at start shows here before the 24 h crash-free
        # rate moves.
        {"name": "Crash Spreading (1h, 3+ users)", "legacy": [],
         "triggers": [{"type": "event_unique_user_frequency_count",
                       "comparison": {"value": 3, "interval": "1h"}}],
         "filters": [FATAL], "frequency": 60},
    ]


def _issue_stream_detector() -> str | None:
    status, project = api("GET", f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/")
    if status != 200:
        return None
    status, detectors = api("GET", f"/organizations/{SENTRY_ORG}/detectors/?project={project['id']}")
    if status != 200:
        return None
    for d in detectors:
        if d.get("type") == "issue_stream":
            return str(d["id"])
    return None


def _payload(wanted: dict, detector_ids: list[str]) -> dict:
    return {
        "name": wanted["name"],
        "enabled": True,
        "environment": ENVIRONMENT,
        "config": {"frequency": wanted["frequency"]},
        "triggers": {"logicType": "all", "conditions": [_cond(c) for c in wanted["triggers"]],
                     "actions": []},
        "actionFilters": [{"logicType": "all",
                           "conditions": [_cond(c) for c in wanted["filters"]],
                           "actions": [EMAIL]}],
        "detectorIds": detector_ids,
    }


def _diverges(existing: dict, wanted: dict, detector: str) -> list[str]:
    why = []
    if existing.get("name") != wanted["name"]:
        why.append("name")
    if not existing.get("enabled"):
        why.append("disabled")
    if existing.get("environment") != ENVIRONMENT:
        why.append(f"environment={existing.get('environment')}")
    if (existing.get("config") or {}).get("frequency") != wanted["frequency"]:
        why.append("frequency")
    if detector not in [str(x) for x in existing.get("detectorIds") or []]:
        why.append("not wired to this project")
    have_t = [(c.get("type"), c.get("comparison"))
              for c in (existing.get("triggers") or {}).get("conditions", [])]
    if have_t != [(c["type"], c["comparison"]) for c in wanted["triggers"]]:
        why.append("triggers")
    filters = existing.get("actionFilters") or [{}]
    have_f = [(c.get("type"), c.get("comparison")) for c in filters[0].get("conditions", [])]
    if have_f != [(c["type"], c["comparison"]) for c in wanted["filters"]]:
        why.append("filters")
    actions = [a for af in filters for a in af.get("actions") or []]
    if not any(a.get("type") == "email" for a in actions):
        why.append("no mail action")
    return why


def setup_workflows(dry_run: bool) -> bool:
    print("\n  Alert workflows...")
    detector = _issue_stream_detector()
    if not detector:
        print("  ERROR: this project's issue_stream detector was not found")
        return False
    status, workflows = api("GET", f"/organizations/{SENTRY_ORG}/workflows/")
    if status != 200:
        print(f"  ERROR: workflows unreadable (HTTP {status})")
        return False

    ok = True
    for wanted in desired_workflows():
        names = [wanted["name"]] + wanted["legacy"]
        existing = next((w for n in names for w in workflows if w.get("name") == n), None)
        if existing is None:
            print(f"  + create {wanted['name']}")
            if not dry_run:
                status, data = api("POST", f"/organizations/{SENTRY_ORG}/workflows/",
                                   _payload(wanted, [detector]))
                if status not in (200, 201):
                    print(f"    ERROR HTTP {status}: {str(data)[:300]}")
                    ok = False
            continue
        why = _diverges(existing, wanted, detector)
        if not why:
            print(f"  = {wanted['name']}")
            continue
        print(f"  ~ {existing.get('name')}: {', '.join(why)}")
        if dry_run:
            continue
        # Keep detectors of other projects (the web worker's) wired as they are.
        ids = sorted({str(x) for x in existing.get("detectorIds") or []} | {detector})
        status, data = api("PUT", f"/organizations/{SENTRY_ORG}/workflows/{existing['id']}/",
                           _payload(wanted, ids))
        if status != 200:
            print(f"    ERROR HTTP {status}: {str(data)[:300]}")
            ok = False
    return ok


# The crash-free session rate, production, 24 h window: base ~99.9 %,
# < 99.5 % medium, < 99 % high. The API cannot create metric detectors (POST
# answers 405), so a missing one is a browser step; thresholds are repaired.
CRASH_FREE_NAME = "Crash-free oturum oranı (24 sa)"
CRASH_FREE_AGGREGATE = "percentage(sessions_crashed, sessions) AS _crash_rate_alert_aggregate"
CRASH_FREE_CONDITIONS = [
    {"type": "lt", "comparison": 99, "conditionResult": 75},
    {"type": "lt", "comparison": 99.5, "conditionResult": 50},
    {"type": "gte", "comparison": 99.5, "conditionResult": 0},
]


def setup_crash_free(dry_run: bool) -> bool:
    print("\n  Crash-free rate detector...")
    status, project = api("GET", f"/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/")
    status, detectors = api("GET", f"/organizations/{SENTRY_ORG}/detectors/?project={project['id']}")
    if status != 200:
        print(f"  ERROR: detectors unreadable (HTTP {status})")
        return False
    det = next((d for d in detectors if d.get("type") == "metric_issue"
                and any(((s.get("queryObj") or {}).get("snubaQuery") or {}).get("aggregate")
                        == CRASH_FREE_AGGREGATE for s in d.get("dataSources") or [])), None)
    if det is None:
        print("  MISSING: create it in the browser — Alerts > Create Alert > Crash Free Session")
        print(f"           Rate, production, 24 h, warning < 99.5, critical < 99, name '{CRASH_FREE_NAME}'")
        return False
    query = det["dataSources"][0]["queryObj"]["snubaQuery"]
    ok = query.get("environment") == ENVIRONMENT and query.get("timeWindow") == 86400
    print(f"  {'=' if ok else '!'} {det['name']}: env={query.get('environment')} "
          f"window={query.get('timeWindow')}s")
    have = [{k: c[k] for k in ("type", "comparison", "conditionResult")}
            for c in (det.get("conditionGroup") or {}).get("conditions", [])]
    if have != CRASH_FREE_CONDITIONS and not dry_run:
        print("  ~ repairing thresholds")
        status, data = api("PUT", f"/organizations/{SENTRY_ORG}/detectors/{det['id']}/",
                           {"name": det["name"],
                            "conditionGroup": {"logicType": "any",
                                               "conditions": CRASH_FREE_CONDITIONS}})
        ok &= status == 200
    if not det.get("workflowIds"):
        print("  ! not wired to any workflow — connect it to a mail workflow in the browser")
        ok = False
    return ok


def verify_alerts() -> bool:
    """Re-read the alert graph from Sentry. Configured is not the same as
    configured-and-working: this is the only thing that counts as proof."""
    print("\n  Verifying alerts (server state)...")
    detector = _issue_stream_detector()
    status, workflows = api("GET", f"/organizations/{SENTRY_ORG}/workflows/")
    if status != 200 or not detector:
        print(f"  ERROR: could not read the alert graph back (HTTP {status})")
        return False
    ok = True
    by_name = {w.get("name"): w for w in workflows}
    for wanted in desired_workflows():
        w = by_name.get(wanted["name"])
        why = ["missing"] if w is None else _diverges(w, wanted, detector)
        ok &= not why
        print(f"  {'OK  ' if not why else 'BAD '} {wanted['name']}"
              + (f" — {', '.join(why)}" if why else ""))
    # Anything else wired to this project must at least be able to notify.
    for w in workflows:
        if w.get("name") in {d["name"] for d in desired_workflows()}:
            continue
        if detector in [str(x) for x in w.get("detectorIds") or []] and w.get("enabled"):
            actions = [a for af in w.get("actionFilters") or [] for a in af.get("actions") or []]
            print(f"  {'OK  ' if actions else 'DEAD'} {w.get('name')} (not managed here)")
            ok &= bool(actions)
    return ok


def main():
    parser = argparse.ArgumentParser(description="Sentry project configuration")
    parser.add_argument("--dry-run", action="store_true", help="Preview without changes")
    args = parser.parse_args()

    load_env()

    if not os.environ.get("SENTRY_AUTH_TOKEN"):
        print("ERROR: SENTRY_AUTH_TOKEN not set (check .env or environment)")
        sys.exit(1)

    print("=" * 70)
    print("  Sentry Project Setup")
    print("=" * 70)
    print(f"  Org:     {SENTRY_ORG}")
    print(f"  Project: {SENTRY_PROJECT}")
    print(f"  GitHub:  {GITHUB_ORG}/{GITHUB_REPO}")
    if args.dry_run:
        print("  MODE: DRY RUN")

    print("\n  Checking authentication...")
    if not check_auth():
        sys.exit(1)

    print("\n  Checking GitHub integration...")
    gh_integration = check_github_integration()
    if gh_integration:
        print(f"  ✓ GitHub integration found: {gh_integration.get('name', 'unknown')}")
        setup_code_mapping(gh_integration["id"], args.dry_run)
    else:
        # Not a failure of this script: installing it is an OAuth step in the
        # browser (owner).
        print("  WARNING: GitHub integration not installed")
        print("  → Install it at: https://makineceviri.sentry.io/settings/integrations/github/")

    configured = setup_project(args.dry_run)
    configured &= setup_workflows(args.dry_run)
    configured &= setup_crash_free(args.dry_run)

    # Read everything back. This script once reported success for rules the
    # server had quietly stripped; "we sent the request" is not an outcome.
    verified = True
    if not args.dry_run:
        print("\n  Verifying project settings (server state)...")
        verified = verify_project()
        verified &= verify_alerts()

    print("\n" + "=" * 70)
    if configured and verified:
        print("  Setup complete — read back from the server.")
    else:
        print("  Setup INCOMPLETE — see above.")
    print("=" * 70)
    if not (configured and verified):
        sys.exit(1)


if __name__ == "__main__":
    main()
