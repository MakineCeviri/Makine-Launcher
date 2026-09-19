#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri
"""Katalogdaki installNotes metinlerini paketin GERCEK icerigiyle karsilastirir.

Neden var: 2026-09-19'da sahadan "Far Cry 6 yama kurulu ama oyun hala Ingilizce"
bildirimi geldi. Paket dogru kuruluyordu; not yanlisti - kullaniciya oyunda hic
bulunmayan bir "Turkce" secenegini secmesi soyleniyordu. Not serbest metin
oldugu icin hicbir asamada dogrulanmiyordu.

Bu arac uc soruyu sorar:
  1. Not kullaniciya bir dil sectiriyor mu, hangi dili?
  2. Paket gercekte hangi dil dosyalarina yaziyor?
  3. Ikisi birbirini tutuyor mu?

Kullanim:
    python scripts/catalog_notes_audit.py              # sadece kurulum adimlari
    python scripts/catalog_notes_audit.py --deep       # paketleri indirip acar
    python scripts/catalog_notes_audit.py --deep --max-mb 50
"""

import argparse
import io
import json
import os
import re
import sys
import tarfile
import urllib.request
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

CDN = "https://cdn.makineceviri.org"
UA = {"User-Agent": "Mozilla/5.0 (makine-catalog-audit)"}

# Bir dil kodunun/adinin Turkce karsiligi. Ayni tablo C++ tarafinda
# qml/src/services/postinstallrules.h icinde de var - ikisi birlikte guncellenir.
LANGS = {
    "english": "İngilizce", "eng": "İngilizce", "en": "İngilizce",
    "int": "İngilizce",                      # Unreal "International" = Ingilizce
    "french": "Fransızca", "francais": "Fransızca", "fra": "Fransızca", "fr": "Fransızca",
    "german": "Almanca", "deutsch": "Almanca", "deu": "Almanca", "ger": "Almanca", "de": "Almanca",
    "spanish": "İspanyolca", "espanol": "İspanyolca", "esn": "İspanyolca", "esp": "İspanyolca", "es": "İspanyolca",
    "italian": "İtalyanca", "italiano": "İtalyanca", "ita": "İtalyanca", "it": "İtalyanca",
    "polish": "Lehçe", "polski": "Lehçe", "pol": "Lehçe", "pl": "Lehçe",
    "russian": "Rusça", "rus": "Rusça", "ru": "Rusça",
    "portuguese": "Portekizce", "ptb": "Portekizce", "pt": "Portekizce",
    "japanese": "Japonca", "jpn": "Japonca", "ja": "Japonca",
    "korean": "Korece", "kor": "Korece", "ko": "Korece",
    "chinese": "Çince", "chs": "Çince", "cht": "Çince", "zh": "Çince",
    "turkish": "Türkçe", "türkçe": "Türkçe", "turkce": "Türkçe", "tur": "Türkçe", "tr": "Türkçe",
    # FromSoftware (Elden Ring, Dark Souls, Sekiro) kendi kodlarini kullanir
    "engus": "İngilizce", "jpnjp": "Japonca", "frafr": "Fransızca", "gerde": "Almanca",
    "itait": "İtalyanca", "polpl": "Lehçe", "rusru": "Rusça", "spaes": "İspanyolca",
    "korkr": "Korece", "zhocn": "Çince", "ptbbr": "Portekizce",
}

# Notun kullaniciya "su dili sec" dedigi kaliplar.
NOTE_LANG_PAT = re.compile(
    r"(türkçe|turkce|ingilizce|fransızca|fransizca|almanca|"
    r"ispanyolca|italyanca|lehçe|lehce|polish|polski|"
    r"rusça|rusca|portekizce|japonca|korece|çince|cince)", re.I)
NOTE_TO_LANG = {
    "türkçe": "Türkçe", "turkce": "Türkçe",
    "ingilizce": "İngilizce",
    "fransızca": "Fransızca", "fransizca": "Fransızca",
    "almanca": "Almanca",
    "ispanyolca": "İspanyolca",
    "italyanca": "İtalyanca",
    "lehçe": "Lehçe", "lehce": "Lehçe", "polish": "Lehçe", "polski": "Lehçe",
    "rusça": "Rusça", "rusca": "Rusça", "portekizce": "Portekizce",
    "japonca": "Japonca", "korece": "Korece", "çince": "Çince", "cince": "Çince",
}
# Notun bir EYLEM istedigini gosteren kaliplar.
#
# Emir/gereklilik kipi SART. "Ingilizce metin dosyasini Turkce ile degistirir"
# yamanin ne yaptigini anlatir; "dili Ingilizce olarak degistirin" kullanicidan
# bir sey ister. Genis bir "degistir" kaliba ikisini ayiramadigi icin Divinity'yi
# yanlislikla celiski saymisti - ekler zorunlu.
ACTION_PAT = re.compile(
    r"(unutmay|seçmey|secmey|seçin|secin|seçiniz|seciniz|seçilmeli|secilmeli|"
    r"değiştirin|degistirin|değiştiriniz|degistiriniz|yapın|yapin|yapınız|yapiniz|"
    r"yapmanız|yapmaniz|açın|acin|açmay|acmay|kapatıp|kapatip|kapatın|kapatin|"
    r"etkinleştir(in|iniz|mey)|etkinlestir(in|iniz|mey)|kopyalayın|kopyalayin|"
    r"ekleyin|kurun|çalıştırın|calistirin|girin|bırakın|birakin|indirin|"
    r"işaretleyin|isaretleyin|gerekir|gereklidir|gerekmekte|gerekiyor)", re.I)

