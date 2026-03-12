#!/usr/bin/env python3
"""
deploy.py -- Full deployment orchestrator for MakineAI translation packages.

Single command to:
  1. Run compression + encryption pipeline (package_pipeline.py)
  2. Upload .makine files to Cloudflare R2 (r2_upload.py)
  3. Upload updated manifests to R2 (wrangler)
  4. Create Sentry release + associate commits (sentry-cli)
  5. Upload debug symbols for stack trace symbolication (sentry-cli)

Usage:
    python scripts/deploy.py                     # Full deploy (all packages)
    python scripts/deploy.py --app-id 1716740    # Deploy single package
    python scripts/deploy.py --skip-pipeline     # Skip pipeline, upload existing
    python scripts/deploy.py --skip-upload       # Skip upload, update manifests only
    python scripts/deploy.py --dry-run           # Preview everything

Prerequisites:
    pip install zstandard cryptography boto3
    CLOUDFLARE_API_TOKEN env var (see .env)
    SENTRY_AUTH_TOKEN env var (see .env, optional)
    sentry-cli (pip install sentry-cli, optional)
"""

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent
NPX = "npx.cmd" if sys.platform == "win32" else "npx"
ASSETS_DIR = Path("C:/cedra/MakineAI-Assets")  # Local manifest cache
BUILD_DIR = Path("C:/cedra/MakineAI-Assets-Build/data")
R2_BUCKET = "makineai-translations"
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


def upload_manifests_to_r2(dry_run: bool = False) -> bool:
    """Upload index.json and packages/*.json to R2 via wrangler."""
    print()
    print("=" * 70)
    print("  STEP: Upload Manifests to R2")
    print("=" * 70)

    if not ASSETS_DIR.exists():
        print(f"  ERROR: Assets directory not found: {ASSETS_DIR}")
        return False

    if dry_run:
        index = ASSETS_DIR / "index.json"
        pkgs = list((ASSETS_DIR / "packages").glob("*.json"))
        print(f"  [DRY RUN] Would upload index.json + {len(pkgs)} package files")
        return True

    if not os.environ.get("CLOUDFLARE_API_TOKEN"):
        print("  ERROR: CLOUDFLARE_API_TOKEN not set (check .env or environment)")
        return False

    uploaded = 0
    errors = 0

    # Upload index.json
    index_path = ASSETS_DIR / "index.json"
    if index_path.exists():
        r = subprocess.run(
            [NPX, "wrangler", "r2", "object", "put",
             f"{R2_BUCKET}/assets/index.json",
             f"--file={index_path}",
             "--content-type=application/json", "--remote"],
            capture_output=True, text=True
        )
        if r.returncode == 0:
            print("  ✓ index.json")
            uploaded += 1
        else:
            print(f"  ✗ index.json: {r.stderr[:100]}")
            errors += 1

    # Upload per-game JSONs
    packages_dir = ASSETS_DIR / "packages"
    if packages_dir.exists():
        pkg_files = sorted(packages_dir.glob("*.json"))
        for f in pkg_files:
            r = subprocess.run(
                [NPX, "wrangler", "r2", "object", "put",
                 f"{R2_BUCKET}/assets/packages/{f.name}",
                 f"--file={f}",
                 "--content-type=application/json", "--remote"],
                capture_output=True, text=True
            )
            if r.returncode == 0:
                uploaded += 1
            else:
                print(f"  ✗ {f.name}: {r.stderr[:80]}")
                errors += 1

        print(f"  ✓ {uploaded - 1} package files uploaded")

    print(f"\n  Total: {uploaded} uploaded, {errors} errors")
    return errors == 0


def get_app_version() -> str:
    """Read MAKINEAI_VERSION_FULL from top-level CMakeLists.txt."""
    cmake_path = PROJECT_DIR / "CMakeLists.txt"
    if not cmake_path.exists():
        return "0.1.0-alpha"

    version = ""
    suffix = ""
    for line in cmake_path.read_text().splitlines():
        stripped = line.strip()
        if stripped.startswith("project(") and "VERSION" in stripped:
            # project(MakineAI VERSION 0.1.0 ...)
            parts = stripped.split()
            for i, p in enumerate(parts):
                if p == "VERSION" and i + 1 < len(parts):
                    version = parts[i + 1].rstrip(")")
                    break
        if "MAKINEAI_VERSION_SUFFIX" in stripped and "set(" in stripped.lower():
            # set(MAKINEAI_VERSION_SUFFIX "pre-alpha")
            if '"' in stripped:
                suffix = stripped.split('"')[1]

    return f"{version}-{suffix}" if version and suffix else version or "0.1.0-alpha"


