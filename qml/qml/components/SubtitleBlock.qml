// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
pragma ComponentBehavior: Bound

/**
 * SubtitleBlock — one translated subtitle as the player sees it over the game.
 *
 * Shared by the in-game overlay (TranslationOverlay) and the Makine Live page
 * ("Şu an" and the appearance preview), so the page shows what the game gets.
 *
 * One direction, readability first (Fable, 2026-11-24): an ink band hugging the
 * text, Inter Display SemiBold, a black outline and a soft shadow. Text never
 * sits bare on the game: below band 0.5 the outline is at least 2 px (Qt's own
 * outline is 1 px; thicker is eight offset copies behind the line). A machine
 * translation not yet approved carries a thin yellow strip at the band's left;
 * an approved line none. Text never shrinks: a line that would need more than
 * `maxLines` rows widens its band (up to `stretchWidth`) instead.
 *
 * Motion (`animate`): the band's size follows in 120 ms; a new line crossfades
 * over the old one in 120 ms. Appearing and disappearing are the window's.
 *
 * `style` keys (all optional; docs/plugins/README.md overlay.show.style):
 * preset band · contrast · classic · minimal, font, sizePx, maxLines,
 * textColor, bandColor, bandOpacity, outlinePx, shadow, showSource.
 * `lines`: [{text, sourceText?, kind?, label?}]; kind "verified" = approved; label is the
 * speaker (one entry per speaker), untranslated, inline before the text in the quieter
 * colour ("Jason: Ne oldu?"), as in the game's own subtitles.
 *
 * Self-contained on purpose (no launcher module import): a preview harness can
 * load it on its own.
 */
