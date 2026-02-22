#!/usr/bin/env python3
"""
Migrate manifest.json from v3 to v4 format.

Changes:
  - Standardize field names (name->gameName, folder->dirName, installNote->installNotes)
  - Normalize engine names (Unreal variants)
  - Extract referenceCheck/notes/source/status -> manifest_meta.json
  - Add tier, lastUpdated, sizeBytes, fileCount, checksum fields
  - Convert Spider-Man 2 / Indiana Jones / Hollow Knight 'versions' -> variants
  - Update Skyrim installNotes (elderscrollsturk -> tr-yama)
  - Sort packages by numeric AppID
  - Add top-level meta block
"""

import json
import shutil
import sys
from collections import OrderedDict
from datetime import datetime, timezone
from pathlib import Path

# ── Engine normalization map ──────────────────────────────────────────────────

ENGINE_MAP = {
    "Unreal": "Unreal Engine",
    "UE3": "Unreal Engine 3",
    "UE4": "Unreal Engine 4",
    "UE5": "Unreal Engine 5",
}

# ── Today's date ──────────────────────────────────────────────────────────────

TODAY = datetime.now(timezone.utc).strftime("%Y-%m-%d")
NOW_ISO = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def normalize_engine(engine: str) -> str:
    """Normalize engine name to canonical form."""
    return ENGINE_MAP.get(engine, engine)


def migrate_package(app_id: str, pkg: dict, meta: dict) -> dict:
    """Migrate a single package entry from v3 to v4."""
    out = OrderedDict()

    # === Basic info ===
    game_name = pkg.get("gameName") or pkg.get("name", "")
    out["gameName"] = game_name

    engine = pkg.get("engine", "")
    out["engine"] = normalize_engine(engine)

    dir_name = pkg.get("dirName") or pkg.get("folder", "")
    out["dirName"] = dir_name

    # === Distribution ===
    out["tier"] = "free"
    out["sizeBytes"] = 0
    out["fileCount"] = 0
    out["checksum"] = ""
    out["lastUpdated"] = TODAY

    # === Store IDs ===
    if "storeIds" in pkg:
        out["storeIds"] = pkg["storeIds"]

    # === Variants ===
    # Handle old 'versions' format (Spider-Man 2, Indiana Jones, Hollow Knight)
    if "versions" in pkg:
        versions_list = pkg["versions"]
        out["variantType"] = "version"
        out["variants"] = [v["version"] for v in versions_list if "version" in v]
    elif "variants" in pkg:
        if "variantType" in pkg:
            out["variantType"] = pkg["variantType"]
        out["variants"] = pkg["variants"]

    # === Install method ===
    if "installMethod" in pkg:
        im = pkg["installMethod"]
        if isinstance(im, dict):
            # Convert "direct" type to "script" with empty steps (backward compat)
            out["installMethod"] = im
        else:
            # String installMethod (legacy) — preserve as-is
            out["installMethod"] = im

    # === Install notes ===
    notes = pkg.get("installNotes") or pkg.get("installNote", "")
    if notes:
        # Skyrim: update elderscrollsturk reference
        if app_id == "489830" and "elderscrollsturk" in notes:
            notes = notes.replace("elderscrollsturk-tr-yama", "tr-yama").replace(
                "elderscrollsturk", "tr-yama"
            )
        out["installNotes"] = notes

    # === Version (if top-level, not from versions array) ===
    if "version" in pkg and "versions" not in pkg:
        out["version"] = pkg["version"]

    # === Special dialog ===
    if "specialDialog" in pkg:
        out["specialDialog"] = pkg["specialDialog"]

    # === Package ID (legacy pak/ reference) ===
    if "packageId" in pkg:
        out["packageId"] = pkg["packageId"]

    # === Contributors ===
    if "contributors" in pkg:
        out["contributors"] = pkg["contributors"]

    # === Extract meta fields ===
    if "referenceCheck" in pkg:
        if "referenceChecks" not in meta:
            meta["referenceChecks"] = {}
        meta["referenceChecks"][app_id] = pkg["referenceCheck"]

    if "notes" in pkg:
        if "devNotes" not in meta:
            meta["devNotes"] = {}
        meta["devNotes"][app_id] = pkg["notes"]

    if "source" in pkg:
        if "sources" not in meta:
            meta["sources"] = {}
        meta["sources"][app_id] = pkg["source"]

    if "status" in pkg:
        # Merge status into referenceCheck if exists
        if "referenceChecks" in meta and app_id in meta["referenceChecks"]:
            meta["referenceChecks"][app_id]["status"] = pkg["status"]

    return out


def migrate(manifest_path: Path):
    """Run full v3 -> v4 migration."""
    print(f"Reading {manifest_path}...")
    with open(manifest_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    if data.get("version", 0) >= 4:
        print("Already v4 or higher, skipping.")
        return

    packages = data.get("packages", {})
    print(f"Found {len(packages)} packages (v{data.get('version', '?')})")

    # Backup original
    backup_path = manifest_path.with_suffix(".v3.bak")
    if not backup_path.exists():
        shutil.copy2(manifest_path, backup_path)
        print(f"Backup saved to {backup_path}")
    else:
        print(f"Backup already exists at {backup_path}")

    # Migrate packages
    meta = OrderedDict()
    migrated = OrderedDict()

    for app_id, pkg in packages.items():
        migrated[app_id] = migrate_package(app_id, pkg, meta)

    # Sort by numeric AppID
    sorted_keys = sorted(migrated.keys(), key=lambda x: int(x))
    sorted_packages = OrderedDict((k, migrated[k]) for k in sorted_keys)

    # Build v4 manifest
    v4 = OrderedDict()
    v4["version"] = 4
    v4["generatedAt"] = NOW_ISO
    v4["meta"] = OrderedDict(
        [
            ("totalPackages", len(sorted_packages)),
            ("schema", "manifest.schema.json"),
        ]
    )
    v4["packages"] = sorted_packages

    # Write v4 manifest
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(v4, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"Written v4 manifest: {len(sorted_packages)} packages")

    # Write meta file
    meta_path = manifest_path.parent / "manifest_meta.json"
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"Written meta file: {meta_path}")

    # Stats
    engine_changes = sum(
        1
        for k in packages
        if normalize_engine(packages[k].get("engine", ""))
        != packages[k].get("engine", "")
    )
    versions_converted = sum(1 for v in packages.values() if "versions" in v)
    name_fixed = sum(
        1 for v in packages.values() if "name" in v and "gameName" not in v
    )
    ref_extracted = sum(1 for v in packages.values() if "referenceCheck" in v)

    print(f"\nMigration summary:")
    print(f"  Engine names normalized: {engine_changes}")
    print(f"  'versions' -> variants: {versions_converted}")
    print(f"  'name' -> gameName: {name_fixed}")
    print(f"  referenceCheck extracted: {ref_extracted}")


if __name__ == "__main__":
    base = Path(__file__).resolve().parent.parent
    manifest = base / "translation_data" / "manifest.json"

    if len(sys.argv) > 1:
        manifest = Path(sys.argv[1])

    if not manifest.exists():
        # Try relative to CWD
        manifest = Path("translation_data/manifest.json")

    if not manifest.exists():
        print(f"Manifest not found: {manifest}")
        sys.exit(1)

    migrate(manifest)
