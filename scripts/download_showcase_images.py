#!/usr/bin/env python3
"""
download_showcase_images.py — Validate Steam App IDs and download showcase images.

For each package in manifest.json:
1. Query Steam Store API to validate the steamAppId
2. Compare returned game name with manifest gameName (fuzzy match)
3. Download header_image for validated packages
4. Save to qml/resources/showcase/{steamAppId}.jpg

Usage:
    python scripts/download_showcase_images.py          # Normal (uses cache)
    python scripts/download_showcase_images.py --force   # Re-download everything
"""

import json
import os
import re
import sys
import time
import argparse
import unicodedata
from pathlib import Path
from datetime import datetime, timezone
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError

# --- Paths ---
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
MANIFEST_PATH = PROJECT_ROOT.parent / "translation_data" / "manifest.json"
SHOWCASE_DIR = PROJECT_ROOT / "qml" / "resources" / "showcase"
VALIDATION_CACHE = SCRIPT_DIR / "showcase_validation.json"

# --- Config ---
STEAM_API_URL = "https://store.steampowered.com/api/appdetails?appids={appid}"
REQUEST_DELAY = 1.5  # seconds between API calls
MAX_RETRIES = 3
BACKOFF_BASE = 5  # seconds
MIN_IMAGE_SIZE = 5 * 1024  # 5KB minimum for valid JPEG
JPEG_MAGIC = b"\xff\xd8\xff"
USER_AGENT = "Makine-Launcher/0.1 (showcase-downloader)"


def normalize(name: str) -> str:
    """Normalize game name for fuzzy comparison."""
    # Remove Unicode symbols BEFORE NFKD (NFKD decomposes TM/R into letters)
    name = re.sub(r"[\u2018\u2019\u201c\u201d\u2122\u00ae\u00a9\u2013\u2014]", "", name)
    # Unicode NFKD normalization (curly quotes -> straight, etc.)
    name = unicodedata.normalize("NFKD", name)
    name = name.lower().strip()
    # Replace underscores with spaces
    name = name.replace("_", " ")
    # Remove common punctuation and special chars
    name = re.sub(r"['\";:,!?./\\()\[\]{}\-]", "", name)
    # Collapse whitespace
    name = re.sub(r"\s+", " ", name).strip()
    return name


def fuzzy_match(manifest_name: str, steam_name: str) -> bool:
    """Check if two game names are similar enough to be a match."""
    a = normalize(manifest_name)
    b = normalize(steam_name)

    if a == b:
        return True

    # One contains the other
    if a in b or b in a:
        return True

    # Check if all significant words from manifest appear in steam name
    words_a = set(a.split())
    words_b = set(b.split())
    # Remove very short words (articles, etc.)
    words_a = {w for w in words_a if len(w) > 2}
    words_b = {w for w in words_b if len(w) > 2}

    if not words_a:
        return False

    overlap = len(words_a & words_b) / len(words_a)
    return overlap >= 0.7


def fetch_json(url: str) -> dict | None:
    """Fetch JSON from URL with retry logic."""
    for attempt in range(MAX_RETRIES):
        try:
            req = Request(url, headers={"User-Agent": USER_AGENT})
            with urlopen(req, timeout=15) as resp:
                if resp.status == 200:
                    return json.loads(resp.read().decode("utf-8"))
        except HTTPError as e:
            if e.code == 429:
                wait = BACKOFF_BASE * (2 ** attempt)
                print(f"  Rate limited, waiting {wait}s...")
                time.sleep(wait)
                continue
            if e.code in (404, 403):
                return None
            print(f"  HTTP {e.code} for {url}")
        except (URLError, TimeoutError, OSError) as e:
            print(f"  Network error: {e}")
            if attempt < MAX_RETRIES - 1:
                time.sleep(BACKOFF_BASE)
    return None


def download_image(url: str, dest: Path) -> int:
    """Download image file. Returns file size or 0 on failure."""
    for attempt in range(MAX_RETRIES):
        try:
            req = Request(url, headers={"User-Agent": USER_AGENT})
            with urlopen(req, timeout=30) as resp:
                data = resp.read()
                if len(data) < MIN_IMAGE_SIZE:
                    print(f"  Image too small ({len(data)} bytes)")
                    return 0
                if not data[:3] == JPEG_MAGIC:
                    print(f"  Not a valid JPEG (magic: {data[:3].hex()})")
                    return 0
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
                return len(data)
        except (HTTPError, URLError, TimeoutError, OSError) as e:
            if attempt < MAX_RETRIES - 1:
                time.sleep(2)
            else:
                print(f"  Download failed: {e}")
    return 0


