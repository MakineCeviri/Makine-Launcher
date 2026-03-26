#!/usr/bin/env python3
"""
split_manifest.py — Monolitik manifest.json'u index.json + per-game JSON dosyalarına böler.

Kullanım:
    python scripts/split_manifest.py [--manifest PATH] [--output DIR] [--data DIR]

Varsayılan:
    --manifest  $MAKINE_DATA_DIR/manifest.json  (veya ../translation_data/manifest.json)
    --output    $MAKINE_ASSETS_DIR              (veya ../Makine-Launcher-Assets/assets)
    --data      $MAKINE_DATA_DIR                (veya ../translation_data)

Çıktı:
    {output}/index.json           — Lightweight katalog (~10-20KB)
    {output}/packages/{appId}.json — Per-game detay (265 dosya)

index.json format:
    {
      "version": 5,
      "generatedAt": "2026-02-25T...",
      "totalPackages": 265,
      "packages": {
        "70":      { "name": "Half-Life",  "v": "2026-02-21", "sizeBytes": 12345678 },
        "1716740": { "name": "Stray",      "v": "2026-02-21", "sizeBytes": 5000000 },
        ...
      }
    }

packages/{appId}.json format:
    Full package data from manifest + computed sizeBytes + dataUrl (null until R2 upload)
"""

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path


def calculate_dir_size(dir_path: Path) -> int:
    """Calculate total size of all files in a directory (recursive)."""
    total = 0
    if not dir_path.exists():
        return 0
    for entry in dir_path.rglob("*"):
        if entry.is_file():
            try:
                total += entry.stat().st_size
            except OSError:
                pass
    return total


