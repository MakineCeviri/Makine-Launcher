// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * WindowControls — the frameless window's caption buttons as Windows 11
 * draws them: 46 × 32 px, flush in the top right corner of the page panel
 * (of the window during the welcome flow), thin 10 px marks. A soft fill
 * under the pointer; close turns Windows red. A window in the background
 * dims its marks. Hide to the tray, minimise, close.
 */
Row {
    id: controls

    // Over cover art the marks are white
    property bool onArt: false
    // The tray needs the tray icon, which exists only after the welcome flow
    property bool showTray: true
    // The close button's outer corner follows the corner it sits in
    property real cornerRadius: 0

    signal trayClicked()
    signal minimizeClicked()
    signal closeClicked()

    readonly property bool _active: Window.window ? Window.window.active : true

    spacing: 0

    CaptionButton {
        visible: controls.showTray
        // An arrow down onto a line: put away, still running
        mark: "M 5 0.5 L 5 6.5 M 2 3.5 L 5 6.5 L 8 3.5 M 0 9.5 L 10 9.5"
        tip: qsTr("Tepsiye gizle")
        onClicked: controls.trayClicked()
    }
    CaptionButton {
        mark: "M 0 5 L 10 5"
        tip: qsTr("Küçült")
        onClicked: controls.minimizeClicked()
    }
    CaptionButton {
        mark: "M 0 0 L 10 10 M 10 0 L 0 10"
        tip: qsTr("Kapat")
        isClose: true
        onClicked: controls.closeClicked()
    }

    component CaptionButton: Item {
        id: button
        property string mark: ""
        property string tip: ""
        property bool isClose: false
        signal clicked()

        readonly property bool _red: button.isClose && hover.hovered

        width: 46
        height: 32

        Accessible.role: Accessible.Button
        Accessible.name: button.tip
        Accessible.onPressAction: button.clicked()

        Rectangle {
            anchors.fill: parent
            topRightRadius: button.isClose ? controls.cornerRadius : 0
            color: button._red ? (tap.pressed ? "#B3261A" : "#C42B1C")
                 : !hover.hovered ? "transparent"
                 : controls.onArt ? Qt.rgba(1, 1, 1, tap.pressed ? 0.08 : 0.12)
                 : Theme.darkMode ? Qt.rgba(1, 1, 1, tap.pressed ? 0.04 : 0.065)
                 : Qt.rgba(0, 0, 0, tap.pressed ? 0.03 : 0.045)
        }

        Shape {
            anchors.centerIn: parent
            width: 10
            height: 10
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: button._red || controls.onArt ? "#FFFFFF"
                           : controls._active || hover.hovered ? Theme.textPrimary : Theme.textTertiary
                strokeWidth: 1
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin
                PathSvg { path: button.mark }
            }
        }

        HoverHandler { id: hover }
        TapHandler {
            id: tap
            onTapped: button.clicked()
        }
        StyledToolTip {
            visible: hover.hovered
            text: button.tip
        }
    }
}
