#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""catalog_validate.py -- validate the published catalog against what the
launcher can actually execute.

Every rule here mirrors real launcher code, and every one of them exists
because a user hit it:

  MANGLED     non-ASCII directory names in a recipe. The packaging tar writes
              "Türkçe Yama" as "T?rk?e Yama"; the extractor rewrites '?' to '_'
              because Windows rejects it, and the recipe then looks for a name
              that is not on disk. 575 events / ~106 users on Elden Ring.
  ACTION      a step action executeStep() does not implement -> the recipe is
              refused, or (before the pre-flight guard) half-applied.
  FIELD       a known action whose required parameter is empty. The parser reads
              action/src/dest/exe/fallback/workDir/language/args only, so a
              recipe written with "cmd"/"pattern"/"to"/"patch" parses into a
              well-formed step that cannot do anything.
  SHAPE       installMethod given as a bare string. parseInstallMethod() only
              accepted objects, so the type was dropped and the package fell
              through to a plain overlay copy — for "modengine" that is a wrong
              install reported as success.
  GATE        a non-overlay-safe type with no steps -> honesty gate, package is
              not installable at all.
  NODATA      listed to users with nothing to download and no redirect.
  EMPTY       data file present but zero bytes.
  TRAVERSAL   a step destination that escapes the game directory.

Usage:
    python scripts/catalog_validate.py
    python scripts/catalog_validate.py --json
    python scripts/catalog_validate.py --strict     # non-zero exit on findings
    python scripts/catalog_validate.py --no-network # skip data-file HEAD checks
