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
 * call), or a subtitle of several `lines` drawn by SubtitleBlock with the
 * plugin's `overlayStyle` (docs/plugins/README.md). Placement keys live here:
 * widthPct (the band's widest, of the region or screen), marginPx, position
 * region (centred on the picked subtitle region, over the original line) ·
 * bottom · top (of the screen under the region).
 *
 * Click-through, never takes focus, and kept out of screen capture
 * (PluginManager.keepOutOfCapture: WDA_EXCLUDEFROMCAPTURE) so a plugin that
 * reads the screen never reads its own subtitles.
 */
Window {
    id: overlayWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip
           | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
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

    function _style(key, fallback) {
        const s = overlayStyle || {}
        return s[key] !== undefined && s[key] !== null ? s[key] : fallback
    }
    readonly property real _widthPct: _style("widthPct", 80)
    readonly property int _marginPx: _style("marginPx", 24)
    readonly property bool _hasRegion: sourceRegion.width > 0 && sourceRegion.height > 0
    readonly property string _position: _style("position", _hasRegion ? "region" : "bottom")

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
    readonly property rect _screenRect: Qt.rect(_screen ? _screen.virtualX : 0, _screen ? _screen.virtualY : 0,
                                                _screen ? _screen.width : 1280, _screen ? _screen.height : 720)
    readonly property bool _inRegion: _position === "region" && _hasRegion
    // Over a region the band may be as wide as the region (a console window's subtitle
    // area); a very narrow region still gets room for a readable line
    readonly property real _maxBand: _inRegion
        ? Math.max(sourceRegion.width, Math.min(560, _screenRect.width * 0.5))
        : _screenRect.width * _widthPct / 100

    width: _subtitle ? subtitle.implicitWidth : Math.max(200, textItem.implicitWidth + 28)
    height: _subtitle ? subtitle.implicitHeight : textItem.implicitHeight + 20

    x: _subtitle
       ? Math.round((_inRegion ? sourceRegion.x + sourceRegion.width / 2 : _screenRect.x + _screenRect.width / 2) - width / 2)
       : sourceRegion.x + (sourceRegion.width - width) / 2
    y: !_subtitle ? sourceRegion.y + sourceRegion.height + 6
       : _inRegion ? Math.round(sourceRegion.y + sourceRegion.height / 2 - height / 2)
       : _position === "top" ? _screenRect.y + _marginPx - subtitle.gutter
       : _screenRect.y + _screenRect.height - height - _marginPx + subtitle.gutter

    // Out of screen capture once it has a native window; a short fade on each appearance
    onVisibleChanged: {
        if (!visible) return
        if (typeof PluginManager !== "undefined" && PluginManager) PluginManager.keepOutOfCapture(overlayWindow)
        content.opacity = 0
        fadeIn.restart()
    }

    Item {
        id: content
        anchors.fill: parent
        NumberAnimation on opacity { id: fadeIn; running: false; to: 1; duration: 160; easing.type: Easing.OutCubic }

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
        SubtitleBlock {
            id: subtitle
            visible: overlayWindow._subtitle
            lines: overlayWindow.lines
            style: overlayWindow.overlayStyle
            maxWidth: overlayWindow._maxBand
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
