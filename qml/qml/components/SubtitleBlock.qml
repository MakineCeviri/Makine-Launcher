// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
pragma ComponentBehavior: Bound

/**
 * SubtitleBlock — one translated subtitle as the player sees it over the game.
 *
 * Shared by the in-game overlay (TranslationOverlay) and the preview on the
 * Makine Live page, so what the page shows is what the game gets.
 *
 * One direction, readability first ("ink band", Makine Live's own look,
 * spec/design_tokens.json): an ink band hugging the text, Segoe UI Variable
 * Display Semibold, a black outline and a soft shadow. Text never sits bare
 * on the game: once the band is lighter than 0.5, the outline is at least
 * 2 px and the shadow stays on. Qt's own outline is 1 px, so a thicker one
 * is drawn as eight offset copies behind the line (cheap: static glyphs).
 *
 * `maxLines` caps both the entries shown and how many rows one long line may
 * wrap to before it shrinks (Live sends one entry per subtitle).
 *
 * `style` keys (all optional; docs/plugins/README.md overlay.show.style):
 * preset band · contrast · classic · minimal, font, sizePx, maxLines,
 * textColor, bandColor, bandOpacity, outlinePx, shadow, showSource.
 *
 * Self-contained on purpose (no launcher module import): a preview harness can
 * load it on its own.
 */
Item {
    id: block

    // [{text, sourceText}]; the last `maxLines` are shown
    property var lines: []
    property var style: ({})
    // The widest the band may get (the overlay passes widthPct of its base)
    property real maxWidth: 900

    // Makine Live's signature: classic subtitle yellow, only ever on subtitles
    readonly property color signatureYellow: "#f2c230"

    // ── Style, resolved: the style's own key, then its preset, then "band" ──
    readonly property var _presets: ({
        // Ink band, white text: reads over any scene; the default (GTA VI console)
        band:     { bandOpacity: 0.78, outlinePx: 1, shadow: true, textColor: "#FFFFFF" },
        // Darker band, yellow text, heavier outline: bright, busy scenes
        contrast: { bandOpacity: 0.9,  outlinePx: 2, shadow: true, textColor: "#f2c230" },
        // No band: film subtitle, outline and shadow carry it
        classic:  { bandOpacity: 0.0,  outlinePx: 2, shadow: true, textColor: "#FFFFFF" },
        // No band, outline only
        minimal:  { bandOpacity: 0.0,  outlinePx: 2, shadow: false, textColor: "#FFFFFF" }
    })
    function _pick(key, fallback) {
        const s = block.style || {}
        if (s[key] !== undefined && s[key] !== null) return s[key]
        const preset = _presets[s.preset] || _presets.band
        return preset[key] !== undefined ? preset[key] : fallback
    }
    readonly property int sizePx: Math.round(_pick("sizePx", 28))
    readonly property int maxLines: Math.max(1, Math.round(_pick("maxLines", 2)))
    readonly property color textColor: _pick("textColor", "#FFFFFF")
    readonly property color bandColor: _pick("bandColor", "#0f1012")
    readonly property real bandOpacity: _pick("bandOpacity", 0.78)
    readonly property bool _bare: bandOpacity < 0.5
    readonly property real outlinePx: Math.max(_pick("outlinePx", 1), _bare ? 2 : 0)
    readonly property bool shadow: _pick("shadow", true)
    readonly property bool showSource: _pick("showSource", false)
    // Windows 11 has the Variable face; Windows 10 gets plain Segoe UI
    readonly property string _segoe: Qt.fontFamilies().indexOf("Segoe UI Variable Display") >= 0
                                     ? "Segoe UI Variable Display" : "Segoe UI"
    readonly property string fontFamily: _pick("font", "") !== "" ? _pick("font", "") : _segoe

    readonly property var shownLines: (lines && lines.length > 0)
                                      ? lines.slice(Math.max(0, lines.length - maxLines)) : []

    // Band padding scales a little with the text so a big subtitle keeps its air
    readonly property real _padX: Math.round(22 * Math.max(1, sizePx / 28))
    readonly property real _padY: Math.round(12 * Math.max(1, sizePx / 28))
    // Room around the band for its shadow and the outline (the overlay window is this size)
    readonly property real gutter: 14

    implicitWidth: band.width + 2 * gutter
    implicitHeight: band.height + 2 * gutter

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
        width: Math.max(column.width + 2 * block._padX, block.bandOpacity > 0 ? Math.min(320, block.maxWidth) : 0)
        height: column.height + 2 * block._padY
        radius: 10
        color: Qt.rgba(block.bandColor.r, block.bandColor.g, block.bandColor.b, block.bandOpacity)
    }

    Column {
        id: column
        anchors.centerIn: band
        spacing: Math.round(block.sizePx * 0.12)

        Repeater {
            model: block.shownLines

            Column {
                id: entry
                required property var modelData
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                spacing: 2

                readonly property real _innerMax: Math.max(120, block.maxWidth - 2 * block._padX - 2 * block.gutter)

                // A long line shrinks (to 74 % at most) to stay within maxLines, as Live's
                // own renderer does (overlay_render.py fit_text); past that it wraps on
                TextMetrics {
                    id: measure
                    font.family: block.fontFamily
                    font.pixelSize: block.sizePx
                    font.weight: Font.DemiBold
                    text: entry.modelData.text || ""
                }
                readonly property real _fit: {
                    const steps = [1.0, 0.9, 0.82, 0.74]
                    // 0.92: a wrapped line never fills its width to the last pixel
                    const room = block.maxLines * entry._innerMax * 0.92
                    for (let i = 0; i < steps.length; ++i)
                        if (measure.advanceWidth * steps[i] <= room) return steps[i]
                    return steps[steps.length - 1]
                }

                SubtitleLine {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: entry.modelData.text || ""
                    pixelSize: Math.round(block.sizePx * entry._fit)
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
            text: face.text
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
                text: face.text
                font: face.font
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                color: "#000000"
            }
        }
        Text {
            id: face
            width: Math.min(implicitWidth, lineRoot.innerMax)
            text: lineRoot.text
            font.family: block.fontFamily
            font.pixelSize: lineRoot.pixelSize
            font.weight: lineRoot.weight
            color: lineRoot.color
            style: lineRoot._r > 0 ? Text.Outline : Text.Normal
            styleColor: "#000000"
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
        }
    }
}
