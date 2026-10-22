// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppNotice — a floating capsule that states something about the whole app
 * (offline, a failed integrity check): coloured glyph, one plain sentence,
 * an optional close button.
 */
Item {
    id: notice

    property string glyph: Icons.info
    property color tint: Theme.primary
    property string text: ""
    property bool busy: false
    property bool dismissible: false

    signal dismissed()

    implicitWidth: Math.min(row.implicitWidth + 28, 640)
    implicitHeight: Math.max(36, label.implicitHeight + 16)
    width: implicitWidth
    height: implicitHeight

    Accessible.role: Accessible.AlertMessage
    Accessible.name: notice.text

    RectangularShadow {
        anchors.fill: bg
        radius: bg.radius
        offset.y: 4
        blur: 16
        color: Theme.shadowMedium
    }
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Math.min(height / 2, 18)
        color: Theme.surfaceElevated
        border.width: 1
        border.color: Theme.withAlpha(notice.tint, 0.35)
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: notice.dismissible ? 6 : 14
        spacing: 9

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Icon {
                anchors.centerIn: parent
                visible: !notice.busy
                glyph: notice.glyph
                size: 16
                color: notice.tint
            }
            Spinner {
                anchors.centerIn: parent
                visible: notice.busy
                size: 15
                color: notice.tint
            }
        }
        Text {
            id: label
            Layout.fillWidth: true
            Layout.maximumWidth: 560
            textFormat: Text.PlainText
            text: notice.text
            wrapMode: Text.WordWrap
            font.pixelSize: Dimensions.callout
            font.weight: Font.Medium
            color: Theme.textPrimary
        }
        AppButton {
            visible: notice.dismissible
            size: "small"
            style: "plain"
            icon: Icons.x
            toolTip: qsTr("Kapat")
            tint: Theme.textSecondary
            onClicked: notice.dismissed()
        }
    }
}
