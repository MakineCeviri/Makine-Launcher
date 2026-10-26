#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
Build the UI's bundled fonts from their pinned upstream releases.

  * MakineIcons.ttf — the Lucide icons the launcher uses (ISC), subset from
    lucide-static. Writes qml/qml/theme/Icons.qml: one property per icon, so a
    misspelt icon name is a QML error rather than a blank square.
  * MakineSymbols.ttf — filled counterparts (Phosphor "fill", MIT) of the
    icons in FILLED, at the same code points: a white glyph on a coloured
    tile (IconTile) is filled, as in macOS System Settings. Brand marks
    (BRANDS: Discord, YouTube, GitHub…) are the real logos from Simple Icons
    (CC0), not Phosphor's redraws.
  * InterDisplay-Regular/Medium/SemiBold/Bold.ttf — Inter's display cut,
    the UI's typeface (OFL; tighter, closer to SF Pro than Inter's text cut),
    subset to Latin + Turkish + the punctuation and symbols the UI prints.

Adding an icon: put its Lucide name in ICONS (and, if it can sit on a tile,
its Phosphor name in FILLED), run this, commit the outputs.

Usage:
    python scripts/build_ui_fonts.py
Needs: fonttools (pip install fonttools)
"""

import io
import json
import tarfile
import urllib.request
import zipfile
from pathlib import Path

from fontTools import subset
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import SVGPath
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "qml" / "resources" / "fonts"
ICONS_QML = ROOT / "qml" / "qml" / "theme" / "Icons.qml"

LUCIDE_VERSION = "1.61.0"
LUCIDE_URL = f"https://registry.npmjs.org/lucide-static/-/lucide-static-{LUCIDE_VERSION}.tgz"
PHOSPHOR_VERSION = "2.1.1"
PHOSPHOR_URL = f"https://registry.npmjs.org/@phosphor-icons/core/-/core-{PHOSPHOR_VERSION}.tgz"
SIMPLE_ICONS_VERSION = "16.34.0"
SIMPLE_ICONS_URL = f"https://registry.npmjs.org/simple-icons/-/simple-icons-{SIMPLE_ICONS_VERSION}.tgz"
INTER_URL = "https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip"

ICONS = """
activity app-window archive arrow-down-a-z arrow-down-narrow-wide arrow-down-to-line
arrow-left arrow-right arrow-up-down arrow-up-right badge-check badge-info ban bell
book-open bookmark box bug calendar calendar-clock check chevron-down chevron-left
chevron-right chevron-up chevrons-down circle circle-alert circle-arrow-down circle-arrow-up
circle-check circle-check-big circle-dot circle-help circle-play circle-user circle-x
clock clock-arrow-up cloud-download cog copy cpu crown download ellipsis external-link
eye file-text filter flag folder folder-check folder-input folder-open folder-search
folder-x gamepad-2 gauge git-branch github globe hand-heart hard-drive
hard-drive-download heart heart-handshake history hourglass house image info keyboard
languages laptop layers layout-grid library life-buoy link list list-checks
list-ordered loader-circle lock log-out mail maximize-2 medal message-circle
message-square-text messages-square minimize-2 minus monitor monitor-check
monitor-down moon mouse-pointer-click move-right package package-check palette
panel-left party-popper pause picture-in-picture-2 pin play plus power puzzle
refresh-cw rocket rotate-ccw rotate-cw scan scan-search scroll-text search send server
settings shield shield-alert shield-check sliders-horizontal sparkle sparkles square
square-stop star sun swatch-book tag terminal trash-2 triangle-alert trophy type
undo-2 user users wand-sparkles wifi-off wrench x zap
""".split()

# Lucide name → Phosphor "fill" name: the glyph an IconTile shows for it.
# An icon missing here keeps its Lucide outline on a tile.
FILLED = {
    "activity": "pulse", "app-window": "app-window", "archive": "archive",
    "badge-check": "seal-check", "badge-info": "info", "ban": "prohibit", "bell": "bell",
    "book-open": "book-open", "bookmark": "bookmark-simple", "box": "package", "bug": "bug",
    "calendar": "calendar", "chevrons-down": "tray-arrow-down",
    "circle-alert": "warning-circle", "circle-arrow-down": "arrow-circle-down",
    "circle-arrow-up": "arrow-circle-up", "external-link": "arrow-square-out",
    "circle-check": "check-circle", "circle-check-big": "check-circle",
    "circle-help": "question", "circle-play": "play-circle", "circle-user": "user-circle",
    "circle-x": "x-circle", "clock": "clock", "cloud-download": "cloud-arrow-down",
    "cog": "gear-six", "cpu": "cpu", "crown": "crown", "download": "download-simple",
    "eye": "eye", "file-text": "file-text", "flag": "flag", "folder": "folder",
    "folder-input": "folder-plus", "folder-open": "folder-open", "folder-search": "folder-open",
    "gamepad-2": "game-controller", "gauge": "gauge", "git-branch": "git-branch",
    "github": "github-logo", "globe": "globe", "hand-heart": "hand-heart",
    "hard-drive": "hard-drive", "hard-drive-download": "hard-drives", "heart": "heart",
    "heart-handshake": "handshake", "history": "clock-counter-clockwise",
    "hourglass": "hourglass", "house": "house", "image": "image", "info": "info",
    "keyboard": "keyboard", "languages": "translate", "laptop": "laptop", "layers": "stack",
    "layout-grid": "squares-four", "library": "books", "life-buoy": "lifebuoy", "link": "link",
    "list-checks": "list-checks", "list-ordered": "list-numbers", "lock": "lock",
    "mail": "envelope", "medal": "medal", "message-circle": "chat-circle",
    "message-square-text": "chat-text", "messages-square": "chats", "monitor": "monitor",
    "moon": "moon", "mouse-pointer-click": "cursor-click", "package": "package",
    "palette": "palette", "party-popper": "confetti", "pin": "push-pin", "play": "play",
    "power": "power", "puzzle": "puzzle-piece", "refresh-cw": "arrows-clockwise",
    "rocket": "rocket", "rotate-ccw": "arrow-counter-clockwise", "rotate-cw": "arrow-clockwise",
    "scan-search": "magnifying-glass", "scroll-text": "scroll", "search": "magnifying-glass",
    "send": "paper-plane-tilt", "server": "hard-drives", "settings": "gear",
    "shield": "shield", "shield-alert": "shield-warning", "shield-check": "shield-check",
    "sliders-horizontal": "sliders-horizontal", "sparkle": "sparkle", "sparkles": "sparkle",
    "star": "star", "sun": "sun", "swatch-book": "swatches", "tag": "tag",
    "terminal": "terminal-window", "trash-2": "trash", "triangle-alert": "warning",
    "trophy": "trophy", "type": "text-t", "undo-2": "arrow-u-up-left", "user": "user",
    "users": "users", "wand-sparkles": "magic-wand", "wifi-off": "wifi-slash",
    "wrench": "wrench", "zap": "lightning",
}

# Marks Lucide does not draw (brands): at private-use code points, in
# MakineSymbols.ttf only. Icon picks that font for them.
EXTRA = {
    "discord": ("discord-logo", 0xF101),
    "youtube": ("youtube-logo", 0xF102),
    "instagram": ("instagram-logo", 0xF103),
    "linkedin": ("linkedin-logo", 0xF104),
    "x-logo": ("x-logo", 0xF105),
}

# Brands drawn from Simple Icons, the logos as their owners publish them:
# icon name (EXTRA or FILLED) → Simple Icons slug. LinkedIn left Simple
# Icons at its owner's request; its mark stays Phosphor's ("in" in a square).
BRANDS = {
    "discord": "discord", "youtube": "youtube", "instagram": "instagram",
    "x-logo": "x", "github": "github",
}

# Latin, Latin-1, Latin Extended-A (Turkish ğ ı ş İ), general punctuation,
# currency (₺ €), arrows and the few symbols the UI prints.
DISPLAY_UNICODES = "U+0020-007E,U+00A0-017F,U+2000-206F,U+20A0-20CF,U+2100-214F,U+2190-21FF,U+2212,U+2713,U+2715"


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=120) as r:
        return r.read()


# JavaScript reserved words cannot name a QML property: "package" → packageIcon
RESERVED = {"package", "import", "function", "default", "delete", "new", "class",
            "enum", "export", "interface", "private", "public", "static", "switch"}


def camel(name: str) -> str:
    head, *rest = name.split("-")
    out = head + "".join(p[:1].upper() + p[1:] for p in rest)
    if out in RESERVED:
        out += "Icon"
    return out if not out[0].isdigit() else "i" + out


def set_icon_metrics(font: TTFont) -> None:
    """The em square is the line: no gap above or below, so a glyph box
    of `size` pixels holds the icon exactly, centred."""
    font["hhea"].ascent, font["hhea"].descent, font["hhea"].lineGap = 1000, 0, 0
    os2 = font["OS/2"]
    os2.sTypoAscender, os2.sTypoDescender, os2.sTypoLineGap = 1000, 0, 0
    os2.usWinAscent, os2.usWinDescent = 1000, 0
    os2.version = max(os2.version, 4)
    os2.fsSelection |= 1 << 7  # USE_TYPO_METRICS (OS/2 version 4 and up)


def build_icons() -> dict:
    tar = tarfile.open(fileobj=io.BytesIO(fetch(LUCIDE_URL)), mode="r:gz")
    codepoints = json.load(tar.extractfile("package/font/codepoints.json"))
    ttf = tar.extractfile("package/font/lucide.ttf").read()

    missing = [n for n in ICONS if n not in codepoints]
    missing += [n for n in FILLED if n not in ICONS]
    if missing:
        raise SystemExit(f"not in lucide-static {LUCIDE_VERSION} or ICONS: {', '.join(missing)}")

    font = TTFont(io.BytesIO(ttf))
    opts = subset.Options()
    opts.layout_features = []
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=[codepoints[n] for n in ICONS])
    sub.subset(font)

    # lucide.ttf records every glyph's left edge (bbox xMin and side bearing)
    # as 0 while the outline keeps its place on the 24-unit grid. Saving
    # recomputes the bbox from the outline but leaves the bearing at 0, and
    # renderers then draw each icon shifted left — a chevron by a third of
    # its width. The bearing has to be the outline's real left edge.
    glyf, hmtx = font["glyf"], font["hmtx"]
    for name in font.getGlyphOrder():
        g = glyf[name]
        if g.numberOfContours > 0:
            g.recalcBounds(glyf)
            hmtx[name] = (hmtx[name][0], g.xMin)
    set_icon_metrics(font)

    out = FONTS / "MakineIcons.ttf"
    font.save(out)

    filled = "".join(f"\\u{codepoints[n]:04X}" for n in sorted(FILLED))
    extra = "".join(f"\\u{cp:04X}" for _, cp in EXTRA.values())
    filled += extra
    lines = [
        "// SPDX-License-Identifier: AGPL-3.0-only",
        "// Copyright (c) 2026 Makine Çeviri",
        "// Generated by scripts/build_ui_fonts.py from lucide-static "
        f"{LUCIDE_VERSION} (ISC) and @phosphor-icons/core {PHOSPHOR_VERSION} (MIT) — do not edit.",
        "",
        "pragma Singleton",
        "import QtQuick",
        "",
        "QtObject {",
        '    readonly property string family: "lucide"',
        "    // Filled glyphs at the same code points, for tiles (MakineSymbols.ttf)",
        '    readonly property string symbolsFamily: "Makine Symbols"',
        f'    readonly property string filled: "{filled}"',
        "    // Only in the symbols font (brands): Icon draws them from there",
        f'    readonly property string symbolsOnly: "{extra}"',
        "",
    ]
    for n in sorted(ICONS):
        lines.append(f'    readonly property string {camel(n)}: "\\u{codepoints[n]:04X}"')
    for n, (_, cp) in sorted(EXTRA.items()):
        lines.append(f'    readonly property string {camel(n)}: "\\u{cp:04X}"')
    lines.append("}")
    ICONS_QML.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"{out.name}: {len(ICONS)} icons, {out.stat().st_size // 1024} KB")
    return codepoints


def build_symbols(codepoints: dict) -> None:
    tar = tarfile.open(fileobj=io.BytesIO(fetch(PHOSPHOR_URL)), mode="r:gz")
    brands = tarfile.open(fileobj=io.BytesIO(fetch(SIMPLE_ICONS_URL)), mode="r:gz")
    # Phosphor draws on a 256-unit square, y down; the em is 1000 units, y up
    flip = (1000 / 256, 0, 0, -1000 / 256, 0, 1000)
    # Simple Icons fill a 24-unit square edge to edge: drawn at 82 % of the
    # em, centred, they stand as large as Phosphor's padded glyphs
    k = 1000 * 0.82 / 24
    pad = (1000 - 24 * k) / 2
    brand_flip = (k, 0, 0, -k, pad, 1000 - pad)
    order, cmap, glyphs = [".notdef"], {}, {".notdef": TTGlyphPen(None).glyph()}
    glyph_sources = [(l, ph, codepoints[l]) for l, ph in sorted(FILLED.items())]
    glyph_sources += [(n, ph, cp) for n, (ph, cp) in sorted(EXTRA.items())]
    glyph_sources = [(camel(n), n, ph, cp) for n, ph, cp in glyph_sources]
    for name, icon, phosphor, cp in glyph_sources:
        if icon in BRANDS:
            member = f"package/icons/{BRANDS[icon]}.svg"
            source, transform = brands, brand_flip
        else:
            member = f"package/assets/fill/{phosphor}-fill.svg"
            source, transform = tar, flip
        try:
            svg = source.extractfile(member).read()
        except KeyError:
            raise SystemExit(f"not in its package: {member}")
        pen = TTGlyphPen(None)
        SVGPath.fromstring(svg, transform=transform).draw(Cu2QuPen(pen, 1.0, reverse_direction=True))
        order.append(name)
        glyphs[name] = pen.glyph()
        cmap[cp] = name

    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    glyf = fb.font["glyf"]
    fb.setupHorizontalMetrics({n: (1000, getattr(glyf[n], "xMin", 0)) for n in order})
    fb.setupHorizontalHeader(ascent=1000, descent=0)
    fb.setupNameTable({"familyName": "Makine Symbols", "styleName": "Regular"})
    fb.setupOS2()
    fb.setupPost()
    set_icon_metrics(fb.font)
    out = FONTS / "MakineSymbols.ttf"
    fb.save(out)
    print(f"{out.name}: {len(glyph_sources)} filled icons, {out.stat().st_size // 1024} KB")


def build_display() -> None:
    z = zipfile.ZipFile(io.BytesIO(fetch(INTER_URL)))
    for weight in ("Regular", "Medium", "SemiBold", "Bold"):
        font = TTFont(io.BytesIO(z.read(f"extras/ttf/InterDisplay-{weight}.ttf")))
        opts = subset.Options()
        opts.layout_features = ["*"]
        opts.name_IDs = ["*"]
        sub = subset.Subsetter(opts)
        sub.populate(unicodes=subset.parse_unicodes(DISPLAY_UNICODES))
        sub.subset(font)
        out = FONTS / f"InterDisplay-{weight}.ttf"
        font.save(out)
        print(f"{out.name}: {out.stat().st_size // 1024} KB")


if __name__ == "__main__":
    build_symbols(build_icons())
    build_display()