Item {
    id: block

    property var lines: []
    property var style: ({})
    // The band's usual widest (the overlay passes widthPct of its base) …
    property real maxWidth: 900
    // … and the widest it may grow to keep a long line within maxLines
    property real stretchWidth: maxWidth
    // Text size multiplier (the overlay scales 28 px @1080p to the screen)
    property real textScale: 1
    property bool animate: true

    // Makine Live's signature: classic subtitle yellow, only ever on subtitles
    readonly property color signatureYellow: "#f2c230"

    // ── Style, resolved: the style's own key, then its preset, then "band" ──
    readonly property var _presets: ({
        // Ink band, white text: reads over any scene; the default (GTA VI console)
        band:     { bandOpacity: 0.9, outlinePx: 1, shadow: true, textColor: "#FFFFFF" },
        // Darker band, yellow text, heavier outline: bright, busy scenes
        contrast: { bandOpacity: 0.9, outlinePx: 2, shadow: true, textColor: "#f2c230" },
        // No band: film subtitle, outline and shadow carry it
        classic:  { bandOpacity: 0.0, outlinePx: 2, shadow: true, textColor: "#FFFFFF" },
        // No band, outline only
        minimal:  { bandOpacity: 0.0, outlinePx: 2, shadow: false, textColor: "#FFFFFF" }
    })
    function _pick(key, fallback) {
        const s = block.style || {}
        if (s[key] !== undefined && s[key] !== null) return s[key]
        const preset = _presets[s.preset] || _presets.band
        return preset[key] !== undefined ? preset[key] : fallback
    }
    readonly property int sizePx: Math.round(_pick("sizePx", 28) * textScale)
    readonly property int maxLines: Math.max(1, Math.round(_pick("maxLines", 2)))
    readonly property color textColor: _pick("textColor", "#FFFFFF")
    readonly property color bandColor: _pick("bandColor", "#0f1012")
    readonly property real bandOpacity: _pick("bandOpacity", 0.9)
    readonly property bool _bare: bandOpacity < 0.5
    readonly property real outlinePx: Math.max(_pick("outlinePx", 1), _bare ? 2 : 0)
    readonly property bool shadow: _pick("shadow", true)
    readonly property bool showSource: _pick("showSource", false)
    readonly property string fontFamily: _pick("font", "") !== "" ? _pick("font", "") : "Inter Display"

    readonly property var shownLines: (lines && lines.length > 0)
                                      ? lines.slice(Math.max(0, lines.length - maxLines)) : []
    // The newest line decides the strip: a machine translation awaiting approval
    readonly property bool _pending: {
        const l = shownLines.length > 0 ? shownLines[shownLines.length - 1] : null
        return !!l && !!l.kind && l.kind !== "verified"
    }

    // One wrap width for every entry: the usual widest, or wider when a line would
    // otherwise need more than maxLines rows (0.92: a wrapped row is never full)
    FontMetrics {
        id: metrics
        font.family: block.fontFamily
        font.pixelSize: block.sizePx
        font.weight: Font.DemiBold
    }
    readonly property real _usualInner: Math.max(120, maxWidth - 2 * _padX - 2 * gutter)
    readonly property real _widestInner: Math.max(_usualInner, stretchWidth - 2 * _padX - 2 * gutter)
    readonly property real _innerMax: {
        let need = 0
        for (let i = 0; i < shownLines.length; ++i)
            need = Math.max(need, metrics.advanceWidth((shownLines[i].label ? shownLines[i].label + ": " : "") + (shownLines[i].text || "")) / (maxLines * 0.92))
        return Math.min(_widestInner, Math.max(_usualInner, need))
    }

    // Band padding scales with the text so a big subtitle keeps its air
    readonly property real _padX: Math.round(22 * Math.max(1, sizePx / 28))
    readonly property real _padY: Math.round(12 * Math.max(1, sizePx / 28))
    // Room around the band for its shadow and the outline
    readonly property real gutter: 14
    readonly property int _anim: animate ? 120 : 0

    implicitWidth: band.width + 2 * gutter
    implicitHeight: band.height + 2 * gutter

    // The previous lines, fading out under the new ones (crossfade)
    property var _ghostLines: []
    property var _lastLines: []
    onShownLinesChanged: {
        if (_anim > 0 && _lastLines.length > 0 && JSON.stringify(_lastLines) !== JSON.stringify(shownLines)) {
            _ghostLines = _lastLines
            crossfade.restart()
        }
        _lastLines = shownLines
    }
    ParallelAnimation {
        id: crossfade
        NumberAnimation { target: current; property: "opacity"; from: 0; to: 1; duration: block._anim; easing.type: Easing.OutCubic }
        NumberAnimation { target: ghost; property: "opacity"; from: 1; to: 0; duration: block._anim; easing.type: Easing.OutCubic }
        onFinished: block._ghostLines = []
    }

    // Soft shadow under the band (Live: blur 18, 0.35, a little below)
    RectangularShadow {
        anchors.fill: band
        visible: block.bandOpacity > 0 && block.shadow
        radius: band.radius
        offset.y: 4
        blur: 18
        spread: 0
        color: Qt.rgba(0, 0, 0, 0.35 * Math.min(1, block.bandOpacity / 0.78))
    }

    Rectangle {
        id: band
        x: block.gutter
        y: block.gutter
        width: Math.max(current.width + 2 * block._padX, block.bandOpacity > 0 ? Math.min(320, block.maxWidth) : 0)
        height: current.height + 2 * block._padY
        radius: 10
        color: Qt.rgba(block.bandColor.r, block.bandColor.g, block.bandColor.b, block.bandOpacity)
        Behavior on width { NumberAnimation { duration: block._anim; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: block._anim; easing.type: Easing.OutCubic } }

        // Awaiting approval: a thin yellow strip at the left (Live's status strip)
        Rectangle {
            visible: block._pending && block.bandOpacity > 0
            x: Math.round(block._padX / 2) - 1
            y: block._padY
            width: 3
            height: parent.height - 2 * block._padY
            radius: 1.5
            color: block.signatureYellow
        }
    }

    Lines { id: ghost; model: block._ghostLines; opacity: 0 }
    Lines { id: current; model: block.shownLines }

    // A stack of subtitle entries, centred on the band
    component Lines: Column {
        id: stack
        property var model: []
        anchors.centerIn: band
        spacing: Math.round(block.sizePx * 0.12)

        Repeater {
            model: stack.model

            Column {
                id: entry
                required property var modelData
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                spacing: 2

                readonly property real _innerMax: block._innerMax

                SubtitleLine {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: entry.modelData.text || ""
                    label: entry.modelData.label || ""
                    pixelSize: block.sizePx
                    weight: Font.DemiBold
                    color: block.textColor
                    innerMax: entry._innerMax
                }
                SubtitleLine {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: block.showSource && (entry.modelData.sourceText || "") !== ""
                    text: entry.modelData.sourceText || ""
                    pixelSize: Math.round(block.sizePx * 0.58)
                    weight: Font.Medium
                    color: "#c9c8c2"
                    innerMax: entry._innerMax
                    outline: Math.min(block.outlinePx, 1)
                }
            }
        }
    }

    // A line of text with a real outline of any width and a drop shadow
    component SubtitleLine: Item {
        id: lineRoot
        property string text: ""
        property int pixelSize: 28
        property int weight: Font.DemiBold
        property color color: "#FFFFFF"
        property real innerMax: 800
        property real outline: block.outlinePx
        // Speaker, inline before the text in the quieter colour (same weight: the outline
        // copies are plain text and must lay out exactly like the face)
        property string label: ""
        readonly property string _prefix: label === "" ? "" : (label.endsWith(":") ? label : label + ":") + " "
        readonly property string _plain: _prefix + text
        function _esc(t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }

        width: face.width
        height: face.height

        readonly property real _r: Math.round(lineRoot.outline)
        readonly property var _ring: _r > 1
            ? [[-1, 0], [1, 0], [0, -1], [0, 1], [-0.71, -0.71], [0.71, -0.71], [-0.71, 0.71], [0.71, 0.71]]
            : []

        // Drop shadow: the line once more, dark, a little below
        Text {
            visible: block.shadow
            x: 0
            y: Math.max(1, Math.round(lineRoot.pixelSize / 14))
            width: face.width
            text: lineRoot._plain
            font: face.font
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            color: "#000000"
            opacity: 0.55
            style: lineRoot._r > 0 ? Text.Outline : Text.Normal
            styleColor: "#000000"
        }
        // Thick outline: offset copies in a ring
        Repeater {
            model: lineRoot._ring
            Text {
                required property var modelData
                x: modelData[0] * lineRoot._r
                y: modelData[1] * lineRoot._r
                width: face.width
                text: lineRoot._plain
                font: face.font
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                color: "#000000"
            }
        }
        // The unwrapped width: a wrapped Text's own implicitWidth follows its width
        TextMetrics {
            id: natural
            font: face.font
            text: lineRoot._plain
        }
        Text {
            id: face
            width: Math.min(Math.ceil(natural.advanceWidth) + 2, lineRoot.innerMax)
            text: lineRoot.label === "" ? lineRoot.text
                : "<font color=\"#c9c8c2\">" + lineRoot._esc(lineRoot._prefix) + "</font>" + lineRoot._esc(lineRoot.text)
            font.family: block.fontFamily
            font.pixelSize: lineRoot.pixelSize
            font.weight: lineRoot.weight
            color: lineRoot.color
            style: lineRoot._r > 0 ? Text.Outline : Text.Normal
            styleColor: "#000000"
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            textFormat: lineRoot.label === "" ? Text.PlainText : Text.StyledText
        }
    }
}
