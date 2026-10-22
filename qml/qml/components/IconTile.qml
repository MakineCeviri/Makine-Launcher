// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * IconTile — a white glyph on a coloured rounded square, the way macOS
 * System Settings marks each row and section.
 */
Rectangle {
    id: tile

    property string glyph: ""
    property color tint: Theme.primary
    property real size: 28
    // false: the glyph in the tint on a faint wash of it (status rows)
    property bool solid: true

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size
    radius: Math.round(size * 0.26)
    color: tile.solid ? tile.tint : Theme.withAlpha(tile.tint, 0.16)

    Icon {
        anchors.centerIn: parent
        glyph: tile.glyph
        size: Math.round(tile.size * 0.6)
        color: tile.solid ? "#FFFFFF" : tile.tint
    }
}