LANG_SEG = re.compile(r"^([a-z]{2})[-_][a-z]{2}$", re.I)   # tr_TR, en-US, pt_BR


def fetch(url, timeout=60):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout).read()


def lang_tokens(path):
    """Bir dosya yolundaki dil isaretlerini dondurur."""
    found = []
    p = str(path).replace("\\", "/")
    segments = [s for s in p.split("/") if s]
    if not segments:
        return found
    name = segments[-1]
    stem, dot, ext = name.rpartition(".")
    if not dot:
        stem, ext = name, ""

    def take(tok):
        t = str(tok).strip().lower()
        if not t:
            return
        m = LANG_SEG.match(t)
        if m:
            t = m.group(1).lower()
        if t in LANGS:
            found.append(LANGS[t])

    for seg in segments[:-1]:          # dizin adlari
        take(seg)
    take(ext)                           # .int, .FR, .EN
    take(stem)                          # English.pak, tr.po
    for part in re.split(r"[_\-. ]+", stem):   # global_pol, ..._Türkçe
        take(part)
    return found


def derive_slot(paths):
    """Dosya listesinden baskin dil slotunu cikarir -> (dil, isabet, toplam).

    Cogunluk ETIKETLI dosyalara degil TUM dosyalara gore olcuklur. The Witcher
    paketinde 4712 dosyanin 901'i "it_amm_001.uti" gibi adlar tasiyor; "it"
    Aurora motorunun "item" onekidir, Italyanca degil. Etiketli dosyalara gore
    bakinca %100 Italyanca cikiyordu; tum dosyalara gore %19 ve reddediliyor.
    Gercek bir dil slotu paketin cogunlugunu kaplar (Thief 200/212, Curse of
    the Dead Gods 7/8, Alan Wake 2 2/2).
    """
    counts = Counter()
    for p in paths:
        langs = set(lang_tokens(p))
        if len(langs) == 1:             # iki dil ayni yolda -> oy yok
            counts[next(iter(langs))] += 1
    total = len(paths)
    if not counts or not total:
        return None, 0, total
    lang, hits = counts.most_common(1)[0]
    if hits * 2 <= total:
        return None, hits, total
    return lang, hits, total


def fold_tr(text):
    """Turkce'ye gore kucult: "İ".lower() Python'da "i" degil "i̇" (i + birlesen
    nokta) verir, bu yuzden re.I ile "İngilizce" deseni hic eslesmiyordu ve
    Thief'in "dili Ingilizce olarak degistirin" notu goruinmez olmustu."""
    return text.replace("İ", "i").replace("I", "i").replace("ı", "i").lower()


def note_language(note):
    """Notun kullaniciya sectirdigi dil(ler), yoksa None."""
    note = fold_tr(note)
    if not ACTION_PAT.search(note):
        return None
    hits = set()
    for m in NOTE_LANG_PAT.finditer(note):
        mapped = NOTE_TO_LANG.get(m.group(1).lower())
        if mapped:
            hits.add(mapped)
    return sorted(hits) if hits else None


def walk_steps(im):
    # Katalogda hem adim hem secenek girdileri duz string olabiliyor
    # (catalog_validate.py da ayni sekilini savunuyor) - sessizce atla.
    if not isinstance(im, dict):
        return
    for s in im.get("steps") or []:
        if isinstance(s, dict):
            yield s
    for o in im.get("options") or []:
        yield from walk_steps(o)


def step_paths(pkg):
    im = pkg.get("installMethod") or {}
    out = []
    for s in walk_steps(im):
        for f in ("src", "dest", "file", "target"):
            if s.get(f):
                out.append(str(s[f]))
    # subDir bilerek disarida: paketin ICINDEKI klasor adi ("Türkçe Yama" gibi
    # bir UI etiketi olabiliyor) ve oyunun dil slotu hakkinda hicbir sey soylemez.
    return out


def deep_paths(app_id, pkg, cache, key):
    from package_pipeline import decompress_to_tar, decrypt_package
    blob = cache / ("%s.makine" % app_id)
    if not blob.exists():
        url = pkg.get("dataUrl") or ("%s/data/%s.makine" % (CDN, app_id))
        blob.write_bytes(fetch(url, timeout=900))
    tar_bytes = decompress_to_tar(decrypt_package(blob.read_bytes(), key))
    with tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r") as tar:
        return sorted(m.name for m in tar.getmembers() if m.isfile())


