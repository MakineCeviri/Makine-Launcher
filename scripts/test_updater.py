#!/usr/bin/env python3
"""
Local HTTP server for testing Makine-Launcher auto-updater (self-swap).

Usage:
    python scripts/test_updater.py

Then run Makine-Launcher with:
    MAKINE_UPDATE_URL=http://localhost:8765/update.json ./build/dev/Makine-Launcher.exe
"""

import hashlib
import http.server
import json
import shutil
import sys
import tempfile
from pathlib import Path

PORT = 8765
EXE_NAME = "Makine-Launcher.exe"
FAKE_VERSION = "99.0.0"

# Locate the dev build EXE
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
BUILD_EXE = PROJECT_ROOT / "build" / "dev" / EXE_NAME


def setup_serve_dir() -> Path:
    """Copy build EXE to a temp directory and generate update.json alongside it."""
    serve_dir = Path(tempfile.mkdtemp(prefix="makine_update_test_"))

    if not BUILD_EXE.exists():
        print(f"ERROR: Build EXE not found at {BUILD_EXE}")
        print("Run 'just dev' first to build the application.")
        sys.exit(1)

    # Copy EXE as the "new version"
    dest_exe = serve_dir / EXE_NAME
    print(f"Copying {BUILD_EXE} -> {dest_exe}")
    shutil.copy2(BUILD_EXE, dest_exe)

    # Compute SHA256
    sha256 = hashlib.sha256(dest_exe.read_bytes()).hexdigest()
    size = dest_exe.stat().st_size

    # Generate update.json
    update_info = {
        "version": FAKE_VERSION,
        "url": f"http://localhost:{PORT}/{EXE_NAME}",
        "checksum": f"sha256:{sha256}",
        "size": size,
        "notes": "Test update from local server",
        "mandatory": False,
    }

    update_json_path = serve_dir / "update.json"
    update_json_path.write_text(json.dumps(update_info, indent=2), encoding="utf-8")

    print(f"\nupdate.json content:")
    print(json.dumps(update_info, indent=2))
    return serve_dir


def main():
    serve_dir = setup_serve_dir()

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(serve_dir), **kwargs)

    print(f"\n{'='*60}")
    print(f"  Makine-Launcher Update Test Server")
    print(f"  Serving on http://localhost:{PORT}")
    print(f"  Fake version: {FAKE_VERSION}")
    print(f"  Serve dir: {serve_dir}")
    print(f"{'='*60}")
    print(f"\nTo test, run in another terminal:")
    print(f"  export MAKINE_UPDATE_URL=http://localhost:{PORT}/update.json")
    print(f'  export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"')
    print(f"  ./build/dev/Makine-Launcher.exe")
    print(f"\nVerification checklist:")
    print(f"  1. NavBar download icon appears (pulse animation)")
    print(f'  2. Tooltip: "Guncelleme indir (v{FAKE_VERSION})"')
    print(f"  3. Click -> download starts, progress ring spins")
    print(f"  4. Download completes -> green sync icon")
    print(f"  5. Click -> app closes, self-swap occurs")
    print(f"  6. Makine-Launcher.exe.old created, new EXE in place")
    print(f"  7. New version launches with --post-update")
    print(f"\nPress Ctrl+C to stop.\n")

    with http.server.HTTPServer(("localhost", PORT), Handler) as server:
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down test server.")
        finally:
            # Clean up temp dir
            shutil.rmtree(serve_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
