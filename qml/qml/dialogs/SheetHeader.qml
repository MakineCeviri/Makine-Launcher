// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * SheetHeader — the top of a sheet: a symbol in the sheet's tone, the title
 * (and a line under it), the round close button.
 */
Item {
    id: header

    property string glyph: Icons.info
    property color tint: Theme.primary
    property string title: ""
    property string subtitle: ""
    property bool closable: true

    signal closeClicked()

    implicitHeight: row.implicitHeight + 40

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 22
        anchors.leftMargin: 22
        anchors.rightMargin: 16
        spacing: 12

        IconTile {
            Layout.alignment: Qt.AlignTop
            size: 36
            glyph: header.glyph
            tint: header.tint
            solid: false
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: header.title
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.headline
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Text {
                visible: header.subtitle !== ""
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: header.subtitle
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }
        }
        DialogCloseButton {
            visible: header.closable
            Layout.alignment: Qt.AlignTop
            onClicked: header.closeClicked()
        }
    }
}
