#!/usr/bin/env python3
"""
sign_packages.py -- Sign all .mkpkg packages on Cloudflare R2 with Ed25519.

Creates a .sig JSON file alongside each .mkpkg in the R2 bucket.
Supports two backends for R2 access:
  1. boto3 (S3-compatible) -- requires access_key_id/secret_access_key in r2_config.json
  2. wrangler (fallback)   -- uses wrangler OAuth credentials

Package enumeration uses the CDN index.json (source of truth for all packages).
Signature existence is checked via CDN HEAD requests (fast, no auth needed).

Usage:
    python scripts/sign_packages.py                # Sign all unsigned packages
    python scripts/sign_packages.py --dry-run      # Preview without signing
    python scripts/sign_packages.py --force        # Re-sign even if .sig exists
    python scripts/sign_packages.py --app-id 123   # Sign a single package
    python scripts/sign_packages.py --list         # List packages and signature status

Prerequisites:
    pip install cryptography requests boto3
"""

import argparse
import base64
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

import requests
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import load_pem_private_key

# ── Constants ──

SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent
CONFIG_PATH = SCRIPT_DIR / "r2_config.json"
PRIVATE_KEY_PATH = SCRIPT_DIR / "certs" / "signing_private.pem"
R2_PREFIX = "data/"
KEY_ID = "ed25519-prod-v1"
SIG_VERSION = 1
CDN_BASE_URL = "https://cdn.makineceviri.net"
CDN_INDEX_URL = f"{CDN_BASE_URL}/assets/index.json"
NPX = "npx.cmd" if sys.platform == "win32" else "npx"

HAS_BOTO3 = False
try:
    import boto3
    from botocore.config import Config as BotoConfig

    HAS_BOTO3 = True
except ImportError:
    pass


# ── Environment ──


def load_env() -> None:
    """Load .env file into os.environ (key=value lines)."""
    env_path = PROJECT_DIR / ".env"
    if not env_path.exists():
        return
    for line in env_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key, value = key.strip(), value.strip()
        if key and key not in os.environ:
            os.environ[key] = value


# ── R2 Config ──


def load_config() -> dict:
    """Load R2 config from r2_config.json and determine backend."""
    if not CONFIG_PATH.exists():
        print(f"ERROR: R2 config not found: {CONFIG_PATH}", file=sys.stderr)
        sys.exit(1)

    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))

    if not config.get("bucket_name") or config["bucket_name"] == "PLACEHOLDER":
        print(f"ERROR: bucket_name not configured in {CONFIG_PATH}", file=sys.stderr)
        sys.exit(1)

    has_s3 = (
        config.get("access_key_id", "PLACEHOLDER") != "PLACEHOLDER"
        and config.get("secret_access_key", "PLACEHOLDER") != "PLACEHOLDER"
        and HAS_BOTO3
    )
    config["_backend"] = "s3" if has_s3 else "wrangler"

    return config


def create_s3_client(config: dict):
    """Create boto3 S3 client configured for Cloudflare R2."""
    endpoint_url = f"https://{config['account_id']}.r2.cloudflarestorage.com"
    return boto3.client(
        "s3",
        endpoint_url=endpoint_url,
        aws_access_key_id=config["access_key_id"],
        aws_secret_access_key=config["secret_access_key"],
        config=BotoConfig(
            retries={"max_attempts": 3, "mode": "adaptive"},
            s3={"addressing_style": "path"},
        ),
        region_name="auto",
    )


# ── Ed25519 Signing ──


def load_signing_key() -> Ed25519PrivateKey:
    """Load the Ed25519 private key from PEM file."""
    if not PRIVATE_KEY_PATH.exists():
        print(f"ERROR: Signing key not found: {PRIVATE_KEY_PATH}", file=sys.stderr)
        sys.exit(1)

    key_data = PRIVATE_KEY_PATH.read_bytes()
    key = load_pem_private_key(key_data, password=None)

    if not isinstance(key, Ed25519PrivateKey):
        print(
            f"ERROR: Key is not Ed25519 (got {type(key).__name__})", file=sys.stderr
        )
        sys.exit(1)

    return key


