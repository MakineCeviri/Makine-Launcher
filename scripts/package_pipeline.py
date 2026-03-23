#!/usr/bin/env python3
"""
package_pipeline.py -- Compress + Encrypt translation packages for distribution.

Creates .makine files (MKPK format v1):
    [Magic: 4B "MKPK"] [Version: 1B] [Nonce: 12B] [Ciphertext+AuthTag: NB]

Inner payload: tar.zst (zstandard level 9 compressed tar archive)
Encryption: AES-256-GCM (authenticated, tamper-proof)

Usage:
    python scripts/package_pipeline.py                     # Process all packages
    python scripts/package_pipeline.py --app-id 1716740    # Process single package
    python scripts/package_pipeline.py --dry-run            # Preview without writing
    python scripts/package_pipeline.py --verify 1716740     # Verify a .makine file

Output:
    {output}/data/{appId}.makine   -- Encrypted compressed package
    {output}/pipeline_report.json -- Processing report with sizes & checksums
"""

import argparse
import hashlib
import io
import json
import os
import struct
import sys
import tarfile
import time
from pathlib import Path
from typing import Optional

try:
    import zstandard as zstd
except ImportError:
    print("ERROR: zstandard not installed. Run: pip install zstandard", file=sys.stderr)
    sys.exit(1)

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    print("ERROR: cryptography not installed. Run: pip install cryptography", file=sys.stderr)
    sys.exit(1)


# =============================================================================
# Constants
# =============================================================================

MAGIC = b"MKPK"
FORMAT_VERSION = 0x01
NONCE_SIZE = 12      # AES-GCM standard nonce size
TAG_SIZE = 16        # AES-GCM auth tag (appended by AESGCM.encrypt)
HEADER_SIZE = 4 + 1 + NONCE_SIZE  # 17 bytes total header

ZSTD_LEVEL = 19      # High compression — slower but much smaller
ZSTD_THREADS = 0     # Auto-detect CPU count

# Deferred packages (removed from manifest, kept here for reference)
DEFERRED_APP_IDS = set()


# =============================================================================
# Key Management
# =============================================================================

def load_encryption_key(key_path: Path) -> bytes:
    """Load 256-bit AES key from hex-encoded file."""
    if not key_path.exists():
        print(f"ERROR: Encryption key not found: {key_path}", file=sys.stderr)
        print("Generate one with: python -c \"import os; open('scripts/.encryption_key','w').write(os.urandom(32).hex()+'\\n')\"",
              file=sys.stderr)
        sys.exit(1)

    key_hex = key_path.read_text().strip()
    if len(key_hex) != 64:
        print(f"ERROR: Key must be 64 hex chars (256 bits), got {len(key_hex)}", file=sys.stderr)
        sys.exit(1)

    return bytes.fromhex(key_hex)


# =============================================================================
# Compression
# =============================================================================

def compress_directory(dir_path: Path) -> bytes:
    """Create tar archive of directory, then compress with zstd level 9."""
    # Phase 1: Create tar in memory
    # File extensions to exclude from packages (backups, temporaries)
    excluded_suffixes = {'.bak', '.bak2', '.orig'}

    tar_buffer = io.BytesIO()
    with tarfile.open(fileobj=tar_buffer, mode="w") as tar:
        for entry in sorted(dir_path.rglob("*")):
            if entry.is_file():
                if entry.suffix.lower() in excluded_suffixes:
                    continue
                arcname = entry.relative_to(dir_path).as_posix()
                tar.add(str(entry), arcname=arcname)
    tar_data = tar_buffer.getvalue()

    # Phase 2: Compress with zstd
    compressor = zstd.ZstdCompressor(level=ZSTD_LEVEL, threads=ZSTD_THREADS)
    compressed = compressor.compress(tar_data)

    return compressed


def decompress_to_tar(compressed_data: bytes) -> bytes:
    """Decompress zstd data back to tar."""
    decompressor = zstd.ZstdDecompressor()
    return decompressor.decompress(compressed_data, max_output_size=10 * 1024 * 1024 * 1024)  # 10GB max


# =============================================================================
# Encryption
# =============================================================================

def encrypt_package(compressed_data: bytes, key: bytes) -> bytes:
    """
    Encrypt compressed data with AES-256-GCM.

    Returns MKPK format:
        [MKPK][0x01][nonce:12][ciphertext+tag:N]
    """
    nonce = os.urandom(NONCE_SIZE)
    aesgcm = AESGCM(key)

    # AESGCM.encrypt returns ciphertext + 16-byte auth tag
    encrypted = aesgcm.encrypt(nonce, compressed_data, associated_data=MAGIC)

    # Build MKPK binary
    header = MAGIC + struct.pack("B", FORMAT_VERSION) + nonce
    return header + encrypted


