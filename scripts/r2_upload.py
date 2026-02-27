#!/usr/bin/env python3
"""
r2_upload.py -- Upload .mkpkg files to Cloudflare R2.

Cloudflare R2 is S3-compatible, so we use boto3 with a custom endpoint.

Usage:
    python scripts/r2_upload.py                          # Upload all packages
    python scripts/r2_upload.py --app-id 1716740         # Upload single package
    python scripts/r2_upload.py --dry-run                # Preview without uploading
    python scripts/r2_upload.py --list                   # List files in R2 bucket
    python scripts/r2_upload.py --verify                 # Verify all uploads match local

Requires: pip install boto3
"""

import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

try:
    import boto3
    from botocore.config import Config as BotoConfig
except ImportError:
    print("ERROR: boto3 not installed. Run: pip install boto3", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = Path(__file__).parent
CONFIG_PATH = SCRIPT_DIR / "r2_config.json"
DEFAULT_DATA_DIR = Path("C:/cedra/MakineAI-Assets-Build/data")
R2_PREFIX = "data/"  # Object key prefix in bucket


def load_config() -> dict:
    """Load R2 credentials from config file."""
    if not CONFIG_PATH.exists():
        print(f"ERROR: R2 config not found: {CONFIG_PATH}", file=sys.stderr)
        print("Copy r2_config.json.template and fill in your credentials.", file=sys.stderr)
        sys.exit(1)

    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))

    for key in ("account_id", "access_key_id", "secret_access_key", "bucket_name"):
        if config.get(key, "PLACEHOLDER") == "PLACEHOLDER":
            print(f"ERROR: {key} not configured in {CONFIG_PATH}", file=sys.stderr)
            sys.exit(1)

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


