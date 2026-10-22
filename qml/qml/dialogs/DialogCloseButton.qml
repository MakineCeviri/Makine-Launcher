// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * DialogCloseButton.qml - The round close (X) button of a sheet's header
 *
 * Usage:
 *   DialogCloseButton { onClicked: { root.cancelled(); root.close() } }
 */
Rectangle {
    id: btn

    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: 14
    color: _mouse.containsMouse ? Theme.fillSecondary : Theme.fillTertiary

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Kapat")
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    Icon {
        anchors.centerIn: parent
        glyph: Icons.x
        size: 14
        color: _mouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
    }

    MouseArea {
        id: _mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}
