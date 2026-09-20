#!/usr/bin/env python3
"""Does the published catalogue describe the objects that are actually in R2?

Written because it did not, for six months, and nobody could see it. The
2026-03-13 bulk repack (ZSTD_LEVEL 9 -> 19) produced smaller packages and the
manifests were never regenerated: measured 2026-09-20, 197 of the 201 hosted
packages carried a size that no longer matched the object, and the download
path could not verify anything because verifying against those numbers would
have failed every install. One audit fixed it. This keeps it fixed.

Three questions, each of which was answered wrong in the field:

  1. Does every entry resolve?  A catalogue row the user can click but not
     download is worse than no row. Redirect sources (apex, hangar) are held to
     their externalUrl instead, because "no dataUrl" is normal for those and an
     earlier audit counted 24 of them as missing packages.

  2. Do the two manifests agree with R2 and with each other?  index.json says
     `size`/`checksum`, packages/<id>.json says `compressedSize`/
     `compressedChecksum`, and they are the same fact under two names. They
     drifted apart silently.

  3. Does api/v2/games/<id> still carry what the launcher needs?  The D1
     migration dropped variantType/variants for all 15 variant packages and the
     failure surfaced as "ambiguous at depth 1" install errors months later.

Exit code 1 on any finding, so it can gate a release.

Usage:
    python scripts/catalog_gate.py                  # HEAD only, ~1 min
    python scripts/catalog_gate.py --verify-hashes  # streams 6.6 GB, ~2 min
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import hashlib
import io
import json
import sys
import urllib.error
import urllib.request

CDN = "https://cdn.makineceviri.org"
API = "https://makineceviri.org/api/v2"
# Cloudflare's WAF answers Python-urllib with 403; a browser UA is required.
UA = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131 Safari/537.36"}
WORKERS = 6          # the WAF starts rate-limiting above this
OURS = CDN + "/"


def get_json(url, timeout=90):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        return json.load(r)


def head(url, timeout=60):
    """(status, content_length). Status is a string for transport failures."""
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA, method="HEAD"), timeout=timeout) as r:
            return r.status, int(r.headers.get("Content-Length") or 0)
    except urllib.error.HTTPError as e:
        return e.code, 0
    except Exception as e:                                   # noqa: BLE001
        return type(e).__name__, 0


def sha256_of(url, timeout=600):
    h = hashlib.sha256()
    n = 0
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            h.update(chunk)
            n += len(chunk)
    return h.hexdigest(), n


def norm_sum(v):
    """Accept both spellings in the wild: "sha256:<hex>" and a bare digest."""
    v = (v or "").strip()
    if v.lower().startswith("sha256:"):
        v = v[7:].strip()
    return v.lower() if len(v) == 64 else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify-hashes", action="store_true",
                    help="download every package and check its SHA-256 (~6.6 GB)")
    args = ap.parse_args()
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

    findings = []

    index = get_json(CDN + "/assets/index.json")
    entries = index.get("packages", index)
    print("index.json: %d girdi" % len(entries))

    ours = {a: e for a, e in entries.items()
            if str(e.get("dataUrl") or "").startswith(OURS)}
    redirects = {a: e for a, e in entries.items() if a not in ours}
    print("  kendi barindirdigimiz: %d | yonlendirme: %d" % (len(ours), len(redirects)))

    # --- 1. every entry resolves ------------------------------------------
    def probe_own(item):
        app_id, e = item
        status, length = head(e["dataUrl"])
        return app_id, e, status, length

    with cf.ThreadPoolExecutor(WORKERS) as ex:
        own_results = list(ex.map(probe_own, ours.items()))

    for app_id, e, status, length in own_results:
        if status != 200:
            findings.append("R2 nesnesi yok: %s (%s) -> %s"
                            % (app_id, e.get("name", ""), status))

    def probe_redirect(item):
        app_id, e = item
        target = e.get("externalUrl") or e.get("dataUrl")
        if not target:
            return app_id, e, "HEDEF YOK"
        return app_id, e, head(target)[0]

    with cf.ThreadPoolExecutor(WORKERS) as ex:
        for app_id, e, status in ex.map(probe_redirect, redirects.items()):
            if status != 200:
                findings.append("yonlendirme olu: %s (%s) -> %s"
                                % (app_id, e.get("name", ""), status))

    # --- 2. manifests agree with R2 and with each other --------------------
    real_size = {a: length for a, _, status, length in own_results if status == 200}

    for app_id, size in real_size.items():
        if entries[app_id].get("size") != size:
            findings.append("index.json boyutu yanlis: %s (%s != %s)"
                            % (app_id, entries[app_id].get("size"), size))

    def fetch_pkg(app_id):
        try:
            return app_id, get_json("%s/assets/packages/%s.json" % (CDN, app_id))
        except Exception as e:                               # noqa: BLE001
            return app_id, {"__err": "%s: %s" % (type(e).__name__, e)}

    with cf.ThreadPoolExecutor(WORKERS) as ex:
        pkgs = dict(ex.map(fetch_pkg, real_size))

    for app_id, pkg in pkgs.items():
        if "__err" in pkg:
            findings.append("packages/%s.json okunamadi: %s" % (app_id, pkg["__err"]))
            continue
        if pkg.get("compressedSize") != real_size[app_id]:
            findings.append("packages/%s.json boyutu yanlis: %s != %s"
                            % (app_id, pkg.get("compressedSize"), real_size[app_id]))
        a = norm_sum(entries[app_id].get("checksum"))
        b = norm_sum(pkg.get("compressedChecksum"))
        if a and b and a != b:
            findings.append("iki manifest ayni pakete farkli checksum veriyor: %s" % app_id)
        if not a:
            findings.append("index.json checksum yok/gecersiz: %s" % app_id)

    # --- 3. the API still carries what the launcher parses -----------------
    variant_ids = [a for a, p in pkgs.items()
                   if isinstance(p, dict) and (p.get("variants") or p.get("variantType"))]

    def check_api(app_id):
        try:
            d = get_json("%s/games/%s" % (API, app_id))
        except Exception as e:                               # noqa: BLE001
            return app_id, "okunamadi: %s" % type(e).__name__
        want_t = pkgs[app_id].get("variantType", "")
        want_v = pkgs[app_id].get("variants", [])
        if d.get("variantType") != want_t or d.get("variants") != want_v:
            return app_id, ("varyant alanlari CDN ile uyusmuyor: API(%r,%r) CDN(%r,%r)"
                            % (d.get("variantType"), d.get("variants"), want_t, want_v))
        return app_id, None

    if variant_ids:
        with cf.ThreadPoolExecutor(WORKERS) as ex:
            for app_id, problem in ex.map(check_api, variant_ids):
                if problem:
                    findings.append("api/v2/games/%s %s" % (app_id, problem))
        print("varyantli paket: %d (API alanlari denetlendi)" % len(variant_ids))

    # --- optional: the digests themselves ---------------------------------
    if args.verify_hashes:
        def verify(app_id):
            digest, n = sha256_of(entries[app_id]["dataUrl"])
            want = norm_sum(entries[app_id].get("checksum"))
            if want and digest != want:
                return "checksum uyusmuyor: %s (%s != %s)" % (app_id, digest[:16], want[:16])
            if n != real_size[app_id]:
                return "indirilen boyut HEAD ile uyusmuyor: %s" % app_id
            return None

        with cf.ThreadPoolExecutor(4) as ex:
            for problem in ex.map(verify, real_size):
                if problem:
                    findings.append(problem)
        print("%d paketin SHA-256'si dogrulandi" % len(real_size))

    # --- verdict -----------------------------------------------------------
    print()
    if not findings:
        print("TEMIZ — katalog R2 ile tutarli, API varyant alanlarini tasiyor.")
        return 0
    print("%d BULGU:" % len(findings))
    for f in findings:
        print("  -", f)
    return 1


if __name__ == "__main__":
    sys.exit(main())
