// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * WindowControls — the frameless window's own buttons, top right: hide to
 * the tray, minimise, close. Thin glyphs that only take a fill under the
 * pointer, so they sit on any page (art included) without a title bar.
 */
Row {
    id: controls

    // Over cover art the glyphs turn white and take a dark wash on hover
    property bool onArt: false
    // The tray needs the tray icon, which exists only after the welcome flow
    property bool showTray: true

    signal trayClicked()
    signal minimizeClicked()
    signal closeClicked()

    spacing: 2

    WindowButton {
        visible: controls.showTray
        glyph: Icons.chevronsDown
        tip: qsTr("Tepsiye gizle")
        onClicked: controls.trayClicked()
    }
    WindowButton {
        glyph: Icons.minus
        tip: qsTr("Küçült")
        onClicked: controls.minimizeClicked()
    }
    WindowButton {
        glyph: Icons.x
        tip: qsTr("Kapat")
        isClose: true
        onClicked: controls.closeClicked()
    }

    component WindowButton: Item {
        id: wb
        property string glyph: ""
        property string tip: ""
        property bool isClose: false
        signal clicked()

        width: 40
        height: 32

        Accessible.role: Accessible.Button
        Accessible.name: wb.tip
        Accessible.onPressAction: wb.clicked()

        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusMD
            color: !hover.hovered ? "transparent"
                 : wb.isClose ? Theme.closeButtonHover
                 : controls.onArt ? Qt.rgba(0, 0, 0, 0.35) : Theme.fillTertiary
            Behavior on color { ColorAnimation { duration: Dimensions.animVeryFast } }
        }
        Icon {
            anchors.centerIn: parent
            glyph: wb.glyph
            size: 16
            color: hover.hovered && wb.isClose ? "#FFFFFF"
                 : controls.onArt ? "#FFFFFF" : Theme.textSecondary
        }
        HoverHandler { id: hover }
        TapHandler { onTapped: wb.clicked() }
        StyledToolTip {
            visible: hover.hovered
            text: wb.tip
        }
    }
}