def sentry_release_tracking(release_version: str, dry_run: bool = False) -> bool:
    """Create Sentry release, associate commits, and finalize."""
    print()
    print("=" * 70)
    print("  STEP: Sentry Release Tracking")
    print("=" * 70)
    print(f"  Release: {release_version}")

    if not os.environ.get("SENTRY_AUTH_TOKEN"):
        print("  SKIP: SENTRY_AUTH_TOKEN not set")
        return True  # Non-fatal

    if dry_run:
        print("  [DRY RUN] Would create + finalize Sentry release")
        return True

    sentry_cli = "sentry-cli"

    # Create release
    r = subprocess.run(
        [sentry_cli, "releases", "new", release_version],
        capture_output=True, text=True, cwd=str(PROJECT_DIR)
    )
    if r.returncode != 0:
        print(f"  WARNING: sentry-cli releases new failed: {r.stderr[:200]}")
        print("  (Is sentry-cli installed? Run: pip install sentry-cli)")
        return True  # Non-fatal

    print(f"  ✓ Release created: {release_version}")

    # Associate commits (auto-detect from git)
    r = subprocess.run(
        [sentry_cli, "releases", "set-commits", release_version, "--auto"],
        capture_output=True, text=True, cwd=str(PROJECT_DIR)
    )
    if r.returncode == 0:
        print("  ✓ Commits associated")
    else:
        print(f"  WARNING: set-commits failed: {r.stderr[:150]}")

    # Finalize release
    r = subprocess.run(
        [sentry_cli, "releases", "finalize", release_version],
        capture_output=True, text=True, cwd=str(PROJECT_DIR)
    )
    if r.returncode == 0:
        print(f"  ✓ Release finalized")
    else:
        print(f"  WARNING: finalize failed: {r.stderr[:150]}")

    return True


def sentry_upload_debug_symbols(dry_run: bool = False) -> bool:
    """Upload debug symbols (DWARF/PDB) to Sentry for stack trace symbolication."""
    print()
    print("=" * 70)
    print("  STEP: Sentry Debug Symbol Upload")
    print("=" * 70)

    if not os.environ.get("SENTRY_AUTH_TOKEN"):
        print("  SKIP: SENTRY_AUTH_TOKEN not set")
        return True

    # Look for build output directory
    build_dir = PROJECT_DIR / "build" / "dev"
    if not build_dir.exists():
        build_dir = PROJECT_DIR / "build" / "release"
    if not build_dir.exists():
        print("  SKIP: No build directory found")
        return True

    if dry_run:
        print(f"  [DRY RUN] Would upload symbols from {build_dir}")
        return True

    r = subprocess.run(
        ["sentry-cli", "debug-files", "upload", "--include-sources", str(build_dir)],
        capture_output=True, text=True, cwd=str(PROJECT_DIR)
    )
    if r.returncode == 0:
        print(f"  ✓ Debug symbols uploaded from {build_dir}")
    else:
        print(f"  WARNING: symbol upload failed: {r.stderr[:200]}")

    return True


def load_env():
    """Load .env file into os.environ (key=value lines, no quoting)."""
    env_path = SCRIPT_DIR.parent / ".env"
    if not env_path.exists():
        return
    for line in env_path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key, value = key.strip(), value.strip()
        if key and key not in os.environ:
            os.environ[key] = value


def main():
    # Load .env early so all steps have access to tokens
    load_env()

    parser = argparse.ArgumentParser(
        description="Full deployment: pipeline → R2 upload → manifest sync"
    )
    parser.add_argument("--app-id", help="Deploy single package")
    parser.add_argument("--dry-run", action="store_true", help="Preview without changes")
    parser.add_argument("--skip-pipeline", action="store_true",
                        help="Skip compression/encryption (use existing .makine)")
    parser.add_argument("--skip-upload", action="store_true",
                        help="Skip R2 data upload")
    parser.add_argument("--skip-manifests", action="store_true",
                        help="Skip manifest upload to R2")
    parser.add_argument("--include-deferred", action="store_true",
                        help="Include deferred large packages")
    args = parser.parse_args()

    # Load R2 config for public URL
    r2_config_path = SCRIPT_DIR / "r2_config.json"
    r2_public_url = "https://cdn.makineceviri.net"
    if r2_config_path.exists():
        r2_config = json.loads(r2_config_path.read_text(encoding="utf-8"))
        r2_public_url = r2_config.get("public_url", r2_public_url)

    r2_data_url = f"{r2_public_url}/data"

    print("=" * 70)
    print("  MakineAI Translation Package Deployment")
    print("=" * 70)
    print(f"  Pipeline output: {BUILD_DIR}")
    print(f"  Manifests:       {ASSETS_DIR}")
    print(f"  R2 CDN:          {r2_public_url}")
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

    # ── Step 2: Upload .makine to R2 ──
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

        if run_step("Upload Packages to Cloudflare R2", upload_cmd, dry_run=False):
            steps_ok += 1
        else:
            print("\nUpload failed — manifests NOT updated.")
            sys.exit(1)

    # ── Step 3: Upload manifests to R2 ──
    if not args.skip_manifests:
        steps_total += 1
        if upload_manifests_to_r2(args.dry_run):
            steps_ok += 1
        else:
            print("\nManifest upload failed.")
            sys.exit(1)

    # ── Step 4: Sentry release tracking ──
    sentry_release = f"makineai@{get_app_version()}"
    steps_total += 1
    if sentry_release_tracking(sentry_release, args.dry_run):
        steps_ok += 1

    # ── Step 5: Upload debug symbols ──
    steps_total += 1
    if sentry_upload_debug_symbols(args.dry_run):
        steps_ok += 1

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
            print(f"  Packages: {r2_data_url}/<appId>.makine")
            print(f"  Catalog:  {r2_public_url}/assets/index.json")
    else:
        print(f"  WARNING: {steps_total - steps_ok} steps failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