def sha256_stream(file_path: Path) -> str:
    """Compute SHA-256 hex digest by streaming 8 MB chunks."""
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def create_signature(
    private_key: Ed25519PrivateKey, sha256_hex: str
) -> dict:
    """Sign a SHA-256 hash with Ed25519 and return the .sig JSON payload."""
    # Sign the raw hash bytes (32 bytes from the SHA-256 digest)
    hash_bytes = bytes.fromhex(sha256_hex)
    signature_bytes = private_key.sign(hash_bytes)
    signature_b64 = base64.b64encode(signature_bytes).decode("ascii")

    return {
        "version": SIG_VERSION,
        "algorithm": "Ed25519",
        "hash": f"sha256:{sha256_hex}",
        "signature": signature_b64,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "keyId": KEY_ID,
    }


# ── CDN Operations (package discovery + signature check) ──


def fetch_package_index() -> dict[str, dict]:
    """Fetch the CDN index.json and return {appId: metadata} dict."""
    try:
        resp = requests.get(CDN_INDEX_URL, timeout=30)
        resp.raise_for_status()
    except requests.RequestException as e:
        print(f"ERROR: Failed to fetch CDN index: {e}", file=sys.stderr)
        sys.exit(1)

    data = resp.json()
    packages = data.get("packages", {})
    if not packages:
        print("ERROR: CDN index has no packages", file=sys.stderr)
        sys.exit(1)

    return packages


def check_sig_exists_cdn(app_id: str) -> bool:
    """Check if a .sig file exists on the CDN via HEAD request."""
    url = f"{CDN_BASE_URL}/{R2_PREFIX}{app_id}.sig"
    try:
        resp = requests.head(url, timeout=10, allow_redirects=True)
        return resp.status_code == 200
    except requests.RequestException:
        return False


# ── R2 Operations (boto3 / S3) ──


def s3_list_sig_keys(s3_client, bucket: str) -> set[str]:
    """List all .sig keys in the bucket using boto3 paginator."""
    sig_keys: set[str] = set()
    paginator = s3_client.get_paginator("list_objects_v2")
    for page in paginator.paginate(Bucket=bucket, Prefix=R2_PREFIX):
        for obj in page.get("Contents", []):
            if obj["Key"].endswith(".sig"):
                sig_keys.add(obj["Key"])
    return sig_keys


def s3_download_file(s3_client, bucket: str, key: str, dest: Path) -> None:
    """Download an object from R2 to a local file."""
    s3_client.download_file(bucket, key, str(dest))


def s3_upload_json(s3_client, bucket: str, key: str, data: dict) -> None:
    """Upload a JSON object to R2."""
    body = json.dumps(data, indent=2, ensure_ascii=False).encode("utf-8")
    s3_client.put_object(
        Bucket=bucket,
        Key=key,
        Body=body,
        ContentType="application/json",
    )


# ── R2 Operations (wrangler) ──


