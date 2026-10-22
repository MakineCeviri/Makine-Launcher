// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Icon — one glyph of the bundled icon font (Lucide, see Icons.qml).
 *
 *   Icon { glyph: Icons.house; size: 18; color: Theme.primary }
 */
Text {
    property string glyph: ""
    property real size: 16

    text: glyph
    textFormat: Text.PlainText
    font.family: Icons.family
    font.pixelSize: size
    color: Theme.textPrimary
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    // The glyph box is the em square: no line gap pushing it off centre.
    lineHeightMode: Text.FixedHeight
    lineHeight: size
    Accessible.ignored: true
}