def count_files(dir_path: Path) -> int:
    """Count total files in a directory (recursive)."""
    if not dir_path.exists():
        return 0
    count = 0
    for entry in dir_path.rglob("*"):
        if entry.is_file():
            count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description="Split monolithic manifest into index + per-game JSON files")
    default_data = os.environ.get("MAKINE_DATA_DIR", str(Path(__file__).parent.parent / "translation_data"))
    default_assets = os.environ.get("MAKINE_ASSETS_DIR", str(Path(__file__).parent.parent / "Makine-Launcher-Assets" / "assets"))
    parser.add_argument("--manifest", default=os.path.join(default_data, "manifest.json"),
                        help="Path to monolithic manifest.json")
    parser.add_argument("--output", default=default_assets,
                        help="Output directory for split files")
    parser.add_argument("--data", default=default_data,
                        help="Translation data directory (for size calculation)")
    parser.add_argument("--skip-sizes", action="store_true",
                        help="Skip directory size calculation (faster, but sizeBytes will be 0)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print what would be written without writing files")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    output_dir = Path(args.output)
    data_dir = Path(args.data)

    # Validate inputs
    if not manifest_path.exists():
        print(f"ERROR: Manifest not found: {manifest_path}", file=sys.stderr)
        sys.exit(1)

    if not data_dir.exists():
        print(f"WARNING: Data directory not found: {data_dir}", file=sys.stderr)

    # Read manifest
    print(f"Reading manifest: {manifest_path}")
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    packages = manifest.get("packages", {})
    total = len(packages)
    print(f"Found {total} packages")

    # Create output directories
    packages_dir = output_dir / "packages"
    if not args.dry_run:
        packages_dir.mkdir(parents=True, exist_ok=True)

    # Build index and per-game files
    index_packages = {}
    errors = []
    skipped_dirs = []

    for i, (app_id, pkg_data) in enumerate(sorted(packages.items(), key=lambda x: x[0]), 1):
        game_name = pkg_data.get("gameName", f"Unknown ({app_id})")
        dir_name = pkg_data.get("dirName", "")
        last_updated = pkg_data.get("lastUpdated", "")

        # Calculate actual directory size
        size_bytes = 0
        file_count = 0
        if not args.skip_sizes and dir_name:
            game_dir = data_dir / dir_name
            if game_dir.exists():
                size_bytes = calculate_dir_size(game_dir)
                file_count = count_files(game_dir)
            else:
                skipped_dirs.append((app_id, game_name, dir_name))

        # Progress
        if i % 25 == 0 or i == total:
            print(f"  [{i}/{total}] Processing: {game_name} ({size_bytes:,} bytes)")

        # --- index.json entry ---
        index_entry = {
            "name": game_name,
            "v": last_updated,
            "sizeBytes": size_bytes,
        }
        # dirName is required for R2 download → extract flow
        if dir_name:
            index_entry["dirName"] = dir_name
        # dataUrl will be added in Phase 4 (R2 upload)
        # checksum will be added when compressed archives are created
        index_packages[app_id] = index_entry

        # --- packages/{appId}.json ---
        # Full package data + computed fields
        per_game = dict(pkg_data)  # shallow copy
        per_game["sizeBytes"] = size_bytes
        per_game["fileCount"] = file_count
        # Placeholder for future R2 integration
        per_game["dataUrl"] = None
        per_game["compressedSize"] = None
        per_game["compressedChecksum"] = None

        if not args.dry_run:
            per_game_path = packages_dir / f"{app_id}.json"
            with open(per_game_path, "w", encoding="utf-8") as f:
                json.dump(per_game, f, indent=2, ensure_ascii=False)

    # --- index.json ---
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    index = {
        "version": 5,
        "generatedAt": now,
        "totalPackages": total,
        "packages": index_packages,
    }

    index_path = output_dir / "index.json"
    if not args.dry_run:
        with open(index_path, "w", encoding="utf-8") as f:
            json.dump(index, f, indent=2, ensure_ascii=False)

    # --- Summary ---
    index_size = len(json.dumps(index, ensure_ascii=False).encode("utf-8"))

    print()
    print("=" * 60)
    print("SPLIT COMPLETE")
    print("=" * 60)
    print(f"  index.json:    {index_path} ({index_size:,} bytes)")
    print(f"  packages/:     {packages_dir} ({total} files)")
    print(f"  Total packages: {total}")
    print()

    # Size stats
    sizes = [v["sizeBytes"] for v in index_packages.values()]
    nonzero = [s for s in sizes if s > 0]
    if nonzero:
        print(f"  Size stats (from {len(nonzero)} directories):")
        print(f"    Total:   {sum(nonzero):>15,} bytes ({sum(nonzero) / (1024**3):.2f} GB)")
        print(f"    Average: {sum(nonzero) // len(nonzero):>15,} bytes")
        print(f"    Max:     {max(nonzero):>15,} bytes")
        print(f"    Min:     {min(nonzero):>15,} bytes")
        print(f"    Zero-size: {len(sizes) - len(nonzero)} packages")
    else:
        print("  WARNING: No size data calculated (--skip-sizes or missing dirs)")

    if skipped_dirs:
        print(f"\n  WARNING: {len(skipped_dirs)} directories not found:")
        for app_id, name, dir_name in skipped_dirs[:10]:
            print(f"    [{app_id}] {name} -> {dir_name}/")
        if len(skipped_dirs) > 10:
            print(f"    ... and {len(skipped_dirs) - 10} more")

    if errors:
        print(f"\n  ERRORS: {len(errors)}")
        for e in errors:
            print(f"    {e}")

    # Validation
    print("\n--- VALIDATION ---")
    ok = True

    # Check index package count
    if len(index_packages) != total:
        print(f"  FAIL: index has {len(index_packages)} packages, expected {total}")
        ok = False
    else:
        print(f"  OK: {total} packages in index")

    # Check per-game files exist
    if not args.dry_run:
        file_count = len(list(packages_dir.glob("*.json")))
        if file_count != total:
            print(f"  FAIL: {file_count} JSON files in packages/, expected {total}")
            ok = False
        else:
            print(f"  OK: {total} JSON files in packages/")

    # Check images cross-reference
    images_dir = output_dir / "images"
    if images_dir.exists():
        image_ids = {p.stem for p in images_dir.glob("*.png")}
        missing_images = set(index_packages.keys()) - image_ids
        extra_images = image_ids - set(index_packages.keys())
        if missing_images:
            print(f"  WARNING: {len(missing_images)} packages without images: {list(missing_images)[:5]}...")
        if extra_images:
            print(f"  INFO: {len(extra_images)} images without packages: {list(extra_images)[:5]}...")
        matching = len(set(index_packages.keys()) & image_ids)
        print(f"  OK: {matching}/{total} packages have matching images")

    if ok:
        print("\n  ALL CHECKS PASSED")
    else:
        print("\n  SOME CHECKS FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
