#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
sentry_setup.py -- One-time Sentry project configuration via API.

Sets up:
  1. GitHub integration (code mapping for stack traces)
  2. Alert rules (new crash → GitHub issue, regression → reopen)

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

SENTRY_BASE_URL = "https://sentry.io/api/0"
SENTRY_ORG = "makine-ceviri"
# Project slug as it exists in Sentry (see .sentryclirc). Not the repo name —
# pointing this at "makine-launcher" silently targets a project that does not
# exist, so every API call 404s and no rule is ever created.
SENTRY_PROJECT = "native"
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
    r = requests.get(f"{SENTRY_BASE_URL}/", headers=sentry_headers(), timeout=10)
    if r.status_code == 200:
        user = r.json().get("user", {}).get("username", "unknown")
        print(f"  Authenticated as: {user}")
        return True
    print(f"  ERROR: Auth failed (HTTP {r.status_code})")
    return False


def list_integrations() -> list:
    """List installed organization integrations."""
    r = requests.get(
        f"{SENTRY_BASE_URL}/organizations/{SENTRY_ORG}/integrations/",
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


# Every rule needs at least one action that actually reaches a person.
#
# The previous version used NotifyEventAction ("send a notification via legacy
# integrations"). Sentry accepts that id, stores the rule — and then drops the
# action, because no legacy integration is installed. The result was three
# active rules with `actions: []`: conditions evaluated, nothing was ever sent.
# A rule that fires into the void is worse than no rule, because the project
# looks monitored.
#
# Mail is the one channel that needs no integration, so it is the baseline.
NOTIFY_EMAIL = {
    "id": "sentry.mail.actions.NotifyEmailAction",
    "targetType": "IssueOwners",
    "fallthroughType": "ActiveMembers",
    "targetIdentifier": "",
}


def desired_rules() -> list[dict]:
    """The alert rules this project is supposed to have."""
    return [
        # New system-side defect appears. Level >= error already excludes the
        # user-actionable warnings (disk full, game running), so this fires on
        # things we have to fix.
        {
            "name": "New Crash → GitHub Issue",
            "actionMatch": "all",
            "filterMatch": "all",
            "conditions": [
                {"id": "sentry.rules.conditions.first_seen_event.FirstSeenEventCondition"}
            ],
            "filters": [
                {"id": "sentry.rules.filters.level.LevelFilter",
                 "match": "gte", "level": "40"}  # ERROR and above
            ],
            "actions": [NOTIFY_EMAIL],
            "frequency": 1440,  # Once per day per issue
        },
        # A resolved issue starts receiving events again. RtlpHpSegReAlloc was
        # closed and then collected 43 events over two months with nobody
        # informed — exactly what this is meant to catch.
        {
            "name": "Regression Detected",
            "actionMatch": "all",
            "filterMatch": "all",
            "conditions": [
                {"id": "sentry.rules.conditions.regression_event.RegressionEventCondition"}
            ],
            "filters": [],
            "actions": [NOTIFY_EMAIL],
            "frequency": 30,
        },
        # A defect reaching several users at once — a bad package or a broken
        # release — rather than one user's disk being full.
        #
        # The threshold is 3, not 10. With the beta's actual user count the
        # worst defect in the project (the .forge injection failure) peaked at
        # 8 unique users, so a 10-user gate would never have fired on the single
        # most reported problem we have.
        {
            "name": "Widespread Failure",
            "actionMatch": "all",
            "filterMatch": "all",
            "conditions": [
                {"id": "sentry.rules.conditions.event_frequency.EventUniqueUserFrequencyCondition",
                 "interval": "1h", "value": 3}
            ],
            "filters": [
                {"id": "sentry.rules.filters.tagged_event.TaggedEventFilter",
                 "key": "failure.side", "match": "eq", "value": "system"}
            ],
            "actions": [NOTIFY_EMAIL],
            "frequency": 60,
        },
    ]


def _rule_needs_repair(existing: dict, wanted: dict) -> list[str]:
    """Return the reasons `existing` diverges from `wanted` (empty = healthy)."""
    reasons = []

    if not existing.get("actions"):
        reasons.append("no actions (fires into the void)")
    elif not any(
        a.get("id") == NOTIFY_EMAIL["id"] for a in existing.get("actions", [])
    ):
        reasons.append("no mail action")

    if existing.get("status") != "active":
        reasons.append(f"status={existing.get('status')}")

    # Compare condition thresholds — the user-frequency gate is the one we tune.
    for want_c in wanted.get("conditions", []):
        match = next(
            (c for c in existing.get("conditions", []) if c.get("id") == want_c["id"]),
            None,
        )
        if match is None:
            reasons.append(f"missing condition {want_c['id'].rsplit('.', 1)[-1]}")
            continue
        for key in ("value", "interval"):
            if key in want_c and str(match.get(key)) != str(want_c[key]):
                reasons.append(f"{key}={match.get(key)} (want {want_c[key]})")

    return reasons


def setup_alert_rules(dry_run: bool = False) -> bool:
    """Create missing alert rules and repair existing ones that cannot notify.

    Idempotent by design: "create only if the name is absent" was not enough,
    because the rules existed and were still broken.
    """
    print("\n  Setting up alert rules...")

    r = requests.get(
        f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/",
        headers=sentry_headers(), timeout=10
    )
    existing_rules = r.json() if r.status_code == 200 else []
    by_name = {rule.get("name", ""): rule for rule in existing_rules}

    success = True
    for wanted in desired_rules():
        name = wanted["name"]
        existing = by_name.get(name)

        if existing is None:
            if dry_run:
                print(f"  [DRY RUN] Would create rule: {name}")
                continue
            resp = requests.post(
                f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/",
                headers=sentry_headers(), json=wanted, timeout=15
            )
            if resp.status_code in (200, 201):
                print(f"  + Alert rule created: {name}")
            else:
                print(f"  WARNING: create '{name}' failed: "
                      f"{resp.status_code} {resp.text[:200]}")
                success = False
            continue

        reasons = _rule_needs_repair(existing, wanted)
        if not reasons:
            print(f"  = Rule OK: {name}")
            continue

        print(f"  ! Rule broken: {name} -> {', '.join(reasons)}")
        if dry_run:
            print(f"  [DRY RUN] Would repair rule: {name}")
            continue

        payload = dict(wanted)
        payload["id"] = existing.get("id")
        resp = requests.put(
            f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/"
            f"{existing.get('id')}/",
            headers=sentry_headers(), json=payload, timeout=15
        )
        if resp.status_code in (200, 201, 202):
            print(f"  ~ Alert rule repaired: {name}")
        else:
            print(f"  WARNING: repair '{name}' failed: "
                  f"{resp.status_code} {resp.text[:200]}")
            success = False

    return success


def verify_alert_rules() -> bool:
    """Re-read the rules from Sentry and confirm each one can notify someone.

    Configuring is not the same as configured: the API accepted the old
    actions and silently discarded them. This reads the server's own view back
    and is the only thing that counts as proof.
    """
    print("\n  Verifying alert rules (server state)...")
    r = requests.get(
        f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/",
        headers=sentry_headers(), timeout=15
    )
    if r.status_code != 200:
        print(f"  ERROR: could not read rules back (HTTP {r.status_code})")
        return False

    ok = True
    for rule in r.json():
        actions = [a.get("id", "").rsplit(".", 1)[-1] for a in rule.get("actions", [])]
        if actions:
            print(f"  OK   {rule.get('name')}: {', '.join(actions)}")
        else:
            print(f"  DEAD {rule.get('name')}: no actions — nobody is notified")
            ok = False
    return ok


def main():
    parser = argparse.ArgumentParser(description="One-time Sentry project configuration")
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

    # Step 1: Auth check
    print("\n  Checking authentication...")
    if not check_auth():
        sys.exit(1)

    # Step 2: GitHub integration
    print("\n  Checking GitHub integration...")
    gh_integration = check_github_integration()
    if gh_integration:
        print(f"  ✓ GitHub integration found: {gh_integration.get('name', 'unknown')}")
        integration_id = gh_integration["id"]

        # Step 3: Code mapping
        setup_code_mapping(integration_id, args.dry_run)
    else:
        print("  WARNING: GitHub integration not installed")
        print("  → Install it at: https://makine-ceviri.sentry.io/settings/integrations/github/")
        print("  → Then re-run this script for code mappings")

    # Step 4: Alert rules
    configured = setup_alert_rules(args.dry_run)

    # Step 5: Read the result back from Sentry. The whole reason this script
    # needed fixing is that it reported success for rules the server had
    # quietly stripped, so "we sent the request" is not an acceptable outcome.
    verified = True
    if not args.dry_run:
        verified = verify_alert_rules()

    print("\n" + "=" * 70)
    if configured and verified:
        print("  Setup complete — every rule can reach someone.")
    else:
        print("  Setup INCOMPLETE — see warnings above.")
    print("=" * 70)

    # Non-zero exit so a pipeline or scheduled run fails loudly instead of
    # printing a warning nobody reads.
    if not (configured and verified):
        sys.exit(1)


if __name__ == "__main__":
    main()
