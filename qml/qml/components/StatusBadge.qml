// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * StatusBadge — a small capsule saying a state: coloured dot + word.
 * onArt: dark glass that reads on any cover; otherwise a wash of the colour.
 */
Rectangle {
    id: badge

    property string text: ""
    property color tint: Theme.primary
    property bool onArt: false
    property bool showDot: true

    visible: badge.text !== ""
    implicitHeight: 22
    implicitWidth: row.implicitWidth + 16
    radius: height / 2
    color: badge.onArt ? Qt.rgba(0, 0, 0, 0.62) : Theme.withAlpha(badge.tint, Theme.darkMode ? 0.16 : 0.12)

    Accessible.role: Accessible.StaticText
    Accessible.name: badge.text

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 5

        Rectangle {
            visible: badge.showDot
            anchors.verticalCenter: parent.verticalCenter
            width: 6; height: 6; radius: 3
            color: badge.tint
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: badge.text
            font.pixelSize: Dimensions.caption1
            font.weight: Font.DemiBold
            color: badge.onArt ? "#FFFFFF" : badge.tint
        }
    }
}