"""

import argparse
import json
import sys
import unicodedata
import urllib.error
import urllib.request
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor

for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

CDN = "https://cdn.makineceviri.org"
UA = {"User-Agent": "Makine-Launcher/0.1"}

# Mirrors kKnownStepActions in localpackagemanager.cpp.
KNOWN_ACTIONS = {"copy", "copyFile", "copyDir", "delete", "installFont",
                 "run", "copyToDesktop", "rename", "setSteamLanguage"}
# Mirrors missingStepField() in installsteprules.h.
REQUIRED = {
    "copy": ("src", "dest"), "copyFile": ("src", "dest"),
    "copyDir": ("src", "dest"), "rename": ("src", "dest"),
    "copyToDesktop": ("src", "dest"), "delete": ("dest",),
    "installFont": ("src",), "run": ("exe",), "setSteamLanguage": ("language",),
}
# Mirrors kOverlaySafeTypes.
OVERLAY_SAFE = {"", "direct", "overlay", "copy", "file-replace"}
# Types that intentionally redirect instead of installing.
REDIRECT_TYPES = {"external", "forge_inject", "workshop", "installer",
                  "paradox-mod", "unityPatch", "modengine", "d2r_mod", "vpatch"}
# Keys the step parser reads. Anything else in a step is silently discarded.
PARSED_KEYS = {"action", "src", "dest", "exe", "fallback", "workDir",
               "language", "args"}


def fetch(url, as_json=True):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=90) as r:
        return json.load(r) if as_json else r.read()


def head(url):
    req = urllib.request.Request(url, headers=UA, method="HEAD")
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status, int(r.headers.get("content-length") or 0)
    except urllib.error.HTTPError as e:
        return e.code, 0
    except Exception:
        return 0, 0


def is_mangled_risk(text):
    """True when the packaging tar would not round-trip this name.

    Anything outside ASCII is replaced with '?' on write and '_' on extract, so
    the recipe's spelling stops matching the directory that appears on disk.
    """
    return any(ord(ch) > 127 for ch in (text or ""))


def walk_steps(im):
    """Yield (location, step) for every step object anywhere in installMethod."""
    if not isinstance(im, dict):
        return
    if isinstance(im.get("steps"), list):
        for s in im["steps"]:
            if isinstance(s, dict):
                yield "steps", s
    for opt in im.get("options") or []:
        if not isinstance(opt, dict):
            continue
        for s in opt.get("steps") or []:
            if isinstance(s, dict):
                yield f"option:{opt.get('id', '?')}", s
    for key, arr in (im.get("combinedSteps") or {}).items():
        for s in arr or []:
            if isinstance(s, dict):
                yield f"combined:{key}", s
    for vname, vc in (im.get("variantInstallOptions") or {}).items():
        if not isinstance(vc, dict):
            continue
        for opt in vc.get("options") or []:
            for s in (opt or {}).get("steps") or []:
                if isinstance(s, dict):
                    yield f"variant:{vname}", s
        for key, arr in (vc.get("combinedSteps") or {}).items():
            for s in arr or []:
                if isinstance(s, dict):
                    yield f"variant:{vname}/{key}", s


def check_package(app_id, meta, no_network):
    findings = []

    def add(code, detail):
        findings.append({"appId": app_id, "name": meta.get("name", ""),
                         "code": code, "detail": detail})

    try:
        pkg = fetch(f"{CDN}/assets/packages/{app_id}.json")
    except Exception as exc:
        add("DETAIL404", f"detay JSON alınamadı: {exc}")
        return findings

    im = pkg.get("installMethod")
    if isinstance(im, str):
        add("SHAPE", f'installMethod düz string ("{im}") — nesne bekleniyor')
        im = {"type": im}
    elif im is None:
        im = {}
    elif not isinstance(im, dict):
        add("SHAPE", f"installMethod beklenmeyen tip: {type(im).__name__}")
        im = {}

    itype = im.get("type", "") or ""
    steps = list(walk_steps(im))

    # subDir names travel through the same tar as the payload.
    for opt in im.get("options") or []:
        sub = (opt or {}).get("subDir") or ""
        if is_mangled_risk(sub):
            add("MANGLED", f'option "{opt.get("id", "?")}" subDir="{sub}" — '
                           "çıkarıcı bunu bozar")

    for where, s in steps:
        action = s.get("action") or ""
        if action and action not in KNOWN_ACTIONS:
            add("ACTION", f"{where}: '{action}' çalıştırıcıda yok")
        elif action:
            missing = [f for f in REQUIRED.get(action, ()) if not s.get(f)]
            if missing:
                unread = sorted(set(s) - PARSED_KEYS)
                hint = f" (okunmayan anahtar: {', '.join(unread)})" if unread else ""
                add("FIELD", f"{where}: '{action}' için {'/'.join(missing)} boş{hint}")
        for field in ("src", "dest", "exe"):
            val = s.get(field) or ""
            if is_mangled_risk(val):
                add("MANGLED", f'{where}: {field}="{val}" ASCII dışı')
            if field == "dest" and (".." in str(val).split("/") or
                                    ".." in str(val).split("\\")):
                add("TRAVERSAL", f"{where}: dest oyun klasörünün dışına çıkıyor: {val}")

    has_steps = bool(steps)
    ext = pkg.get("externalUrl") or meta.get("externalUrl") or ""
    # "userPath" installs without steps: the payload is copied to
    # <home>/<target> (localpackagemanager.cpp:1320). It is only broken when
    # that target is missing, which would send it to the honesty gate.
    if itype == "userPath":
        if not im.get("target"):
            add("GATE", "tip='userPath' ama 'target' alanı yok — kurulamaz")
    elif itype not in OVERLAY_SAFE and not has_steps and itype not in REDIRECT_TYPES:
        add("GATE", f"tip='{itype}', adım yok — otomatik kurulamaz")

    data_url = meta.get("dataUrl") or pkg.get("dataUrl") or ""
    if not data_url and not ext and itype not in REDIRECT_TYPES:
        add("NODATA", "indirilecek dosya yok ve yönlendirme de yok")
    elif data_url and not no_network:
        status, size = head(data_url)
        if status != 200:
            add("HTTP", f"veri dosyası HTTP {status}")
        elif size == 0:
            add("EMPTY", "veri dosyası 0 bayt")

    return findings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--no-network", action="store_true")
    args = ap.parse_args()

    index = fetch(f"{CDN}/assets/index.json")
    packages = index["packages"]
    print(f"katalog sürümü {index.get('version')} · {len(packages)} paket "
          f"· {index.get('generatedAt', '')}", file=sys.stderr)

    findings = []
    with ThreadPoolExecutor(max_workers=16) as pool:
        futures = [pool.submit(check_package, aid, meta, args.no_network)
                   for aid, meta in packages.items()]
        for f in futures:
            findings.extend(f.result())

    if args.json:
        print(json.dumps(findings, ensure_ascii=False, indent=1))
    else:
        by_code = defaultdict(list)
        for f in findings:
            by_code[f["code"]].append(f)
        order = ["MANGLED", "ACTION", "FIELD", "SHAPE", "TRAVERSAL",
                 "DETAIL404", "NODATA", "EMPTY", "HTTP", "GATE"]
        print("=" * 78)
        print(f"  KATALOG DOĞRULAMA — {len(findings)} bulgu / {len(packages)} paket")
        print("=" * 78)
        for code in order + sorted(set(by_code) - set(order)):
            rows = by_code.get(code)
            if not rows:
                continue
            print(f"\n[{code}] {len(rows)}")
            for r in rows:
                print(f"    {r['appId']:<9} {r['name'][:30]:<31} {r['detail'][:78]}")
        if not findings:
            print("\n  temiz.")

    # MANGLED and FIELD break installs silently; they gate the exit code.
    blocking = [f for f in findings
                if f["code"] in {"MANGLED", "ACTION", "FIELD", "SHAPE", "TRAVERSAL"}]
    if args.strict and blocking:
        print(f"\nSTRICT: {len(blocking)} engelleyici bulgu", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
