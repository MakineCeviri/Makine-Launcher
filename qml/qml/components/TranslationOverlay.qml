// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Frameless overlay a plugin draws over the game (overlay.show).
 *
 * Two shapes: one translated text under the picked region (the plain `text`
 * call), or a subtitle of several `lines` styled by `overlayStyle`
 * (docs/plugins/README.md: preset classic · contrast · band · minimal, font,
 * sizePx, maxLines, textColor, bandColor, bandOpacity, outlinePx, shadow,
 * widthPct, marginPx, position bottom · top · region, showSource). Every
 * style key is optional; the defaults read as a normal film subtitle.
 */
Window {
    id: overlayWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip | Qt.WindowTransparentForInput
    color: "transparent"
    visible: false

    property string translatedText: ""
    property string ocrText: ""
    property rect sourceRegion: Qt.rect(0, 0, 0, 0)
    // [{text, sourceText}] — when set, the subtitle shape is drawn instead
    property var lines: []
    property var overlayStyle: ({})

    // Off when a plugin drives the overlay and hides it itself
    property bool autoHide: true

    // Configurable from plugin settings
    property color textColor: "#f3f4f6"
    property int textSize: 15
    property color bgColor: "#CC1a1a1a"

    readonly property bool _subtitle: lines && lines.length > 0

    // ── Subtitle style, resolved: preset first, then the plugin's own keys ──
    readonly property var _presets: ({
        classic:  { bandOpacity: 0.0,  outlinePx: 2, shadow: true },
        contrast: { bandOpacity: 1.0,  outlinePx: 0, shadow: false },
        band:     { bandOpacity: 0.6,  outlinePx: 0, shadow: false },
        minimal:  { bandOpacity: 0.0,  outlinePx: 0, shadow: true }
    })
    function _style(key, fallback) {
        const s = overlayStyle || {}
        if (s[key] !== undefined) return s[key]
        const preset = _presets[s.preset || "classic"] || _presets.classic
        return preset[key] !== undefined ? preset[key] : fallback
    }
    readonly property int _sizePx: _style("sizePx", 28)
    readonly property int _maxLines: _style("maxLines", 3)
    readonly property color _textColor: _style("textColor", "#FFFFFF")
    readonly property color _bandColor: _style("bandColor", "#000000")
    readonly property real _bandOpacity: _style("bandOpacity", 0)
    readonly property int _outlinePx: _style("outlinePx", 2)
    readonly property bool _shadow: _style("shadow", true)
    readonly property real _widthPct: _style("widthPct", 80)
    readonly property int _marginPx: _style("marginPx", 24)
    readonly property bool _showSource: _style("showSource", false)
    readonly property string _font: _style("font", "")
    readonly property bool _hasRegion: sourceRegion.width > 0 && sourceRegion.height > 0
    readonly property string _position: _style("position", _hasRegion ? "region" : "bottom")
    readonly property var _shownLines: _subtitle ? lines.slice(Math.max(0, lines.length - _maxLines)) : []

    // The screen under the region (or the first one): what top/bottom refer to
    readonly property var _screen: {
        const screens = Qt.application.screens
        const cx = sourceRegion.x + sourceRegion.width / 2
        const cy = sourceRegion.y + sourceRegion.height / 2
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i]
            if (cx >= s.virtualX && cx < s.virtualX + s.width && cy >= s.virtualY && cy < s.virtualY + s.height)
                return s
        }
        return screens[0]
    }
    readonly property rect _base: _position === "region" && _hasRegion
        ? sourceRegion
        : Qt.rect(_screen ? _screen.virtualX : 0, _screen ? _screen.virtualY : 0,
                  _screen ? _screen.width : 1280, _screen ? _screen.height : 720)

    width: _subtitle ? Math.max(200, _base.width * _widthPct / 100) : Math.max(200, textItem.implicitWidth + 28)
    height: _subtitle ? subtitleColumn.implicitHeight + 16 : textItem.implicitHeight + 20

    x: _subtitle ? _base.x + (_base.width - width) / 2
                 : sourceRegion.x + (sourceRegion.width - width) / 2
    y: !_subtitle ? sourceRegion.y + sourceRegion.height + 6
       : _position === "top" ? _base.y + _marginPx
       : _base.y + _base.height - height - _marginPx

    // ── Single text under the region ──
    Rectangle {
        anchors.fill: parent
        visible: !overlayWindow._subtitle
        radius: 6
        color: overlayWindow.bgColor

        Text {
            id: textItem
            anchors.centerIn: parent
            width: Math.min(implicitWidth, 480)
            text: overlayWindow.translatedText
            font.pixelSize: overlayWindow.textSize
            font.weight: Font.Medium
            color: overlayWindow.textColor
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    // ── Subtitle ──
    Rectangle {
        anchors.fill: parent
        visible: overlayWindow._subtitle && overlayWindow._bandOpacity > 0
        radius: 4
        color: overlayWindow._bandColor
        opacity: overlayWindow._bandOpacity
    }

    Column {
        id: subtitleColumn
        visible: overlayWindow._subtitle
        anchors.centerIn: parent
        width: parent.width - 24
        spacing: 2

        Repeater {
            model: overlayWindow._shownLines

            Column {
                id: lineItem
                required property var modelData
                width: subtitleColumn.width
                spacing: 0

                Item {
                    width: parent.width
                    height: lineText.implicitHeight

                    // Drop shadow: the same line, offset and dark, behind it
                    Text {
                        visible: overlayWindow._shadow
                        x: 2; y: 2
                        width: parent.width
                        text: lineText.text
                        font: lineText.font
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        color: "#000000"
                        opacity: 0.6
                    }
                    Text {
                        id: lineText
                        width: parent.width
                        text: lineItem.modelData.text || ""
                        font.pixelSize: overlayWindow._sizePx
                        font.weight: Font.DemiBold
                        font.family: overlayWindow._font !== "" ? overlayWindow._font : Dimensions.fontText
                        color: overlayWindow._textColor
                        style: overlayWindow._outlinePx > 0 ? Text.Outline : Text.Normal
                        styleColor: "#000000"
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                    }
                }
                Text {
                    visible: overlayWindow._showSource && (lineItem.modelData.sourceText || "") !== ""
                    width: parent.width
                    text: lineItem.modelData.sourceText || ""
                    font.pixelSize: Math.round(overlayWindow._sizePx * 0.6)
                    font.family: lineText.font.family
                    color: overlayWindow._textColor
                    opacity: 0.75
                    style: lineText.style
                    styleColor: "#000000"
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                }
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 15000
        onTriggered: overlayWindow.visible = false
    }

    onTranslatedTextChanged: {
        if (translatedText.length > 0) {
            visible = true
            if (overlayWindow.autoHide) hideTimer.restart()
        }
    }
}