def wrangler_download_file(bucket: str, key: str, dest: Path) -> bool:
    """Download an object from R2 to a local file via wrangler."""
    result = subprocess.run(
        [NPX, "wrangler", "r2", "object", "get",
         f"{bucket}/{key}",
         f"--file={dest}", "--remote"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        err = result.stderr.replace("\x1b[31m", "").replace("\x1b[0m", "")[:200]
        print(f"download error: {err}", file=sys.stderr)
        return False
    return True


def cf_api_upload_json(bucket: str, key: str, data: dict) -> bool:
    """Upload a JSON payload to R2 via Cloudflare REST API."""
    token = os.environ.get("CLOUDFLARE_API_TOKEN", "")
    account_id = os.environ.get("CLOUDFLARE_ACCOUNT_ID", "")
    if not token or not account_id:
        print("ERROR: CLOUDFLARE_API_TOKEN or CLOUDFLARE_ACCOUNT_ID not set", file=sys.stderr)
        return False

    url = f"https://api.cloudflare.com/client/v4/accounts/{account_id}/r2/buckets/{bucket}/objects/{key}"
    body = json.dumps(data, indent=2, ensure_ascii=False).encode("utf-8")
    resp = requests.put(
        url,
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        },
        data=body,
        timeout=30,
    )
    if resp.status_code != 200 or not resp.json().get("success"):
        print(f"upload error: {resp.status_code} {resp.text[:200]}", file=sys.stderr)
        return False
    return True


# ── Main Logic ──


def sign_package(
    *,
    private_key: Ed25519PrivateKey,
    bucket: str,
    app_id: str,
    s3_client=None,
    dry_run: bool = False,
) -> dict:
    """Download a .mkpkg from R2, hash it, sign it, and upload the .sig.

    Returns a result dict with status and details.
    """
    pkg_key = f"{R2_PREFIX}{app_id}.mkpkg"
    sig_key = f"{R2_PREFIX}{app_id}.sig"

    if dry_run:
        return {"appId": app_id, "status": "dry-run", "sigKey": sig_key}

    # Create temp directory for download
    with tempfile.TemporaryDirectory(prefix="mkpkg_sign_") as tmp_dir:
        local_path = Path(tmp_dir) / f"{app_id}.mkpkg"

        # Download the package via public CDN (no auth needed, no rate limits)
        cdn_url = f"{CDN_BASE_URL}/{R2_PREFIX}{app_id}.mkpkg"
        try:
            resp = requests.get(cdn_url, stream=True, timeout=120)
            resp.raise_for_status()
            with open(local_path, "wb") as f:
                for chunk in resp.iter_content(chunk_size=8 * 1024 * 1024):
                    f.write(chunk)
        except Exception as e:
            return {"appId": app_id, "status": "error", "error": f"CDN download: {e}"}

        if not local_path.exists() or local_path.stat().st_size == 0:
            return {"appId": app_id, "status": "error", "error": "empty download"}

        file_size = local_path.stat().st_size

        # Compute SHA-256 (streaming, 8 MB chunks)
        sha256_hex = sha256_stream(local_path)

        # temp .mkpkg is auto-deleted when exiting the context manager

    # Create signature payload
    sig_payload = create_signature(private_key, sha256_hex)

    # Upload .sig to R2
    if s3_client:
        try:
            s3_upload_json(s3_client, bucket, sig_key, sig_payload)
        except Exception as e:
            return {"appId": app_id, "status": "error", "error": f"upload: {e}"}
    else:
        if not cf_api_upload_json(bucket, sig_key, sig_payload):
            return {"appId": app_id, "status": "error", "error": "sig upload failed"}

    return {
        "appId": app_id,
        "status": "signed",
        "hash": f"sha256:{sha256_hex}",
        "sigKey": sig_key,
        "fileSize": file_size,
    }


def main():
    load_env()

    parser = argparse.ArgumentParser(
        description="Sign .mkpkg packages on Cloudflare R2 with Ed25519"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview signing without downloading or uploading",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-sign packages even if .sig already exists",
    )
    parser.add_argument(
        "--app-id",
        help="Sign a single package by app ID (e.g., 1716740)",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List packages and their signature status",
    )
    args = parser.parse_args()

    # Load config and signing key
    config = load_config()
    backend = config["_backend"]
    bucket = config["bucket_name"]
    public_url = config.get("public_url", CDN_BASE_URL)

    private_key = load_signing_key()

    print("sign_packages.py -- Ed25519 package signing")
    print(f"  Signing key: {PRIVATE_KEY_PATH.name}")
    print(f"  Key ID:      {KEY_ID}")
    print(f"  R2 bucket:   {bucket}")
    print(f"  Backend:     {backend}")
    print()

    # Create S3 client if using boto3 backend
    s3 = create_s3_client(config) if backend == "s3" else None

    # Fetch package index from CDN (source of truth)
    print("Fetching package index from CDN...", end=" ", flush=True)
    packages = fetch_package_index()
    print(f"{len(packages)} packages found")

    # Filter by app ID if specified
    if args.app_id:
        if args.app_id not in packages:
            print(f"ERROR: App ID {args.app_id} not found in CDN index", file=sys.stderr)
            sys.exit(1)
        packages = {args.app_id: packages[args.app_id]}

    total = len(packages)

    # Check which packages already have signatures
    print("Checking existing signatures...", end=" ", flush=True)

    if s3:
        # S3 backend: list .sig keys in bulk (faster)
        existing_sigs = s3_list_sig_keys(s3, bucket)
        sig_status = {}
        for app_id in packages:
            sig_key = f"{R2_PREFIX}{app_id}.sig"
            sig_status[app_id] = sig_key in existing_sigs
    else:
        # Wrangler backend: check each .sig via CDN HEAD request
        sig_status = {}
        for app_id in packages:
            sig_status[app_id] = check_sig_exists_cdn(app_id)

    signed_count = sum(1 for v in sig_status.values() if v)
    unsigned_count = total - signed_count
    print(f"{signed_count} signed, {unsigned_count} unsigned")
    print()

    # ── List mode ──

    if args.list:
        for app_id in sorted(packages.keys(), key=int):
            meta = packages[app_id]
            name = meta.get("name", "?")
            size_mb = meta.get("sizeBytes", 0) / (1024 * 1024)
            has_sig = sig_status.get(app_id, False)
            marker = "+" if has_sig else "-"
            status = "SIGNED" if has_sig else "UNSIGNED"
            print(f"  [{marker}] {app_id:>10s}  {size_mb:>8.1f} MB  {status:10s}  {name}")
        print()
        print(f"Total: {total} | Signed: {signed_count} | Unsigned: {unsigned_count}")
        return

    # ── Signing mode ──

    # Determine which packages need signing
    to_sign = []
    skipped = 0
    for app_id in sorted(packages.keys(), key=int):
        has_sig = sig_status.get(app_id, False)
        if has_sig and not args.force:
            skipped += 1
            continue
        to_sign.append(app_id)

    if not to_sign:
        print("All packages already signed. Nothing to do.")
        if not args.force:
            print("  Use --force to re-sign all packages.")
        return

    sign_count = len(to_sign)
    print(f"Packages to sign: {sign_count}")
    if skipped > 0:
        print(f"Already signed (skipped): {skipped}")
    if args.force:
        print("Force mode: re-signing all packages")
    if args.dry_run:
        print("DRY RUN: no changes will be made")
    print()
    print("-" * 70)

    # Sign each package
    signed = 0
    errors = 0
    results = []
    t0 = time.monotonic()

    for i, app_id in enumerate(to_sign, 1):
        meta = packages[app_id]
        name = meta.get("name", "?")
        size_bytes = meta.get("sizeBytes", meta.get("size", 0))
        size_mb = size_bytes / (1024 * 1024)
        re_sign = args.force and sig_status.get(app_id, False)

        action = "re-signing" if re_sign else "signing"
        print(f"[{i}/{sign_count}] {action} {app_id} ({size_mb:.1f} MB) {name}")

        step_t0 = time.monotonic()
        result = sign_package(
            private_key=private_key,
            bucket=bucket,
            app_id=app_id,
            s3_client=s3,
            dry_run=args.dry_run,
        )
        step_elapsed = time.monotonic() - step_t0
        results.append(result)

        status = result["status"]
        if status == "signed":
            signed += 1
            short_hash = result["hash"][:32] + "..."
            print(f"    OK  {short_hash}  ({step_elapsed:.1f}s)")
        elif status == "dry-run":
            signed += 1
            print(f"    (dry-run)")
        else:
            errors += 1
            print(f"    FAILED: {result.get('error', 'unknown')}")

        # Rate limit: brief pause between packages to avoid R2 throttling
        if i < sign_count and not args.dry_run:
            time.sleep(0.5)

    elapsed = time.monotonic() - t0

    # ── Summary ──

    print()
    print("=" * 70)
    print("  SIGNING COMPLETE")
    print("=" * 70)
    print(f"  Signed:    {signed}")
    print(f"  Skipped:   {skipped}")
    print(f"  Errors:    {errors}")
    print(f"  Total:     {total}")
    print(f"  Time:      {elapsed:.1f}s")

    if not args.dry_run and signed > 0:
        print(f"  Sigs at:   {public_url}/{R2_PREFIX}<appId>.sig")
        print(f"  Example:   {public_url}/{R2_PREFIX}{to_sign[0]}.sig")

    if errors > 0:
        print()
        print("  Failed packages:")
        for r in results:
            if r["status"] == "error":
                print(f"    - {r['appId']}: {r.get('error', 'unknown')}")
        sys.exit(1)


if __name__ == "__main__":
    main()
