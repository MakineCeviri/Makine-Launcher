// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * ConfirmDialog.qml - An Apple-style alert for an action worth a second
 * thought: a symbol, the question, what will happen, "Vazgeç" and the action.
 *
 * Usage:
 *   ConfirmDialog {
 *       title: qsTr("Türkçe yama kaldırılsın mı?")
 *       message: qsTr("…")
 *       confirmText: qsTr("Yamayı kaldır")
 *       onConfirmed: doIt()
 *   }
 */
BaseDialog {
    id: root

    property string message: ""
    property string confirmText: qsTr("Onayla")
    property string cancelText: qsTr("Vazgeç")
    property string glyph: Icons.triangleAlert
    accentColor: Theme.error

    signal confirmed()

    width: 380
    contentHeight: contentColumn.implicitHeight

    header: Item { implicitHeight: 0 }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 0

        IconTile {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 26
            size: 48
            glyph: root.glyph
            tint: root.accentColor
            solid: false
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.topMargin: 14
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            horizontalAlignment: Text.AlignHCenter
            text: root.title
            font.pixelSize: Dimensions.headline
            font.weight: Font.Bold
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 20
            horizontalAlignment: Text.AlignHCenter
            text: root.message
            font.pixelSize: Dimensions.callout
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.25
            visible: root.message !== ""
        }
    }

    footer: Item {
        implicitHeight: buttons.implicitHeight + 40

        RowLayout {
            id: buttons
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 10

            AppButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                fillWidth: true
                style: "gray"
                text: root.cancelText
                onClicked: { root.cancelled(); root.close() }
            }
            AppButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                fillWidth: true
                style: "filled"
                tint: root.accentColor
                text: root.confirmText
                onClicked: { root.confirmed(); root.close() }
            }
        }
    }
}
