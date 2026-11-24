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
 * widthPct (the band's usual widest, of the screen; default 62), marginPx,
 * position region (centred on the picked subtitle region, right over the
 * original line; the default once a region is known) · bottom · top. Text is
 * 28 px at 1080p and scales with the screen.
 *
 * `shown` (not `visible`) is what the owner drives: appearing is 160 ms of
 * opacity and a 6 px rise, disappearing 200 ms; instant with animations off.
 * The window keeps one width while it is up, so a new line only moves the band
 * inside it (no native resize per line).
 *
 * Click-through, never takes focus, and kept out of screen capture
 * (PluginManager.keepOutOfCapture: WDA_EXCLUDEFROMCAPTURE) so a plugin that
 * reads the screen never reads its own subtitles; style `showOnStream` lets
 * capture (OBS, a Discord stream) see it again.
 */
Window {
    id: overlayWindow
    // Click-through unless the player is placing it (overlay.setMovable)
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip | Qt.WindowDoesNotAcceptFocus
           | (movable ? 0 : Qt.WindowTransparentForInput)
    color: "transparent"
    visible: false

    // Driven by the owner (PluginManager.overlayVisible); plain-text callers set it too
    property bool shown: false

    property string translatedText: ""
    property string ocrText: ""
    property rect sourceRegion: Qt.rect(0, 0, 0, 0)
    // [{text, sourceText, kind}] — when set, the subtitle shape is drawn instead
    property var lines: []
    property var overlayStyle: ({})

    // Whether the plugin reads the screen under the region (Live's PC mode). If so and
    // the overlay is visible to capture (exclusion failed, or "Yayında göster"), the band
    // goes just above the region instead of over the line being read.
    property bool regionIsRead: false
    property bool _excluded: true
    readonly property bool _above: regionIsRead && _inRegion && (!_excluded || _showOnStream)

    // overlay.setMovable: drag to place; the new place is kept and reported (overlayMoved)
    property bool movable: false
    property bool _placed: false
    property point _place: Qt.point(0, 0)

    // Off when a plugin drives the overlay and hides it itself
    property bool autoHide: true

    // Configurable from plugin settings
    property color textColor: "#f3f4f6"
    property int textSize: 15
    property color bgColor: "#CC1a1a1a"

    readonly property bool _subtitle: lines && lines.length > 0
    readonly property bool _animate: Dimensions.animFast > 0

    function _style(key, fallback) {
        const s = overlayStyle || {}
        return s[key] !== undefined && s[key] !== null ? s[key] : fallback
    }
    readonly property real _widthPct: _style("widthPct", 62)
    readonly property int _marginPx: _style("marginPx", 24)
    readonly property bool _showOnStream: _style("showOnStream", false)
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
    // A long line widens its band up to this instead of shrinking or adding rows
    readonly property real _stretch: Math.max(_maxBand, _screenRect.width * 0.9)

    // Room for the tallest subtitle the style allows, so the window rarely resizes
    readonly property real _roomHeight: {
        const rows = subtitle.maxLines * 2
        const lineH = subtitle.sizePx * 1.3 * (subtitle.showSource ? 1.6 : 1)
        return Math.max(subtitle.implicitHeight, rows * lineH + 2 * subtitle._padY + 2 * subtitle.gutter)
    }

    width: _subtitle ? Math.round(_stretch) : Math.max(200, textItem.implicitWidth + 28)
    height: _subtitle ? Math.round(_roomHeight) : textItem.implicitHeight + 20

    x: _placed ? _place.x : _subtitle
       ? Math.round((_inRegion ? sourceRegion.x + sourceRegion.width / 2 : _screenRect.x + _screenRect.width / 2) - width / 2)
       : sourceRegion.x + (sourceRegion.width - width) / 2
    y: _placed ? _place.y : !_subtitle ? sourceRegion.y + sourceRegion.height + 6
       : _above ? Math.round(sourceRegion.y - height + subtitle.gutter - 4)
       : _inRegion ? Math.round(sourceRegion.y + sourceRegion.height / 2 - height / 2)
       : _position === "top" ? _screenRect.y + _marginPx - subtitle.gutter
       : _screenRect.y + _screenRect.height - height - _marginPx + subtitle.gutter

    // ── Appear / disappear ──
    onShownChanged: {
        if (shown) {
            hideAnim.stop()
            if (!visible) {
                content.opacity = 0
                rise.y = 6
                visible = true
                showAnim.restart()
            }
        } else if (visible) {
            showAnim.stop()
            hideAnim.restart()
        }
    }
    // Out of screen capture once it has a native window (and again if the style changes)
    function _applyCapture() {
        if (visible && typeof PluginManager !== "undefined" && PluginManager)
            _excluded = PluginManager.keepOutOfCapture(overlayWindow, !_showOnStream) && !_showOnStream
    }
    onVisibleChanged: _applyCapture()
    on_ShowOnStreamChanged: _applyCapture()

    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: content; property: "opacity"; to: 1; duration: overlayWindow._animate ? 160 : 0; easing.type: Easing.OutCubic }
        NumberAnimation { target: rise; property: "y"; to: 0; duration: overlayWindow._animate ? 160 : 0; easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: hideAnim
        NumberAnimation { target: content; property: "opacity"; to: 0; duration: overlayWindow._animate ? 200 : 0; easing.type: Easing.OutCubic }
        ScriptAction { script: overlayWindow.visible = false }
    }

    Item {
        id: content
        anchors.fill: parent
        transform: Translate { id: rise }

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

        // ── Subtitle: centred across, anchored to the side it grows away from ──
        SubtitleBlock {
            id: subtitle
            visible: overlayWindow._subtitle
            x: Math.round((parent.width - width) / 2)
            y: overlayWindow._above ? parent.height - height
             : overlayWindow._inRegion ? Math.round((parent.height - height) / 2)
             : overlayWindow._position === "top" ? 0
             : parent.height - height
            lines: overlayWindow.lines
            style: overlayWindow.overlayStyle
            maxWidth: overlayWindow._maxBand
            stretchWidth: overlayWindow._stretch
            textScale: overlayWindow._screenRect.height > 0 ? overlayWindow._screenRect.height / 1080 : 1
            animate: overlayWindow._animate
        }
    }

    // While movable: drag anywhere on it; once it rests, keep and report the place
    MouseArea {
        anchors.fill: parent
        enabled: overlayWindow.movable
        cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
        onPressed: overlayWindow.startSystemMove()
    }
    Rectangle {
        anchors.fill: parent
        visible: overlayWindow.movable
        color: "transparent"
        radius: 12
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.6)
    }
    onXChanged: if (movable) placeTimer.restart()
    onYChanged: if (movable) placeTimer.restart()
    Timer {
        id: placeTimer
        interval: 300
        onTriggered: {
            overlayWindow._place = Qt.point(overlayWindow.x, overlayWindow.y)
            overlayWindow._placed = true
            if (typeof PluginManager !== "undefined" && PluginManager)
                PluginManager.overlayMoved(overlayWindow.x, overlayWindow.y, overlayWindow.width, overlayWindow.height)
        }
    }

    Timer {
        id: hideTimer
        interval: 15000
        onTriggered: overlayWindow.shown = false
    }

    onTranslatedTextChanged: {
        if (translatedText.length > 0) {
            shown = true
            if (overlayWindow.autoHide) hideTimer.restart()
        }
    }
}
