#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""catalog_reconcile.py -- planning sheet vs. what users can actually install.

Three questions the team cannot answer by eye, and each one is a user-visible
defect when the answer is wrong:

  * Is a cancelled or unfinished patch still installable?        -> LIVE_BUT_SHOULD_NOT_BE
  * Is a finished patch missing from the catalog?                -> DONE_BUT_UNPUBLISHED
  * Is something installable that nobody is tracking?            -> LIVE_BUT_UNTRACKED

The sheet is keyed by game NAME, the catalog by Steam AppID, so the join is
inherently lossy: no string rule can tell "Dead Space 2" from "Dead Space".
Add an "AppID" column to the sheet and this becomes exact — until then, matches
below `exact` confidence are reported as REVIEW, never as conclusions, and
known name drift is recorded in ALIASES.

Usage:
    python scripts/catalog_reconcile.py --sheet <file.xlsx>
    python scripts/catalog_reconcile.py --sheet <file.xlsx> --json
    python scripts/catalog_reconcile.py --sheet <file.xlsx> --fail-on-live-cancelled
"""

import argparse
import json
import re
import sys
import unicodedata
import urllib.request

for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

INDEX_URL = "https://cdn.makineceviri.org/assets/index.json"
USER_AGENT = "Makine-Launcher/0.1"

STATUS_DONE = {"Tamamlandı", "Güncelleme"}
STATUS_DEFERRED = {"Büyük Dosya"}
STATUS_NOT_LIVE = {"Z-İptal", "Yapılacak"}
STATUS_REDIRECT = {"Yönlendirme"}

# Teams we no longer have a distribution agreement with. Their translations may
# have been finished — that is why the sheet still says "Tamamlandı" — but we
# have no right to ship them, so ABSENT from the catalog is the correct state,
# not a publishing gap. The check that matters runs the other way: if one of
# their packages is still live, that is an agreement breach, not a bug report.
REMOVED_PARTNERS = {"calypso", "sinnerclown"}


def is_removed_partner(owner, note=""):
    blob = f"{owner} {note}".lower()
    return any(p in blob for p in REMOVED_PARTNERS)

# Sheet name -> catalog AppID, for drift no string rule should paper over.
# Every entry here is a name the two systems spell differently on purpose.
ALIASES = {
    "Civilization 5": "8930",
    "DOOM 2016": "379720",
    "Metal Gear Solid Delta Snake Eater": "2417610",
    "NieRAutomata": "524220",
    "SPORE™": "17390",
    "Resident Evil 8 Village": "1196590",
    "Mafia The Old County": "1941540",
}

TR = str.maketrans("çğıöşüÇĞİÖŞÜ", "cgiosuCGIOSU")
ROMAN = {"i": "1", "ii": "2", "iii": "3", "iv": "4", "v": "5", "vi": "6",
         "vii": "7", "viii": "8", "ix": "9", "x": "10", "xi": "11",
         "xii": "12", "xiii": "13", "xiv": "14", "xv": "15", "xvi": "16"}
NOISE = {"the", "a", "an", "edition", "definitive", "remastered", "remake",
         "goty", "complete", "deluxe", "ultimate", "enhanced", "special",
         "directors", "cut", "of", "year", "game", "and", "s"}


def tokens(name):
    s = unicodedata.normalize("NFKD", name or "").translate(TR).lower()
    s = re.sub(r"\(.*?\)", " ", s).replace("&", " and ")
    out = []
    for part in re.split(r"[^a-z0-9]+", s):
        if not part:
            continue
        part = ROMAN.get(part, part)
        if part not in NOISE:
            out.append(part)
    return out


def load_catalog():
    req = urllib.request.Request(INDEX_URL, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=120) as resp:
        data = json.load(resp)
    return {aid: meta.get("name", "") for aid, meta in data["packages"].items()}


def load_sheet(path):
    try:
        import openpyxl
    except ImportError:
        sys.exit("openpyxl gerekli: pip install openpyxl")

    ws = openpyxl.load_workbook(path, data_only=True).worksheets[0]
    rows = [["" if c is None else str(c).strip() for c in r]
            for r in ws.iter_rows(values_only=True)]
    rows = [r for r in rows if any(r)]

    header_at = next((i for i, r in enumerate(rows[:10]) if "Oyunlar" in r), 1)
    header = rows[header_at]
    idx = {name: header.index(name) for name in header if name}
    appid_col = idx.get("AppID", idx.get("AppId", idx.get("appid")))

    def cell(row, i):
        return row[i] if i is not None and len(row) > i else ""

    out = []
    for row in rows[header_at + 1:]:
        name = cell(row, idx.get("Oyunlar", 0))
        if not name:
            continue
        out.append({
            "name": name,
            "durum": cell(row, idx.get("Durum", 1)),
            "kim": cell(row, idx.get("Kimin", 2)),
            "not": cell(row, idx.get("Notlar", 3)),
            "appid": cell(row, appid_col).strip(),
        })
    return out


def match(name, cat_tokens):
    """Return (appids, confidence). Sequel-safe: a trailing number is a
    different game, so "Dead Space 2" never resolves to "Dead Space"."""
    t = tokens(name)
    if not t:
        return [], ""
    exact = [a for a, ct in cat_tokens.items() if ct == t]
    if exact:
        return exact, "exact"
    near = []
    for aid, ct in cat_tokens.items():
        if not ct:
            continue
        short, long_ = (t, ct) if len(t) <= len(ct) else (ct, t)
        if long_[:len(short)] != short:
            continue
        if any(x.isdigit() for x in long_[len(short):]):
            continue
        near.append(aid)
    return near, ("probable" if near else "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sheet", required=True)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--fail-on-live-cancelled", action="store_true")
    args = ap.parse_args()

    catalog = load_catalog()
    cat_tokens = {aid: tokens(name) for aid, name in catalog.items()}
    games = load_sheet(args.sheet)

    used_appid_col = any(g["appid"] for g in games)
    for g in games:
        if g["appid"] and g["appid"] in catalog:
            g["cdn"], g["conf"] = [g["appid"]], "appid"
        elif g["name"] in ALIASES and ALIASES[g["name"]] in catalog:
            g["cdn"], g["conf"] = [ALIASES[g["name"]]], "alias"
        else:
            g["cdn"], g["conf"] = match(g["name"], cat_tokens)

    solid = {"appid", "alias", "exact"}
    # Highest severity: a partner we split with is still being distributed.
    breach = [g for g in games
              if is_removed_partner(g["kim"], g["not"]) and g["cdn"]]
    withdrawn = [g for g in games
                 if is_removed_partner(g["kim"], g["not"]) and not g["cdn"]]
    live_but_not = [g for g in games
                    if g["durum"] in STATUS_NOT_LIVE and g["cdn"] and g["conf"] in solid]
    # Removed partners are excluded: unpublished is the correct state for them.
    unpublished = [g for g in games
                   if g["durum"] in STATUS_DONE and not g["cdn"]
                   and not is_removed_partner(g["kim"], g["not"])]
    deferred = [g for g in games
                if g["durum"] in STATUS_DEFERRED and not g["cdn"]]
    review = [g for g in games if g["conf"] == "probable"]

    tracked = {a for g in games for a in g["cdn"]}
    untracked = sorted(((a, n) for a, n in catalog.items() if a not in tracked),
                       key=lambda x: x[1])

    if args.json:
        print(json.dumps({
            "sheetRows": len(games), "catalog": len(catalog),
            "liveButShouldNotBe": [{"name": g["name"], "durum": g["durum"],
                                    "appIds": g["cdn"]} for g in live_but_not],
            "doneButUnpublished": [{"name": g["name"], "owner": g["kim"]}
                                   for g in unpublished],
            "liveButUntracked": [{"appId": a, "name": n} for a, n in untracked],
            "needsReview": [{"name": g["name"], "appIds": g["cdn"]} for g in review],
        }, ensure_ascii=False, indent=1))
    else:
        print("=" * 78)
        print(f"  KATALOG MUTABAKATI — tablo {len(games)} satır · "
              f"katalog {len(catalog)} paket")
        print(f"  eşleştirme: {'AppID sütunu' if used_appid_col else 'İSİM (kayıplı)'}")
        print("=" * 78)

        print(f"\n[0] AYRILINAN EKİP — HÂLÂ YAYINDA ({len(breach)})"
              f"   [dağıtım hakkı yok]")
        if not breach:
            print(f"    temiz — {len(withdrawn)} kayıt tamamen çekilmiş")
        for g in breach:
            ids = ", ".join(f"{a} ({catalog[a]})" for a in g["cdn"])
            print(f"    !! {g['kim'] or '?':<12} {g['name'][:34]:<34} -> {ids}")

        print(f"\n[1] YAYINDA AMA OLMAMALI ({len(live_but_not)})")
        for g in live_but_not:
            ids = ", ".join(f"{a} ({catalog[a]})" for a in g["cdn"])
            print(f"    {g['durum']:<10} {g['name'][:38]:<38} -> {ids}")

        print(f"\n[2] TAMAMLANMIŞ AMA YAYINLANMAMIŞ ({len(unpublished)})")
        by_owner = {}
        for g in unpublished:
            by_owner.setdefault(g["kim"] or "(sahipsiz)", []).append(g)
        for owner, lst in sorted(by_owner.items(), key=lambda kv: -len(kv[1])):
            print(f"  -- {owner} ({len(lst)})")
            for g in lst:
                note = f"  // {g['not'][:48]}" if g["not"] else ""
                print(f"       {g['name'][:46]}{note}")

        print(f"\n[3] BİLİNÇLİ ERTELENEN ({len(deferred)})")
        for g in deferred:
            print(f"    {g['name'][:46]}  // {g['not'][:40]}")

        print(f"\n[4] YAYINDA AMA TABLODA YOK ({len(untracked)})")
        for aid, name in untracked:
            print(f"    {aid:<10} {name[:56]}")

        print(f"\n[5] İSİM EŞLEŞMESİ ŞÜPHELİ — elle doğrula ({len(review)})")
        for g in review:
            ids = ", ".join(f"{a} ({catalog[a]})" for a in g["cdn"])
            print(f"    {g['durum']:<10} {g['name'][:34]:<34} -> {ids}")

        if not used_appid_col:
            print("\n  NOT: Tabloda AppID sütunu yok. Eklenirse [5] tamamen kaybolur")
            print("       ve [1]-[4] kesinleşir.")

    if args.fail_on_live_cancelled and live_but_not:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
