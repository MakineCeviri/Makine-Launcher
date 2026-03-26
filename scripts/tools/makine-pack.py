#!/usr/bin/env python3
"""
makine-pack — Package a plugin directory into .makine format (MKPK v2)

MKPK v2 format: [Magic: 4B "MKPK"] [Version: 1B = 0x02] [zstd compressed tar data]

Usage:
    python makine-pack.py <plugin-dir> [--output <file.makine>] [--level 22]

Example:
    python makine-pack.py ./build/release/my-plugin/
    python makine-pack.py ./my-plugin/ --output my-plugin-1.0.0.makine
    python makine-pack.py ./my-plugin/ --level 19  # faster, slightly larger

The plugin directory must contain:
    - manifest.json (required)
    - *.dll (at least one)

Output: <plugin-id>-<version>.makine (auto-named from manifest.json)
"""

import argparse
import hashlib
import io
import json
import os
import struct
import sys
import tarfile

try:
    import zstandard as zstd
except ImportError:
    print("ERROR: zstandard package required. Install with: pip install zstandard")
    sys.exit(1)


MKPK_MAGIC = b"MKPK"
MKPK_VERSION_V2 = 2

# Forbidden file extensions in plugins
FORBIDDEN_EXTENSIONS = {".exe", ".bat", ".cmd", ".ps1", ".vbs", ".msi", ".js"}


def validate_plugin_dir(plugin_dir: str) -> dict:
    """Validate plugin directory and return parsed manifest."""
    manifest_path = os.path.join(plugin_dir, "manifest.json")
    if not os.path.exists(manifest_path):
        print(f"ERROR: manifest.json not found in {plugin_dir}")
        sys.exit(1)

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    required = ["id", "name", "version", "apiVersion", "entry"]
    for field in required:
        if field not in manifest:
            print(f"ERROR: manifest.json missing required field: {field}")
            sys.exit(1)

    entry_dll = os.path.join(plugin_dir, manifest["entry"])
    if not os.path.exists(entry_dll):
        print(f"ERROR: Entry DLL not found: {manifest['entry']}")
        sys.exit(1)

    # Check for forbidden files
    for root, _dirs, files in os.walk(plugin_dir):
        for fname in files:
            ext = os.path.splitext(fname)[1].lower()
            if ext in FORBIDDEN_EXTENSIONS:
                print(f"ERROR: Forbidden file type found: {fname}")
                sys.exit(1)

    return manifest


def create_tar_bytes(plugin_dir: str) -> bytes:
    """Create tar archive of plugin directory contents."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tar:
        for entry in sorted(os.listdir(plugin_dir)):
            full_path = os.path.join(plugin_dir, entry)
            tar.add(full_path, arcname=entry)
    return buf.getvalue()


def compress_zstd(data: bytes, level: int = 22) -> bytes:
    """Compress data with zstandard at given level."""
    cctx = zstd.ZstdCompressor(level=level)
    return cctx.compress(data)


def pack_mkpk_v2(tar_data: bytes, zstd_level: int = 22) -> bytes:
    """Create MKPK v2 package: magic + version + zstd(tar)."""
    compressed = compress_zstd(tar_data, zstd_level)

    header = MKPK_MAGIC + struct.pack("B", MKPK_VERSION_V2)
    return header + compressed


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(
        description="Package a plugin directory into .makine format (MKPK v2)")
    parser.add_argument("plugin_dir", help="Path to plugin directory")
    parser.add_argument("--output", "-o", help="Output .makine file path")
    parser.add_argument("--level", "-l", type=int, default=22,
                        help="Zstandard compression level (1-22, default: 22)")
    args = parser.parse_args()

    plugin_dir = os.path.abspath(args.plugin_dir)
    if not os.path.isdir(plugin_dir):
        print(f"ERROR: Not a directory: {plugin_dir}")
        sys.exit(1)

    # Validate
    manifest = validate_plugin_dir(plugin_dir)
    plugin_id = manifest["id"]
    version = manifest["version"]

    print(f"Packing: {manifest['name']} v{version}")
    print(f"  ID: {plugin_id}")
    print(f"  Entry: {manifest['entry']}")
    print(f"  Compression: zstd level {args.level}")

    # Create tar
    print("  Creating tar archive...")
    tar_data = create_tar_bytes(plugin_dir)
    print(f"  Tar size: {len(tar_data):,} bytes")

    # Pack as MKPK v2
    print("  Compressing with zstd...")
    package = pack_mkpk_v2(tar_data, args.level)

    ratio = len(package) / len(tar_data) * 100
    print(f"  Package size: {len(package):,} bytes ({ratio:.1f}% of tar)")

    # Output path
    if args.output:
        output = args.output
    else:
        safe_id = plugin_id.replace(".", "-")
        output = f"{safe_id}-{version}.makine"

    with open(output, "wb") as f:
        f.write(package)

    checksum = sha256_hex(package)
    print(f"\nOutput: {output}")
    print(f"SHA-256: {checksum}")
    print(f"Size: {len(package):,} bytes")

    # Print index.json entry for convenience
    print(f"\n--- index.json entry ---")
    entry = {
        "id": plugin_id,
        "version": version,
        "sha256": checksum,
        "size": len(package),
    }
    print(json.dumps(entry, indent=2))


if __name__ == "__main__":
    main()
