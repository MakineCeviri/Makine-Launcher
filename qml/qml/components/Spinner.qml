// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Spinner — Apple's activity indicator: eight spokes, the lit one going round.
 * A timer steps the head; nothing runs while it is hidden.
 */
Item {
    id: spinner

    property real size: 18
    property color color: Theme.textSecondary
    property bool running: visible

    implicitWidth: size
    implicitHeight: size
    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Yükleniyor")

    property int _head: 0

    Timer {
        interval: 100
        repeat: true
        running: spinner.running && spinner.visible
        onTriggered: spinner._head = (spinner._head + 1) % 8
    }

    Repeater {
        model: 8

        Rectangle {
            id: spoke
            required property int index
            readonly property int _age: (spinner._head - spoke.index + 8) % 8
            width: Math.max(1.5, spinner.size * 0.1)
            height: spinner.size * 0.28
            radius: width / 2
            x: (spinner.width - width) / 2
            y: 0
            color: spinner.color
            opacity: 1.0 - spoke._age * 0.11
            transform: Rotation {
                origin.x: spoke.width / 2
                origin.y: spinner.height / 2
                angle: spoke.index * 45
            }
        }
    }
}
