#!/usr/bin/env python3
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

try:
    import requests
except ImportError:
    print("ERROR: requests not installed. Run: pip install requests", file=sys.stderr)
    sys.exit(1)

SENTRY_BASE_URL = "https://sentry.io/api/0"
SENTRY_ORG = "makine-ceviri"
SENTRY_PROJECT = "makineai"
GITHUB_ORG = "MakineCeviri"
GITHUB_REPO = "MakineAI"


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


def setup_alert_rules(dry_run: bool = False) -> bool:
    """Create alert rules for crash → GitHub issue."""
    print("\n  Setting up alert rules...")

    # List existing rules
    r = requests.get(
        f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/",
        headers=sentry_headers(), timeout=10
    )
    existing_rules = r.json() if r.status_code == 200 else []
    existing_names = {rule.get("name", "") for rule in existing_rules}

    rules_to_create = []

    # Rule 1: New crash → GitHub issue
    if "New Crash → GitHub Issue" not in existing_names:
        rules_to_create.append({
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
            "actions": [
                {"id": "sentry.rules.actions.notify_event.NotifyEventAction"}
            ],
            "frequency": 1440,  # Once per day per issue
        })
    else:
        print("  Rule 'New Crash → GitHub Issue' already exists")

    # Rule 2: Regression → Notify
    if "Regression Detected" not in existing_names:
        rules_to_create.append({
            "name": "Regression Detected",
            "actionMatch": "all",
            "filterMatch": "all",
            "conditions": [
                {"id": "sentry.rules.conditions.regression_event.RegressionEventCondition"}
            ],
            "actions": [
                {"id": "sentry.rules.actions.notify_event.NotifyEventAction"}
            ],
            "frequency": 30,
        })
    else:
        print("  Rule 'Regression Detected' already exists")

    if not rules_to_create:
        print("  All alert rules already configured")
        return True

    if dry_run:
        for rule in rules_to_create:
            print(f"  [DRY RUN] Would create rule: {rule['name']}")
        return True

    success = True
    for rule in rules_to_create:
        r = requests.post(
            f"{SENTRY_BASE_URL}/projects/{SENTRY_ORG}/{SENTRY_PROJECT}/rules/",
            headers=sentry_headers(), json=rule, timeout=10
        )
        if r.status_code in (200, 201):
            print(f"  ✓ Alert rule created: {rule['name']}")
        else:
            print(f"  WARNING: Failed to create '{rule['name']}': {r.status_code} {r.text[:200]}")
            success = False

    return success


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
    setup_alert_rules(args.dry_run)

    print("\n" + "=" * 70)
    print("  Setup complete!")
    print("=" * 70)


if __name__ == "__main__":
    main()
