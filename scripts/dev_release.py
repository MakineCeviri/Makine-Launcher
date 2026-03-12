#!/usr/bin/env python3
"""
dev_release.py -- Publish dev build via GitHub Releases for auto-update.

Flow:
  1. Locate built EXE (build/dev/MakineAI.exe)
  2. Compute SHA-256 checksum
  3. Create git tag + GitHub Release (prerelease)
  4. Upload EXE as release asset
  5. Dev builds auto-detect via GitHub Releases API

Usage:
    python scripts/dev_release.py                    # Auto-detect version
    python scripts/dev_release.py --dry-run           # Preview without changes
    python scripts/dev_release.py --notes "Bug fix"   # Custom release notes
"""

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent

# Build output locations (in priority order)
EXE_CANDIDATES = [
    PROJECT_DIR / "build" / "dev" / "MakineAI.exe",
    PROJECT_DIR / "build" / "release" / "MakineAI.exe",
    PROJECT_DIR / "build" / "release-static" / "MakineAI.exe",
]


def get_version() -> str:
    """Read version from CMakeLists.txt."""
    cmake = PROJECT_DIR / "CMakeLists.txt"
    content = cmake.read_text()
    version = ""
    suffix = ""
    for line in content.splitlines():
        s = line.strip()
        if "VERSION" in s and not s.startswith("#"):
            parts = s.split()
            for i, p in enumerate(parts):
                if p == "VERSION" and i + 1 < len(parts):
                    v = parts[i + 1].rstrip(")")
                    if v and v[0].isdigit():
                        version = v
                    break
        if s.lower().startswith("set(makineai_version_suffix"):
            if '"' in s:
                suffix = s.split('"')[1]
    return f"{version}-{suffix}" if version and suffix else version or "0.1.0"


def sha256_file(path: Path) -> str:
    """Compute SHA-256 hex digest."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def find_exe() -> Path | None:
    """Find the built EXE."""
    for p in EXE_CANDIDATES:
        if p.exists():
            return p
    return None


def main():
    parser = argparse.ArgumentParser(description="Publish dev build via GitHub Releases")
    parser.add_argument("--dry-run", action="store_true", help="Preview without changes")
    parser.add_argument("--notes", default="", help="Release notes")
    parser.add_argument("--exe", help="Path to EXE (auto-detect if omitted)")
    args = parser.parse_args()

    version = get_version()
    tag = f"v{version}"

    # Find EXE
    exe_path = Path(args.exe) if args.exe else find_exe()
    if not exe_path or not exe_path.exists():
        print(f"ERROR: No built EXE found. Run 'just dev' first.", file=sys.stderr)
        print(f"  Searched: {[str(p) for p in EXE_CANDIDATES]}", file=sys.stderr)
        sys.exit(1)

    exe_size = exe_path.stat().st_size
    checksum = sha256_file(exe_path)

    print(f"Version:  {version}")
    print(f"Tag:      {tag}")
    print(f"EXE:      {exe_path}")
    print(f"Size:     {exe_size:,} bytes ({exe_size / (1024*1024):.1f} MB)")
    print(f"SHA-256:  {checksum}")
    print()

    if args.dry_run:
        print("[DRY RUN] Would create:")
        print(f"  Git tag: {tag}")
        print(f"  GitHub Release: {tag} (prerelease)")
        print(f"  Asset: MakineAI.exe ({exe_size:,} bytes)")
        return

    # Build release notes with checksum
    notes = args.notes or f"Dev build {version}"
    release_body = f"{notes}\n\nSHA256:{checksum}"

    # Check if tag already exists
    r = subprocess.run(["git", "tag", "-l", tag], capture_output=True, text=True,
                       cwd=str(PROJECT_DIR))
    if tag in r.stdout:
        print(f"Tag {tag} already exists — deleting to recreate")
        subprocess.run(["git", "tag", "-d", tag], cwd=str(PROJECT_DIR))
        subprocess.run(["git", "push", "origin", f":refs/tags/{tag}"],
                       capture_output=True, cwd=str(PROJECT_DIR))
        # Also delete existing GitHub release
        subprocess.run(["gh", "release", "delete", tag, "--yes"],
                       capture_output=True, cwd=str(PROJECT_DIR))

    # Create tag
    print(f"Creating tag {tag}...")
    subprocess.run(["git", "tag", tag], check=True, cwd=str(PROJECT_DIR))
    subprocess.run(["git", "push", "origin", tag], check=True, cwd=str(PROJECT_DIR))
    print(f"  Tag pushed")

    # Create GitHub Release with EXE asset
    print(f"Creating GitHub Release {tag}...")
    r = subprocess.run(
        ["gh", "release", "create", tag,
         str(exe_path),
         "--title", f"MakineAI {tag}",
         "--notes", release_body,
         "--prerelease"],
        capture_output=True, text=True, cwd=str(PROJECT_DIR)
    )

    if r.returncode != 0:
        print(f"ERROR: gh release create failed: {r.stderr[:300]}", file=sys.stderr)
        sys.exit(1)

    release_url = r.stdout.strip()
    print(f"  Release: {release_url}")

    # Summary
    print()
    print("=" * 60)
    print("  DEV RELEASE COMPLETE")
    print("=" * 60)
    print(f"  Version:  {version}")
    print(f"  Tag:      {tag}")
    print(f"  Release:  {release_url}")
    print(f"  Checksum: sha256:{checksum[:16]}...")
    print()
    print("  Dev launchers will auto-detect this via GitHub API.")


if __name__ == "__main__":
    main()
