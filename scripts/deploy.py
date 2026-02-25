#!/usr/bin/env python3
"""
deploy.py -- Full deployment orchestrator for MakineAI translation packages.

Single command to:
  1. Run compression + encryption pipeline (package_pipeline.py)
  2. Upload .mkpkg files to Cloudflare R2 (r2_upload.py)
  3. Update manifest files with download URLs + sizes (--update-manifest)
  4. Push updated manifests to MakineAI-Assets repo

Usage:
    python scripts/deploy.py                     # Full deploy (all packages)
    python scripts/deploy.py --app-id 1716740    # Deploy single package
    python scripts/deploy.py --skip-pipeline     # Skip pipeline, upload existing
    python scripts/deploy.py --skip-upload       # Skip upload, update manifests only
    python scripts/deploy.py --dry-run           # Preview everything

Prerequisites:
    pip install zstandard cryptography boto3
    Configure scripts/r2_config.json with R2 credentials
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
ASSETS_REPO = Path("C:/cedra/MakineAI-Assets")
BUILD_DIR = Path("C:/cedra/MakineAI-Assets-Build/data")
PYTHON = sys.executable


def run_step(name: str, cmd: list[str], dry_run: bool = False) -> bool:
    """Run a deployment step and return success status."""
    print()
    print("=" * 70)
    print(f"  STEP: {name}")
    print("=" * 70)
    print(f"  Command: {' '.join(cmd)}")

    if dry_run:
        print("  [DRY RUN — skipped]")
        return True

    print()
    result = subprocess.run(cmd, cwd=str(SCRIPT_DIR.parent))
    if result.returncode != 0:
        print(f"\n  FAILED: {name} (exit code {result.returncode})")
        return False

    print(f"\n  OK: {name}")
    return True


def push_assets(dry_run: bool = False) -> bool:
    """Commit and push manifest changes to MakineAI-Assets repo."""
    print()
    print("=" * 70)
    print("  STEP: Push to MakineAI-Assets")
    print("=" * 70)

    if not ASSETS_REPO.exists():
        print(f"  ERROR: Assets repo not found: {ASSETS_REPO}")
        return False

    if dry_run:
        print("  [DRY RUN — skipped]")
        return True

    # Check for changes
    result = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=str(ASSETS_REPO),
        capture_output=True, text=True
    )

    if not result.stdout.strip():
        print("  No changes to push (manifests already up to date)")
        return True

    # Stage, commit, push
    timestamp = time.strftime("%Y-%m-%d %H:%M")
    steps = [
        (["git", "add", "index.json", "packages/"], "Stage manifest files"),
        (["git", "commit", "-m", f"chore: update manifests with R2 data URLs ({timestamp})"],
         "Commit"),
        (["git", "push"], "Push to remote"),
    ]

    for cmd, desc in steps:
        print(f"  {desc}...")
        r = subprocess.run(cmd, cwd=str(ASSETS_REPO))
        if r.returncode != 0:
            print(f"  FAILED: {desc}")
            return False

    print("  OK: Manifests pushed to MakineAI-Assets")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Full deployment: pipeline → R2 upload → manifest update → push"
    )
    parser.add_argument("--app-id", help="Deploy single package")
    parser.add_argument("--dry-run", action="store_true", help="Preview without changes")
    parser.add_argument("--skip-pipeline", action="store_true",
                        help="Skip compression/encryption (use existing .mkpkg)")
    parser.add_argument("--skip-upload", action="store_true",
                        help="Skip R2 upload")
    parser.add_argument("--skip-push", action="store_true",
                        help="Skip pushing manifests to MakineAI-Assets")
    parser.add_argument("--include-deferred", action="store_true",
                        help="Include deferred large packages")
    args = parser.parse_args()

    # Load R2 config for public URL
    r2_config_path = SCRIPT_DIR / "r2_config.json"
    r2_public_url = "https://pub-PLACEHOLDER.r2.dev"
    if r2_config_path.exists():
        r2_config = json.loads(r2_config_path.read_text(encoding="utf-8"))
        r2_public_url = r2_config.get("public_url", r2_public_url)

    r2_data_url = f"{r2_public_url}/data"

    print("=" * 70)
    print("  MakineAI Translation Package Deployment")
    print("=" * 70)
    print(f"  Pipeline output: {BUILD_DIR}")
    print(f"  Assets repo:     {ASSETS_REPO}")
    print(f"  R2 public URL:   {r2_data_url}")
    if args.app_id:
        print(f"  Single package:  {args.app_id}")
    if args.dry_run:
        print("  MODE: DRY RUN")

    t0 = time.monotonic()
    steps_ok = 0
    steps_total = 0

    # ── Step 1: Pipeline (compress + encrypt) ──
    if not args.skip_pipeline:
        steps_total += 1
        pipeline_cmd = [
            PYTHON, str(SCRIPT_DIR / "package_pipeline.py"),
            "--output", str(BUILD_DIR),
            "--update-manifest",
            "--r2-base-url", r2_data_url,
        ]
        if args.app_id:
            pipeline_cmd += ["--app-id", args.app_id]
        if args.include_deferred:
            pipeline_cmd += ["--include-deferred"]
        if args.dry_run:
            pipeline_cmd += ["--dry-run"]

        if run_step("Compression + Encryption Pipeline", pipeline_cmd, dry_run=False):
            steps_ok += 1
        else:
            print("\nPipeline failed — aborting deployment.")
            sys.exit(1)

    # ── Step 2: Upload to R2 ──
    if not args.skip_upload:
        steps_total += 1
        upload_cmd = [
            PYTHON, str(SCRIPT_DIR / "r2_upload.py"),
            "--data-dir", str(BUILD_DIR),
        ]
        if args.app_id:
            upload_cmd += ["--app-id", args.app_id]
        if args.dry_run:
            upload_cmd += ["--dry-run"]

        if run_step("Upload to Cloudflare R2", upload_cmd, dry_run=False):
            steps_ok += 1
        else:
            print("\nUpload failed — manifests NOT updated.")
            sys.exit(1)

    # ── Step 3: Push manifests ──
    if not args.skip_push:
        steps_total += 1
        if push_assets(args.dry_run):
            steps_ok += 1
        else:
            print("\nManifest push failed.")
            sys.exit(1)

    # ── Summary ──
    elapsed = time.monotonic() - t0
    print()
    print("=" * 70)
    print("  DEPLOYMENT COMPLETE")
    print("=" * 70)
    print(f"  Steps: {steps_ok}/{steps_total} succeeded")
    print(f"  Time:  {elapsed:.0f}s ({elapsed / 60:.1f} min)")
    print()

    if steps_ok == steps_total:
        print("  All steps passed!")
        if not args.dry_run:
            print(f"  Packages available at: {r2_data_url}/<appId>.mkpkg")
            print(f"  Manifests updated in:  {ASSETS_REPO}")
    else:
        print(f"  WARNING: {steps_total - steps_ok} steps failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