def decrypt_package(mkpkg_data: bytes, key: bytes) -> bytes:
    """
    Decrypt MKPK format back to compressed data.

    Validates magic, version, and GCM auth tag.
    Raises ValueError on any integrity failure.
    """
    if len(mkpkg_data) < HEADER_SIZE + TAG_SIZE:
        raise ValueError(f"File too small: {len(mkpkg_data)} bytes")

    magic = mkpkg_data[:4]
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {magic!r} (expected {MAGIC!r})")

    version = mkpkg_data[4]
    if version != FORMAT_VERSION:
        raise ValueError(f"Unsupported version: {version} (expected {FORMAT_VERSION})")

    nonce = mkpkg_data[5:5 + NONCE_SIZE]
    ciphertext = mkpkg_data[5 + NONCE_SIZE:]

    aesgcm = AESGCM(key)
    try:
        return aesgcm.decrypt(nonce, ciphertext, associated_data=MAGIC)
    except Exception as e:
        raise ValueError(f"Decryption failed (tampered or wrong key): {e}")


# =============================================================================
# Pipeline
# =============================================================================

def sha256_digest(data: bytes) -> str:
    """Compute SHA-256 hex digest with sha256: prefix."""
    return "sha256:" + hashlib.sha256(data).hexdigest()


def process_package(
    app_id: str,
    dir_path: Path,
    output_dir: Path,
    key: bytes,
    dry_run: bool = False,
) -> Optional[dict]:
    """Process a single translation package through the pipeline."""
    if not dir_path.exists():
        return {"appId": app_id, "error": f"Directory not found: {dir_path}"}

    # Measure raw size
    raw_size = sum(f.stat().st_size for f in dir_path.rglob("*") if f.is_file())
    file_count = sum(1 for f in dir_path.rglob("*") if f.is_file())

    if raw_size == 0:
        return {"appId": app_id, "error": "Empty directory"}

    t0 = time.monotonic()

    # Step 1: Compress
    compressed = compress_directory(dir_path)
    t1 = time.monotonic()

    # Step 2: Encrypt
    mkpkg = encrypt_package(compressed, key)
    t2 = time.monotonic()

    # Step 3: Checksums
    checksum = sha256_digest(mkpkg)

    # Step 4: Write output
    output_path = output_dir / f"{app_id}.makine"
    if not dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(mkpkg)

    ratio = len(compressed) / raw_size * 100 if raw_size > 0 else 0

    return {
        "appId": app_id,
        "rawSize": raw_size,
        "fileCount": file_count,
        "compressedSize": len(compressed),
        "encryptedSize": len(mkpkg),
        "compressionRatio": round(ratio, 1),
        "checksum": checksum,
        "compressTime": round(t1 - t0, 2),
        "encryptTime": round(t2 - t1, 2),
        "outputPath": str(output_path) if not dry_run else "(dry-run)",
    }


def verify_package(app_id: str, output_dir: Path, data_dir: Path, key: bytes) -> dict:
    """Verify a .makine file: decrypt, decompress, compare file list."""
    mkpkg_path = output_dir / f"{app_id}.makine"
    if not mkpkg_path.exists():
        return {"appId": app_id, "valid": False, "error": f"File not found: {mkpkg_path}"}

    try:
        mkpkg_data = mkpkg_path.read_bytes()

        # Decrypt
        compressed = decrypt_package(mkpkg_data, key)

        # Decompress
        tar_data = decompress_to_tar(compressed)

        # List files in tar
        tar_buffer = io.BytesIO(tar_data)
        with tarfile.open(fileobj=tar_buffer, mode="r") as tar:
            tar_files = sorted(m.name for m in tar.getmembers() if m.isfile())

        # Compare with source directory
        source_dir = None
        # Find source dir from manifest
        assets_dir = Path(os.environ.get("MAKINE_ASSETS_DIR", Path(__file__).parent.parent / "Makine-Launcher-Assets" / "assets"))
        manifest_path = assets_dir / "packages" / f"{app_id}.json"
        if manifest_path.exists():
            pkg = json.loads(manifest_path.read_text(encoding="utf-8"))
            dir_name = pkg.get("dirName", "")
            if dir_name:
                source_dir = data_dir / dir_name

        source_files = []
        if source_dir and source_dir.exists():
            source_files = sorted(
                f.relative_to(source_dir).as_posix()
                for f in source_dir.rglob("*") if f.is_file()
            )

        files_match = tar_files == source_files

        return {
            "appId": app_id,
            "valid": True,
            "decrypted": True,
            "decompressed": True,
            "fileCount": len(tar_files),
            "filesMatch": files_match,
            "encryptedSize": len(mkpkg_data),
            "compressedSize": len(compressed),
            "rawSize": len(tar_data),
        }

    except ValueError as e:
        return {"appId": app_id, "valid": False, "error": str(e)}
    except Exception as e:
        return {"appId": app_id, "valid": False, "error": f"Unexpected: {e}"}