def sha256_file(path: Path) -> str:
    """Compute SHA-256 hex digest of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def upload_file(
    s3_client,
    bucket: str,
    local_path: Path,
    object_key: str,
    dry_run: bool = False,
) -> dict:
    """Upload a single file to R2 with progress."""
    file_size = local_path.stat().st_size
    checksum = sha256_file(local_path)

    if dry_run:
        return {
            "key": object_key,
            "size": file_size,
            "checksum": checksum,
            "status": "dry-run",
        }

    # Check if already uploaded with same size
    try:
        head = s3_client.head_object(Bucket=bucket, Key=object_key)
        remote_size = head["ContentLength"]
        if remote_size == file_size:
            return {
                "key": object_key,
                "size": file_size,
                "checksum": checksum,
                "status": "skipped (already exists, same size)",
            }
    except s3_client.exceptions.ClientError:
        pass  # Object doesn't exist yet — proceed with upload

    # Upload with progress callback
    uploaded_bytes = [0]
    last_print = [0]

    def progress_callback(bytes_transferred):
        uploaded_bytes[0] += bytes_transferred
        now = time.monotonic()
        if now - last_print[0] > 1.0:  # Print every 1s
            pct = uploaded_bytes[0] / file_size * 100 if file_size > 0 else 0
            print(f"    {pct:.0f}% ({uploaded_bytes[0]:,} / {file_size:,} bytes)", end="\r")
            last_print[0] = now

    t0 = time.monotonic()
    s3_client.upload_file(
        str(local_path),
        bucket,
        object_key,
        Callback=progress_callback,
        ExtraArgs={"ContentType": "application/octet-stream"},
    )
    elapsed = time.monotonic() - t0
    speed = file_size / elapsed / (1024 * 1024) if elapsed > 0 else 0
    print(f"    100% uploaded ({speed:.1f} MB/s)      ")

    return {
        "key": object_key,
        "size": file_size,
        "checksum": checksum,
        "status": "uploaded",
        "elapsed": round(elapsed, 1),
        "speedMBps": round(speed, 1),
    }


def list_objects(s3_client, bucket: str, prefix: str = R2_PREFIX):
    """List all objects in the bucket with given prefix."""
    objects = []
    paginator = s3_client.get_paginator("list_objects_v2")
    for page in paginator.paginate(Bucket=bucket, Prefix=prefix):
        for obj in page.get("Contents", []):
            objects.append({
                "key": obj["Key"],
                "size": obj["Size"],
                "lastModified": obj["LastModified"].isoformat(),
            })
    return objects


def main():
    parser = argparse.ArgumentParser(description="Upload .mkpkg files to Cloudflare R2")
    parser.add_argument("--data-dir", default=str(DEFAULT_DATA_DIR),
                        help="Local directory containing .mkpkg files")
    parser.add_argument("--app-id", help="Upload single package by app ID")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview without uploading")
    parser.add_argument("--list", action="store_true",
                        help="List files in R2 bucket")
    parser.add_argument("--verify", action="store_true",
                        help="Verify all local files exist in R2 with correct size")
    args = parser.parse_args()

    config = load_config()
    s3 = create_s3_client(config)
    bucket = config["bucket_name"]
    data_dir = Path(args.data_dir)

    # List mode
    if args.list:
        objects = list_objects(s3, bucket)
        total_size = sum(o["size"] for o in objects)
        print(f"R2 bucket '{bucket}' — {len(objects)} objects, {total_size:,} bytes ({total_size / (1024**3):.2f} GB)")
        for obj in sorted(objects, key=lambda o: o["key"]):
            print(f"  {obj['key']:40s}  {obj['size']:>12,} bytes  {obj['lastModified']}")
        return

    # Find .mkpkg files
    if args.app_id:
        files = [data_dir / f"{args.app_id}.mkpkg"]
        if not files[0].exists():
            print(f"ERROR: File not found: {files[0]}", file=sys.stderr)
            sys.exit(1)
    else:
        files = sorted(data_dir.glob("*.mkpkg"))
        if not files:
            print(f"ERROR: No .mkpkg files found in {data_dir}", file=sys.stderr)
            print("Run the pipeline first: python scripts/package_pipeline.py", file=sys.stderr)
            sys.exit(1)

    # Verify mode
    if args.verify:
        remote_objects = {o["key"]: o["size"] for o in list_objects(s3, bucket)}
        ok = 0
        missing = 0
        mismatch = 0

        for f in files:
            key = R2_PREFIX + f.name
            local_size = f.stat().st_size
            remote_size = remote_objects.get(key)

            if remote_size is None:
                print(f"  MISSING: {key}")
                missing += 1
            elif remote_size != local_size:
                print(f"  MISMATCH: {key} (local={local_size:,}, remote={remote_size:,})")
                mismatch += 1
            else:
                ok += 1

        print(f"\nVerification: {ok} OK, {missing} missing, {mismatch} size mismatch")
        sys.exit(0 if (missing == 0 and mismatch == 0) else 1)

    # Upload mode
    total = len(files)
    total_size = sum(f.stat().st_size for f in files)
    public_url = config.get("public_url", "https://cdn.makineceviri.net")

    print(f"Uploading {total} packages to R2 ({total_size:,} bytes / {total_size / (1024**3):.2f} GB)")
    print(f"  Bucket: {bucket}")
    print(f"  Public: {public_url}")
    if args.dry_run:
        print("  MODE: DRY RUN")
    print()

    results = []
    uploaded = 0
    skipped = 0
    errors = 0

    for i, f in enumerate(files, 1):
        object_key = R2_PREFIX + f.name
        app_id = f.stem
        print(f"[{i}/{total}] {app_id} ({f.stat().st_size:,} bytes)")

        try:
            result = upload_file(s3, bucket, f, object_key, args.dry_run)
            results.append(result)

            if result["status"] == "uploaded":
                uploaded += 1
            elif "skipped" in result["status"]:
                skipped += 1
        except Exception as e:
            print(f"    ERROR: {e}")
            errors += 1
            results.append({"key": object_key, "status": "error", "error": str(e)})

    # Summary
    print()
    print("=" * 60)
    print("UPLOAD COMPLETE")
    print("=" * 60)
    print(f"  Uploaded: {uploaded}")
    print(f"  Skipped:  {skipped}")
    print(f"  Errors:   {errors}")
    print(f"  Total:    {total}")

    if public_url and not args.dry_run:
        print(f"\n  Example URL: {public_url}/{R2_PREFIX}<appId>.mkpkg")

    # Save upload report
    if not args.dry_run:
        report_path = data_dir / "upload_report.json"
        report = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "bucket": bucket,
            "publicUrl": public_url,
            "totalFiles": total,
            "uploaded": uploaded,
            "skipped": skipped,
            "errors": errors,
            "files": results,
        }
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        print(f"  Report: {report_path}")


if __name__ == "__main__":
    main()
