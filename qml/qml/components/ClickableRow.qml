// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ClickableRow.qml - A row that leads somewhere (chevron) or acts.
 *
 * Two modes:
 * 1. Built-in layout: title/subtitle + icon (text glyph) or iconSource
 * 2. Custom content: contentItem with a Component
 * For new pages prefer ListRow { clickable: true }.
 */
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string icon: ""           // A glyph (Icons.*) or a short text
    property string iconSource: ""     // SVG/PNG icon path
    property bool showArrow: true
    property bool isDestructive: false
    property alias contentItem: contentLoader.sourceComponent

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.title
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Layout.fillWidth: true
    implicitHeight: Math.max(54, body.implicitHeight + 22)
    implicitWidth: 200

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Dimensions.radiusLG - 1
        color: mouseArea.pressed ? Theme.fillTertiary : Theme.fillQuaternary
        visible: mouseArea.containsMouse || root.activeFocus
    }

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 7
            color: root.isDestructive ? Theme.error : Theme.primary
            visible: root.icon !== "" || root.iconSource !== ""

            Text {
                anchors.centerIn: parent
                textFormat: Text.PlainText
                text: root.icon
                font.family: Icons.family
                font.pixelSize: 16
                color: "#FFFFFF"
                visible: root.icon !== "" && root.iconSource === ""
            }
            Image {
                anchors.centerIn: parent
                source: root.iconSource
                sourceSize: Qt.size(16, 16)
                visible: root.iconSource !== ""
                asynchronous: true
                mipmap: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: root.title !== ""

            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.title
                elide: Text.ElideRight
                font.pixelSize: Dimensions.body
                font.weight: Font.Medium
                color: root.isDestructive ? Theme.error : Theme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.subtitle
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
                visible: root.subtitle !== ""
            }
        }

        Loader {
            id: contentLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.title === "" && sourceComponent !== null
        }

        Icon {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            visible: root.showArrow
            glyph: Icons.chevronRight
            size: 15
            color: Theme.textTertiary
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