def load_cache() -> dict:
    """Load validation cache."""
    if VALIDATION_CACHE.exists():
        try:
            return json.loads(VALIDATION_CACHE.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            pass
    return {"validated_at": None, "results": {}}


def save_cache(cache: dict):
    """Save validation cache."""
    cache["validated_at"] = datetime.now(timezone.utc).isoformat()
    VALIDATION_CACHE.write_text(
        json.dumps(cache, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def main():
    parser = argparse.ArgumentParser(description="Download Makine-Launcher showcase images")
    parser.add_argument("--force", action="store_true", help="Re-download everything")
    args = parser.parse_args()

    # Load manifest
    if not MANIFEST_PATH.exists():
        print(f"ERROR: Manifest not found at {MANIFEST_PATH}")
        sys.exit(1)

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    packages = manifest.get("packages", {})
    total = len(packages)

    print(f"=== Makine-Launcher Showcase Image Downloader ===")
    print(f"Manifest: {total} packages")
    print()

    # Load/reset cache
    cache = {} if args.force else load_cache()
    results = cache.get("results", {})

    # Ensure showcase dir exists
    SHOWCASE_DIR.mkdir(parents=True, exist_ok=True)

    # Stats
    stats = {"match": 0, "mismatch": 0, "not_found": 0, "cached": 0, "error": 0}
    mismatches = []
    not_found = []
    downloaded = []

    for i, (app_id, pkg) in enumerate(packages.items(), 1):
        game_name = pkg.get("gameName", "Unknown")
        dest = SHOWCASE_DIR / f"{app_id}.jpg"

        # Check cache (skip already validated + downloaded)
        if not args.force and app_id in results:
            cached = results[app_id]
            if cached.get("status") == "match" and dest.exists():
                stats["cached"] += 1
                stats["match"] += 1
                continue

        print(f"[{i}/{total}] {game_name} ({app_id})...", end=" ", flush=True)

        # Query Steam API
        api_data = fetch_json(STEAM_API_URL.format(appid=app_id))
        time.sleep(REQUEST_DELAY)

        if api_data is None or app_id not in api_data or not api_data[app_id].get("success", False):
            # API failed — try CDN cover art fallback chain
            cdn_base = f"https://cdn.akamai.steamstatic.com/steam/apps/{app_id}/"
            cdn_ok = False
            for cdn_type in ["library_600x900_2x.jpg", "library_600x900.jpg", "header.jpg"]:
                cdn_url = cdn_base + cdn_type
                size = download_image(cdn_url, dest)
                if size > 0:
                    print(f"NOT IN API, CDN OK ({size // 1024}KB, {cdn_type})")
                    results[app_id] = {
                        "status": "match",
                        "manifest_name": game_name,
                        "steam_name": "(CDN fallback)",
                        "image_url": cdn_url,
                        "image_size": size,
                    }
                    downloaded.append((app_id, game_name, size))
                    stats["match"] += 1
                    cdn_ok = True
                    break
            if not cdn_ok:
                print("NOT FOUND")
                results[app_id] = {
                    "status": "not_found",
                    "manifest_name": game_name,
                }
                not_found.append((app_id, game_name))
                stats["not_found"] += 1
            continue

        app_data = api_data[app_id]

        data = app_data.get("data", {})
        steam_name = data.get("name", "")
        image_url = data.get("header_image", "")

        # Validate name match
        if not fuzzy_match(game_name, steam_name):
            print(f"MISMATCH: Steam says \"{steam_name}\"")
            results[app_id] = {
                "status": "mismatch",
                "manifest_name": game_name,
                "steam_name": steam_name,
            }
            mismatches.append((app_id, game_name, steam_name))
            stats["mismatch"] += 1
            continue

        # Download cover art (library capsule 600x900 — vertical cover image)
        # Fallback chain: library_600x900_2x → library_600x900 → header → capsule_616x353
        cdn_base = f"https://cdn.akamai.steamstatic.com/steam/apps/{app_id}/"
        cover_urls = [
            cdn_base + "library_600x900_2x.jpg",
            cdn_base + "library_600x900.jpg",
            cdn_base + "header.jpg",
            cdn_base + "capsule_616x353.jpg",
        ]

        downloaded_ok = False
        for try_url in cover_urls:
            size = download_image(try_url, dest)
            if size > 0:
                url_type = try_url.split("/")[-1]
                print(f"OK ({size // 1024}KB, {url_type})")
                results[app_id] = {
                    "status": "match",
                    "manifest_name": game_name,
                    "steam_name": steam_name,
                    "image_url": try_url,
                    "image_size": size,
                }
                downloaded.append((app_id, game_name, size))
                stats["match"] += 1
                downloaded_ok = True
                break

        if not downloaded_ok:
            print("DOWNLOAD FAILED")
            results[app_id] = {
                "status": "error",
                "manifest_name": game_name,
                "steam_name": steam_name,
                "image_url": cover_urls[0],
            }
            stats["error"] += 1

    # Save cache
    cache["results"] = results
    save_cache(cache)

    # --- Report ---
    print()
    print("=" * 60)
    print("REPORT")
    print("=" * 60)

    # Count actual images on disk
    images_on_disk = list(SHOWCASE_DIR.glob("*.jpg"))
    total_size = sum(f.stat().st_size for f in images_on_disk)

    print(f"\nValidated & Downloaded: {stats['match']}")
    if stats["cached"] > 0:
        print(f"  (of which {stats['cached']} from cache)")
    for app_id, name, size in downloaded:
        print(f"  [OK] {app_id} - {name} ({size // 1024}KB)")

    if mismatches:
        print(f"\nMismatches (NEED REVIEW): {stats['mismatch']}")
        for app_id, manifest_name, steam_name in mismatches:
            print(f"  [!!] {app_id} - manifest: \"{manifest_name}\", Steam: \"{steam_name}\"")

    if not_found:
        print(f"\nNot Found on Steam: {stats['not_found']}")
        for app_id, name in not_found:
            existing = "using existing image" if (SHOWCASE_DIR / f"{app_id}.jpg").exists() else "no image"
            print(f"  [--] {app_id} - {name} ({existing})")

    if stats["error"] > 0:
        print(f"\nDownload Errors: {stats['error']}")

    print(f"\nTotal embedded: {len(images_on_disk)} images, {total_size / (1024 * 1024):.1f}MB")
    print(f"Validation cache: {VALIDATION_CACHE}")


if __name__ == "__main__":
    main()
