// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * IconTile — a white glyph on a coloured rounded square, the way macOS
 * System Settings marks each row and section: the tile lit from above (a
 * soft top-to-bottom gradient, a faint rim) and the glyph filled, from
 * MakineSymbols.ttf when the icon has a filled form (Icons.filled).
 * play() gives the tile a small bump and its glyph its motion (MotionIcon);
 * rows call it when the pointer reaches them.
 */
Rectangle {
    id: tile

    property string glyph: ""
    property color tint: Theme.primary
    property real size: 28
    // false: the glyph in the tint on a faint wash of it (status rows)
    property bool solid: true

    readonly property bool _filled: tile.solid && tile.glyph !== "" && Icons.filled.indexOf(tile.glyph) >= 0
    // The glyph's box shares the tile's parity, so centring lands on whole
    // pixels: a half-pixel offset is snapped left and up, and the glyph
    // looks pushed toward the top left corner.
    readonly property int _glyphSize: {
        const want = Math.round(tile.size * (tile._filled ? 0.68 : 0.6))
        return (Math.round(tile.size) - want) % 2 === 0 ? want : want + 1
    }

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size
    radius: Math.round(size * 0.24)
    color: tile.solid ? tile.tint : Theme.withAlpha(tile.tint, 0.16)
    gradient: tile.solid ? lit : null
    border.width: tile.solid ? 1 : 0
    border.color: Qt.rgba(1, 1, 1, 0.14)

    Gradient {
        id: lit
        GradientStop { position: 0.0; color: Qt.tint(tile.tint, Qt.rgba(1, 1, 1, 0.16)) }
        GradientStop { position: 1.0; color: Qt.darker(tile.tint, 1.08) }
    }

    function play() {
        glyphIcon.play()
        if (Dimensions.animSlow > 0 && !bump.running) bump.restart()
    }
    SequentialAnimation {
        id: bump
        NumberAnimation { target: tile; property: "scale"; to: 1.08; duration: 120; easing.type: Easing.OutQuad }
        NumberAnimation { target: tile; property: "scale"; to: 1; duration: 340; easing.type: Easing.OutBack }
    }

    MotionIcon {
        id: glyphIcon
        x: Math.round((tile.width - width) / 2)
        y: Math.round((tile.height - height) / 2)
        glyph: tile.glyph
        font.family: tile._filled ? Icons.symbolsFamily : Icons.family
        size: tile._glyphSize
        color: tile.solid ? "#FFFFFF" : tile.tint
    }
}
