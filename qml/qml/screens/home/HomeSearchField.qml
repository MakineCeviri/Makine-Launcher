// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeSearchField — search over every patch, at the top of the home page.
 *
 * The old box sat at 6 % fill on an 8 % border and could not be told from
 * the page. This one has a solid surface, a visible edge and its shortcut in
 * the placeholder. `query` follows the text after a short pause.
 */
TextField {
    id: field

    property int total: 0
    property string query: ""

    implicitHeight: 44
    leftPadding: 44
    rightPadding: clearButton.visible ? 40 : Dimensions.paddingLG
    font.pixelSize: Dimensions.fontMD
    color: Theme.textPrimary
    placeholderText: field.total > 0 ? qsTr("%1 oyunda Türkçe yama ara   (Ctrl+F)").arg(field.total)
                                     : qsTr("Türkçe yama ara   (Ctrl+F)")
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    Accessible.name: qsTr("Türkçe yama ara")

    onTextChanged: debounce.restart()
    Keys.onEscapePressed: {
        if (field.text !== "") field.text = ""
        else field.focus = false
    }

    Timer {
        id: debounce
        interval: 120
        onTriggered: field.query = field.text.trim()
    }

    background: Rectangle {
        radius: Dimensions.radiusLG
        color: field.activeFocus ? Theme.surfaceElevated : Theme.surface
        border.width: 1
        border.color: field.activeFocus ? Theme.primary : Theme.textPrimary12
        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.paddingLG
        anchors.verticalCenter: parent.verticalCenter
        textFormat: Text.PlainText
        text: ""
        font.family: "Segoe MDL2 Assets"
        font.pixelSize: 15
        color: field.activeFocus ? Theme.primary : Theme.textMuted
    }

    Rectangle {
        id: clearButton
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.spacingMD
        anchors.verticalCenter: parent.verticalCenter
        width: 28; height: 28; radius: 14
        visible: field.text !== ""
        color: clearHover.hovered ? Theme.textPrimary12 : "transparent"
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Aramayı temizle")

        Text {
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: ""
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 11
            color: Theme.textSecondary
        }
        HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                field.text = ""
                field.forceActiveFocus()
            }
        }
    }
}
