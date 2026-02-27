#!/usr/bin/env python3
"""
migrate_cdn_urls.py -- Migrate all manifest dataUrl fields to new CDN domain.

Updates index.json and all packages/*.json files to use cdn.makineceviri.net
instead of the old pub-xxx.r2.dev URL.

Usage:
    python scripts/migrate_cdn_urls.py                    # Migrate all
    python scripts/migrate_cdn_urls.py --dry-run          # Preview only
    python scripts/migrate_cdn_urls.py --assets-dir PATH  # Custom assets dir
"""

import argparse
import json
import sys
from pathlib import Path

OLD_DOMAIN = "pub-140c7bb439d7479b96e73779ff0a7c5f.r2.dev"
NEW_DOMAIN = "cdn.makineceviri.net"

DEFAULT_ASSETS_DIR = Path("C:/cedra/MakineAI-Assets")


def migrate_file(filepath: Path, dry_run: bool) -> int:
    """Replace old domain with new in a JSON file. Returns number of replacements."""
    text = filepath.read_text(encoding="utf-8")
    count = text.count(OLD_DOMAIN)

    if count == 0:
        return 0

    new_text = text.replace(OLD_DOMAIN, NEW_DOMAIN)

    if not dry_run:
        filepath.write_text(new_text, encoding="utf-8")

    return count


def main():
    parser = argparse.ArgumentParser(description="Migrate manifest URLs to new CDN domain")
    parser.add_argument("--assets-dir", type=Path, default=DEFAULT_ASSETS_DIR,
                        help=f"Assets directory (default: {DEFAULT_ASSETS_DIR})")
    parser.add_argument("--dry-run", action="store_true", help="Preview without writing")
    args = parser.parse_args()

    assets_dir = args.assets_dir
    if not assets_dir.exists():
        print(f"ERROR: Assets directory not found: {assets_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"CDN URL Migration: {OLD_DOMAIN} -> {NEW_DOMAIN}")
    print(f"Assets dir: {assets_dir}")
    if args.dry_run:
        print("MODE: DRY RUN\n")
    print()

    total_files = 0
    total_replacements = 0

    # 1. index.json
    index_path = assets_dir / "index.json"
    if index_path.exists():
        count = migrate_file(index_path, args.dry_run)
        if count > 0:
            print(f"  index.json: {count} URLs updated")
            total_files += 1
            total_replacements += count
        else:
            print(f"  index.json: already up to date")

    # 2. packages/*.json
    packages_dir = assets_dir / "packages"
    if packages_dir.exists():
        for pkg_file in sorted(packages_dir.glob("*.json")):
            count = migrate_file(pkg_file, args.dry_run)
            if count > 0:
                total_files += 1
                total_replacements += count

        pkg_count = len(list(packages_dir.glob("*.json")))
        updated = total_files - (1 if index_path.exists() else 0)
        print(f"  packages/: {updated}/{pkg_count} files updated")

    print(f"\nTotal: {total_replacements} URL replacements in {total_files} files")

    if args.dry_run:
        print("\n[DRY RUN] No files were modified. Run without --dry-run to apply.")


if __name__ == "__main__":
    main()
