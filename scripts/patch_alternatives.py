#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""patch_alternatives.py -- the catalogue's optional patch keys, checked the
way the launcher reads them.

A game may have several patches: the entry's own keys stay the DEFAULT patch
(what launchers before 0.2.x install) and "alternatives" lists the others.
Every key here is additive and optional (shared contract Makine-WEB <->
Makine-Launcher, 2026-11-24):

    "label":       the default patch's own name           (str, <= 60)
    "gameVersion": {label: str, steamBuild: int >= 0, date: "YYYY-MM-DD"}
    "alternatives": [ {id, label, description, v, sizeBytes, size, dataUrl,
                       checksum, dirName, contributors, gameVersion}, ... ]  (<= 20)

The launcher drops what it cannot use without a word (package_catalog.cpp
parseAlternatives, catalogstore.cpp): an alternative with a bad or repeated
id, or without a label, never reaches the player. That silence is why these
are findings here.

Used by catalog_validate.py, catalog_gate.py and package_pipeline.py.

    python scripts/patch_alternatives.py <index.json>   # check a local index
"""

import json
import re
import sys

ALT_ID = re.compile(r"^[a-z0-9][a-z0-9-]{0,31}$")
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
SHA = re.compile(r"^(sha256:)?[0-9a-fA-F]{64}$")
MAX_ALTERNATIVES = 20
MAX_LABEL = 60
MAX_DESCRIPTION = 300


def _is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def check_game_version(gv, where):
    """Problems with one "gameVersion" value (absent is fine)."""
    if gv is None:
        return []
    if not isinstance(gv, dict):
        return [f"{where}: gameVersion nesne değil ({type(gv).__name__})"]
    out = []
    if "label" in gv and not isinstance(gv["label"], str):
        out.append(f"{where}: gameVersion.label metin değil")
    if "steamBuild" in gv and not (_is_int(gv["steamBuild"]) and gv["steamBuild"] >= 0):
        out.append(f"{where}: gameVersion.steamBuild negatif olmayan tam sayı değil: "
                   f"{gv['steamBuild']!r}")
    if "date" in gv and gv["date"] not in ("", None) and not (
            isinstance(gv["date"], str) and DATE.match(gv["date"])):
        out.append(f"{where}: gameVersion.date YYYY-AA-GG değil: {gv['date']!r}")
    return out


def check_entry(app_id, entry):
    """Problems with one index entry's optional patch keys. [] = clean."""
    if not isinstance(entry, dict):
        return []
    out = []
    label = entry.get("label")
    if label is not None and (not isinstance(label, str) or len(label) > MAX_LABEL):
        out.append(f"{app_id}: label metin değil ya da {MAX_LABEL} karakterden uzun")
    out += check_game_version(entry.get("gameVersion"), app_id)

    alts = entry.get("alternatives")
    if alts is None:
        return out
    if not isinstance(alts, list):
        return out + [f"{app_id}: alternatives liste değil ({type(alts).__name__})"]
    if len(alts) > MAX_ALTERNATIVES:
        out.append(f"{app_id}: {len(alts)} alternatif var, en çok {MAX_ALTERNATIVES}")

    ids = set()
    dirs = {entry.get("dirName") or ""} - {""}
    for i, alt in enumerate(alts):
        where = f"{app_id}/alternatives[{i}]"
        if not isinstance(alt, dict):
            out.append(f"{where}: nesne değil")
            continue
        aid = alt.get("id")
        if not isinstance(aid, str) or not ALT_ID.match(aid) or aid == "default":
            out.append(f"{where}: id geçersiz ({aid!r}) — ^[a-z0-9][a-z0-9-]{{0,31}}$, 'default' olamaz")
        elif aid in ids:
            out.append(f"{where}: id '{aid}' tekrar ediyor")
        else:
            ids.add(aid)
            where = f"{app_id}/{aid}"
        lab = alt.get("label")
        if not isinstance(lab, str) or not lab.strip():
            out.append(f"{where}: label yok (launcher bu yamayı göstermez)")
        elif len(lab) > MAX_LABEL:
            out.append(f"{where}: label {MAX_LABEL} karakterden uzun")
        desc = alt.get("description")
        if desc is not None and (not isinstance(desc, str) or len(desc) > MAX_DESCRIPTION):
            out.append(f"{where}: description metin değil ya da {MAX_DESCRIPTION} karakterden uzun")
        if "v" in alt and not isinstance(alt["v"], str):
            out.append(f"{where}: v metin değil")
        for key in ("sizeBytes", "size"):
            if key in alt and not (_is_int(alt[key]) and alt[key] >= 0):
                out.append(f"{where}: {key} negatif olmayan tam sayı değil")
        url, chk = alt.get("dataUrl") or "", alt.get("checksum") or ""
        if bool(url) != bool(chk):
            out.append(f"{where}: dataUrl ile checksum birlikte olmalı "
                       f"(dataUrl {'var' if url else 'yok'}, checksum {'var' if chk else 'yok'})")
        if chk and not (isinstance(chk, str) and SHA.match(chk)):
            out.append(f"{where}: checksum sha256 değil")
        d = alt.get("dirName") or ""
        if not isinstance(d, str):
            out.append(f"{where}: dirName metin değil")
        elif d:
            if d in dirs:
                out.append(f"{where}: dirName '{d}' başka bir yamanınkiyle aynı "
                           "(her yama kendi klasörüne çıkar)")
            dirs.add(d)
        contribs = alt.get("contributors")
        if contribs is not None and not isinstance(contribs, list):
            out.append(f"{where}: contributors liste değil")
        out += check_game_version(alt.get("gameVersion"), where)
    return out


def iter_alternatives(entry):
    """(id, alt) for every well-formed alternative, index order."""
    if not isinstance(entry, dict) or not isinstance(entry.get("alternatives"), list):
        return
    seen = set()
    for alt in entry["alternatives"]:
        if not isinstance(alt, dict):
            continue
        aid = alt.get("id")
        if isinstance(aid, str) and ALT_ID.match(aid) and aid != "default" and aid not in seen:
            seen.add(aid)
            yield aid, alt


def check_index(index):
    """Every problem in an index.json ("packages" and "soon")."""
    out = []
    for section in ("packages", "soon"):
        entries = index.get(section) if isinstance(index, dict) else None
        if isinstance(entries, dict):
            for app_id, entry in entries.items():
                out += check_entry(app_id, entry)
    return out


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8") as f:
        problems = check_index(json.load(f))
    for p in problems:
        print("  -", p)
    print(f"{len(problems)} bulgu" if problems else "temiz")
    return 1 if problems else 0


if __name__ == "__main__":
    for _s in (sys.stdout, sys.stderr):
        if hasattr(_s, "reconfigure"):
            _s.reconfigure(encoding="utf-8", errors="replace")
    sys.exit(main(sys.argv))