# =============================================================================
# Manifest Update
# =============================================================================

def update_manifest(
    manifest_path: Path,
    packages_dir: Path,
    results: list[dict],
    r2_base_url: str,
) -> dict:
    """
    Update index.json and per-game JSONs with pipeline output.

    Adds to index.json:
      - size: encrypted download size (bytes)
      - dataUrl: R2 download URL
      - checksum: SHA-256 of .makine file

    Adds to packages/{appId}.json:
      - compressedSize, compressedChecksum, dataUrl
    """
    # Build lookup: appId -> result
    result_map = {r["appId"]: r for r in results if "error" not in r}

    if not result_map:
        return {"updated": 0, "error": "No successful results to apply"}

    # Update index.json
    with open(manifest_path, "r", encoding="utf-8") as f:
        index = json.load(f)

    today = time.strftime("%Y-%m-%d", time.gmtime())

    updated_count = 0
    for app_id, r in result_map.items():
        if app_id in index.get("packages", {}):
            entry = index["packages"][app_id]
            entry["size"] = r["encryptedSize"]
            entry["dataUrl"] = f"{r2_base_url}/{app_id}.makine"
            entry["checksum"] = r["checksum"]
            entry["v"] = today  # Bump version date for update detection
            updated_count += 1

    # Bump generatedAt
    index["generatedAt"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=2, ensure_ascii=False)

    # Update per-game JSONs
    per_game_updated = 0
    for app_id, r in result_map.items():
        pkg_path = packages_dir / f"{app_id}.json"
        if not pkg_path.exists():
            continue

        try:
            pkg = json.loads(pkg_path.read_text(encoding="utf-8"))
            pkg["compressedSize"] = r["encryptedSize"]
            pkg["compressedChecksum"] = r["checksum"]
            pkg["dataUrl"] = f"{r2_base_url}/{app_id}.makine"
            pkg["lastUpdated"] = today  # Bump version date
            with open(pkg_path, "w", encoding="utf-8") as f:
                json.dump(pkg, f, indent=2, ensure_ascii=False)
            per_game_updated += 1
        except (json.JSONDecodeError, OSError) as e:
            print(f"  WARNING: Could not update {pkg_path}: {e}", file=sys.stderr)

    return {
        "indexUpdated": updated_count,
        "perGameUpdated": per_game_updated,
        "r2BaseUrl": r2_base_url,
    }


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Compress + encrypt translation packages for distribution"
    )
    default_data = os.environ.get("MAKINE_DATA_DIR", str(Path(__file__).parent.parent / "translation_data"))
    default_assets = os.environ.get("MAKINE_ASSETS_DIR", str(Path(__file__).parent.parent / "Makine-Launcher-Assets" / "assets"))
    default_output = os.environ.get("MAKINE_BUILD_DATA_DIR", str(Path(__file__).parent.parent / "build" / "data"))
    parser.add_argument("--manifest", default=os.path.join(default_assets, "index.json"),
                        help="Path to index.json")
    parser.add_argument("--packages-dir", default=os.path.join(default_assets, "packages"),
                        help="Path to per-game JSON directory")
    parser.add_argument("--data", default=default_data,
                        help="Translation data root directory")
    parser.add_argument("--output", default=default_output,
                        help="Output directory for .makine files")
    parser.add_argument("--key", default="scripts/.encryption_key",
                        help="Path to encryption key file")
    parser.add_argument("--app-id", help="Process single package by app ID")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview without writing files")
    parser.add_argument("--verify", metavar="APP_ID",
                        help="Verify a .makine file (decrypt + decompress + check)")
    parser.add_argument("--skip-deferred", action="store_true", default=True,
                        help="Skip deferred large packages (default: true)")
    parser.add_argument("--include-deferred", action="store_true",
                        help="Include deferred large packages")
    parser.add_argument("--update-manifest", action="store_true",
                        help="Update index.json and per-game JSONs with pipeline results")
    parser.add_argument("--r2-base-url", default="https://cdn.makineceviri.org/data",
                        help="R2 base URL for data downloads")
    args = parser.parse_args()

    # Resolve key path relative to script directory
    script_dir = Path(__file__).parent
    key_path = script_dir / ".encryption_key" if args.key == "scripts/.encryption_key" else Path(args.key)
    key = load_encryption_key(key_path)

    output_dir = Path(args.output)
    data_dir = Path(args.data)

    # Verify mode
    if args.verify:
        result = verify_package(args.verify, output_dir, data_dir, key)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        sys.exit(0 if result.get("valid") else 1)

    # Load manifest
    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        print(f"ERROR: Manifest not found: {manifest_path}", file=sys.stderr)
        sys.exit(1)

    with open(manifest_path, "r", encoding="utf-8") as f:
        index = json.load(f)

    packages = index.get("packages", {})
    packages_dir = Path(args.packages_dir)

    # Filter to single app if specified
    if args.app_id:
        if args.app_id not in packages:
            print(f"ERROR: App ID {args.app_id} not found in manifest", file=sys.stderr)
            sys.exit(1)
        packages = {args.app_id: packages[args.app_id]}

    # Skip deferred packages
    skip_ids = DEFERRED_APP_IDS if (args.skip_deferred and not args.include_deferred) else set()

    # Process
    total = len(packages)
    results = []
    errors = []
    total_raw = 0
    total_encrypted = 0
    skipped = 0

    print(f"Processing {total} packages...")
    print(f"  Data: {data_dir}")
    print(f"  Output: {output_dir}")
    print(f"  Compression: zstd level {ZSTD_LEVEL}")
    print(f"  Encryption: AES-256-GCM")
    if args.dry_run:
        print("  MODE: DRY RUN (no files written)")
    print()

    for i, (app_id, entry) in enumerate(sorted(packages.items()), 1):
        game_name = entry.get("name", f"Unknown ({app_id})")

        if app_id in skip_ids:
            skipped += 1
            if i % 50 == 0 or i == total:
                print(f"  [{i}/{total}] SKIP (deferred): {game_name}")
            continue

        # Find directory name from per-game JSON
        pkg_json_path = packages_dir / f"{app_id}.json"
        dir_name = ""
        if pkg_json_path.exists():
            try:
                pkg_data = json.loads(pkg_json_path.read_text(encoding="utf-8"))
                dir_name = pkg_data.get("dirName", "")
            except (json.JSONDecodeError, OSError):
                pass

        if not dir_name:
            errors.append({"appId": app_id, "error": "No dirName in package JSON"})
            continue

        game_dir = data_dir / dir_name
        result = process_package(app_id, game_dir, output_dir, key, args.dry_run)

        if result and "error" not in result:
            results.append(result)
            total_raw += result["rawSize"]
            total_encrypted += result["encryptedSize"]
        elif result:
            errors.append(result)

        # Progress
        if i % 25 == 0 or i == total:
            if result and "error" not in result:
                print(f"  [{i}/{total}] {game_name}: "
                      f"{result['rawSize']:,} -> {result['encryptedSize']:,} bytes "
                      f"({result['compressionRatio']}%) [{result['compressTime']}s]")
            elif result:
                print(f"  [{i}/{total}] {game_name}: ERROR - {result.get('error', 'unknown')}")

    # Summary
    print()
    print("=" * 70)
    print("PIPELINE COMPLETE")
    print("=" * 70)
    print(f"  Processed: {len(results)}/{total}")
    print(f"  Skipped (deferred): {skipped}")
    print(f"  Errors: {len(errors)}")

    if total_raw > 0:
        overall_ratio = total_encrypted / total_raw * 100
        print(f"  Raw total:       {total_raw:>15,} bytes ({total_raw / (1024**3):.2f} GB)")
        print(f"  Encrypted total: {total_encrypted:>15,} bytes ({total_encrypted / (1024**3):.2f} GB)")
        print(f"  Overall ratio:   {overall_ratio:.1f}%")
        print(f"  Savings:         {(total_raw - total_encrypted):>15,} bytes ({(total_raw - total_encrypted) / (1024**3):.2f} GB)")

    if errors:
        print(f"\n  ERRORS ({len(errors)}):")
        for e in errors[:10]:
            print(f"    [{e['appId']}] {e.get('error', 'unknown')}")
        if len(errors) > 10:
            print(f"    ... and {len(errors) - 10} more")

    # Save report
    report = {
        "generatedAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "totalProcessed": len(results),
        "totalSkipped": skipped,
        "totalErrors": len(errors),
        "totalRawBytes": total_raw,
        "totalEncryptedBytes": total_encrypted,
        "packages": results,
        "errors": errors,
    }

    if not args.dry_run:
        report_path = output_dir / "pipeline_report.json"
        report_path.parent.mkdir(parents=True, exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        print(f"\n  Report: {report_path}")

    # Update manifest files with pipeline results
    if args.update_manifest and results and not args.dry_run:
        print("\n--- MANIFEST UPDATE ---")
        manifest_result = update_manifest(
            manifest_path=manifest_path,
            packages_dir=packages_dir,
            results=results,
            r2_base_url=args.r2_base_url,
        )
        print(f"  index.json: {manifest_result['indexUpdated']} entries updated")
        print(f"  packages/:  {manifest_result['perGameUpdated']} files updated")
        print(f"  R2 base:    {manifest_result['r2BaseUrl']}")
    elif args.update_manifest and args.dry_run:
        print("\n  Manifest update skipped (dry-run mode)")


if __name__ == "__main__":
    main()
