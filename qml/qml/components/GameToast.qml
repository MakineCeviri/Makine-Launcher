// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * GameToast - a notification banner, macOS style: it slides in at the top
 * right with the launcher's icon, a bold line and a sentence, and leaves on
 * its own after 8 s (sooner with its close button). Used for a detected
 * game, an install that ended while its page was closed, plugin notices.
 */
Item {
    id: root

    property string gameName: ""
    property string gameId: ""
    property string customTitle: ""
    property string customSubtitle: ""
    property bool shown: false

    anchors.top: parent.top
    anchors.right: parent.right
    anchors.topMargin: 52
    anchors.rightMargin: shown ? 16 : -width - 32
    width: Math.min(380, parent.width - 48)
    height: toastCard.height
    z: Dimensions.zToast
    visible: shown || anchors.rightMargin > -width - 30
    opacity: shown ? 1 : 0

    Behavior on anchors.rightMargin {
        NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
    }
    Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: titleLabel.text
    Accessible.description: subtitleLabel.text

    // Auto-dismiss after 8 seconds (not while the pointer is on it)
    Timer {
        id: autoHide
        interval: 8000
        running: root.shown && !cardHover.hovered
        onTriggered: root.shown = false
    }

    function show(gId, gName, title, subtitle) {
        root.gameId = gId
        root.gameName = gName
        root.customTitle = title || ""
        root.customSubtitle = subtitle || ""
        root.shown = true
        autoHide.restart()
    }

    RectangularShadow {
        anchors.fill: toastCard
        radius: toastCard.radius
        offset.y: 8
        blur: 28
        spread: -4
        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.5 : 0.18)
    }

    Rectangle {
        id: toastCard
        width: parent.width
        height: contentLayout.implicitHeight + 26
        radius: 16
        color: Theme.surfaceElevated
        border.width: 1
        border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.06)

        HoverHandler { id: cardHover }

        RowLayout {
            id: contentLayout
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14; anchors.rightMargin: 10
            spacing: 12

            Image {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                Layout.alignment: Qt.AlignTop
                source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
                sourceSize: Qt.size(68, 68)
                fillMode: Image.PreserveAspectFit
                mipmap: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    id: titleLabel
                    textFormat: Text.PlainText
                    text: root.customTitle || qsTr("%1 bulundu").arg(root.gameName)
                    font.pixelSize: Dimensions.callout; font.weight: Font.Bold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    id: subtitleLabel
                    textFormat: Text.PlainText
                    text: root.customSubtitle || qsTr("Türkçe yamayı kurmak için önce oyunu kapat, sonra Kütüphanem'den oyunun sayfasını aç.")
                    font.pixelSize: Dimensions.footnote; color: Theme.textSecondary
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                    lineHeight: 1.2
                }
            }

            Rectangle {
                Layout.preferredWidth: 22; Layout.preferredHeight: 22
                Layout.alignment: Qt.AlignTop
                radius: 11
                opacity: cardHover.hovered ? 1 : 0
                color: closeMa.containsMouse ? Theme.fillSecondary : Theme.fillTertiary
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Kapat")

                Icon {
                    anchors.centerIn: parent
                    glyph: Icons.x
                    size: 12
                    color: Theme.textSecondary
                }
                MouseArea {
                    id: closeMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.shown = false
                }
            }
        }
    }
}