def main():
    # Modul seviyesinde degil: bu dosya araclardan import ediliyor ve
    # import aninda stdout sarmalamak cagiranin akisini kapatiyor.
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                                  errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--deep", action="store_true",
                    help="paketleri indirip acarak gercek dosya listesine bak")
    ap.add_argument("--max-mb", type=float, default=0,
                    help="--deep ile: bu boyutun ustundeki paketleri atla (0 = sinirsiz)")
    ap.add_argument("--cache", default=os.environ.get("TEMP", "."),
                    help="indirilen .makine dosyalarinin tutulacagi dizin")
    ap.add_argument("--json", help="bulgulari bu dosyaya JSON olarak yaz")
    args = ap.parse_args()

    key = None
    cache = Path(args.cache) / "makine-audit"
    if args.deep:
        sys.path.insert(0, str(Path(__file__).parent))
        from package_pipeline import load_encryption_key
        key = load_encryption_key(Path(__file__).parent / ".encryption_key")
        cache.mkdir(parents=True, exist_ok=True)

    index = json.loads(fetch("%s/assets/index.json" % CDN))
    pk = index["packages"]
    ids = list(pk.keys()) if isinstance(pk, dict) else [
        str(p.get("appId") or p.get("id")) for p in pk]

    def load(a):
        try:
            return a, json.loads(fetch("%s/assets/packages/%s.json" % (CDN, a)))
        except Exception:
            return a, None

    with ThreadPoolExecutor(max_workers=16) as ex:
        pkgs = dict((a, d) for a, d in ex.map(load, ids) if d)

    targets = dict((a, d) for a, d in pkgs.items() if (d.get("installNotes") or "").strip())
    print("%d paket, %d tanesinde installNotes var%s\n"
          % (len(pkgs), len(targets),
             " - derin tarama" if args.deep else " - yalniz kurulum adimlari"))

    def audit(item):
        app_id, pkg = item
        note = (pkg.get("installNotes") or "").strip()
        paths, source = step_paths(pkg), "adim"
        if args.deep:
            size_mb = (pkg.get("compressedSize") or 0) / 1048576
            if args.max_mb and size_mb > args.max_mb:
                source = "atlandi (%.0f MB)" % size_mb
            else:
                try:
                    paths, source = deep_paths(app_id, pkg, cache, key), "icerik"
                except Exception as exc:
                    source = "hata: %s" % exc
        slot, hits, tagged = derive_slot(paths)
        return {"appId": app_id, "name": pkg.get("gameName", ""), "note": note,
                "noteLangs": note_language(note), "slot": slot, "hits": hits,
                "tagged": tagged, "files": len(paths), "source": source}

    with ThreadPoolExecutor(max_workers=4 if args.deep else 16) as ex:
        rows = list(ex.map(audit, sorted(targets.items(), key=lambda kv: int(kv[0]))))

    mismatch, unverifiable, unstated = [], [], []
    for r in rows:
        nl, slot = r["noteLangs"], r["slot"]
        if nl and slot and slot not in nl:
            mismatch.append(r)
        elif nl and not slot and r["source"] == "icerik":
            unverifiable.append(r)
        elif slot and slot != "Türkçe" and not nl:
            # Turkce slota yazan paket zaten dogal davranisi verir; kullaniciyi
            # uyarmak gereken durum cevirinin YABANCI bir dil slotuna girmesidir.
            unstated.append(r)

    def dump(title, rows_, explain):
        print("=" * 78)
        print("%s  (%d)" % (title, len(rows_)))
        print(explain)
        print("=" * 78)
        for r in rows_:
            print("  %-9s %-38s [%s]" % (r["appId"], r["name"][:38], r["source"]))
            print("    not diyor  : %s" % (", ".join(r["noteLangs"]) if r["noteLangs"] else "-"))
            print("    paket yazar: %s (%d/%d dosya)"
                  % (r["slot"] or "-", r["hits"], r["files"]))
            print('    "%s"' % r["note"].replace("\n", " ")[:96])
        print()

    dump("CELISKI - not baska dil soyluyor, paket baska dile yaziyor", mismatch,
         "Kullanici notu uygular, ceviri gorunmez. Far Cry 6 sinifi hata.
"
         "Elle dogrulayin: bir not bir dili baska bir amacla da anabilir
"
         '(A Quiet Place: "once Italyancaya alip geri cevirin" bir gecici
'
         "cozumdur, hedef dil degil).")
    dump("DOGRULANAMAZ - not dil sectiriyor, pakette o dilin izi yok", unverifiable,
         "Not dogru olabilir ama icerik desteklemiyor; elle kontrol gerekir.")
    dump("EKSIK - paket net bir dil slotuna yaziyor, not bunu soylemiyor", unstated,
         "Kullanici hangi dili sececegini bilmiyor.")

    clean = len(rows) - len(mismatch) - len(unverifiable) - len(unstated)
    print("ozet: %d notlu paket - %d celiski, %d dogrulanamaz, %d eksik, %d sorunsuz"
          % (len(rows), len(mismatch), len(unverifiable), len(unstated), clean))

    if args.json:
        Path(args.json).write_text(json.dumps(rows, ensure_ascii=False, indent=1),
                                   encoding="utf-8")
        print("ayrinti: %s" % args.json)
    return 1 if mismatch else 0


if __name__ == "__main__":
    sys.exit(main())
