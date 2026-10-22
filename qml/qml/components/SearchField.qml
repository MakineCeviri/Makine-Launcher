// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SearchField — Apple's search field: a recessed capsule, magnifier on the
 * left, a clear button once there is text. Escape clears, then lets go.
 */
Item {
    id: field

    property alias text: input.text
    property string placeholder: qsTr("Ara")
    property bool large: false
    readonly property bool editing: input.activeFocus

    signal accepted()

    function focusField() {
        input.forceActiveFocus()
        input.selectAll()
    }
    // Focus with the caret after the text (typing carries on, nothing selected)
    function focusAtEnd() {
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
    }
    function clear() { input.text = "" }

    implicitHeight: field.large ? 38 : 32
    implicitWidth: 240

    Accessible.role: Accessible.EditableText
    Accessible.name: field.placeholder

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height / 2
        color: input.activeFocus ? (Theme.darkMode ? Theme.fillSecondary : Theme.surface) : Theme.fillTertiary
        border.width: input.activeFocus ? 0 : 0
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    }
    // Focus ring
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: height / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.45)
        visible: input.activeFocus
    }

    Icon {
        id: glass
        anchors.left: parent.left
        anchors.leftMargin: field.large ? 14 : 11
        anchors.verticalCenter: parent.verticalCenter
        glyph: Icons.search
        size: field.large ? 17 : 15
        color: Theme.textMuted
    }

    TextInput {
        id: input
        anchors.left: glass.right
        anchors.leftMargin: 8
        anchors.right: clearBtn.visible ? clearBtn.left : parent.right
        anchors.rightMargin: clearBtn.visible ? 4 : 12
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: field.large ? Dimensions.headline : Dimensions.body
        color: Theme.textPrimary
        selectionColor: Theme.primary40
        selectedTextColor: Theme.textPrimary
        clip: true
        selectByMouse: true
        onAccepted: field.accepted()
        Keys.onEscapePressed: (event) => {
            if (input.text !== "") input.text = ""
            else input.focus = false
            event.accepted = true
        }

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
            text: field.placeholder
            elide: Text.ElideRight
            font: input.font
            color: Theme.textTertiary
            visible: input.text === ""
        }
    }

    Item {
        id: clearBtn
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 22; height: 22
        visible: input.text !== ""
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Aramayı temizle")

        Rectangle {
            anchors.centerIn: parent
            width: 16; height: 16; radius: 8
            color: clearHover.hovered ? Theme.textSecondary : Theme.textTertiary
            Icon {
                anchors.centerIn: parent
                glyph: Icons.x
                size: 11
                color: Theme.darkMode ? "#1C1C1E" : "#FFFFFF"
            }
        }
        HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
        MouseArea {
            anchors.fill: parent
            onClicked: { input.text = ""; input.forceActiveFocus() }
        }
    }

    // A click anywhere in the capsule, not only on the text, starts typing.
    TapHandler {
        onTapped: input.forceActiveFocus()
    }
    HoverHandler { cursorShape: Qt.IBeamCursor }
}
