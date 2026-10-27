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
    // The same capsule for other short entries (an address, a code)
    property string glyph: Icons.search
    property alias validator: input.validator
    property alias maximumLength: input.maximumLength
    // Examples that roll by in the empty field ("Elden Ring", …), so it is
    // plain what to type; none: the placeholder stands still
    property var suggestions: []
    property bool large: false
    readonly property bool editing: input.activeFocus

    signal accepted()
    // You typed (not a text set from outside)
    signal edited()

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
        glyph: field.glyph
        size: field.large ? 17 : 15
        color: input.activeFocus || field.suggestions.length > 0 ? Theme.primaryText : Theme.textMuted
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
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
        onTextEdited: field.edited()
        Keys.onEscapePressed: (event) => {
            if (input.text !== "") input.text = ""
            else input.focus = false
            event.accepted = true
        }

        // The placeholder; with suggestions, an example rolls up into its
        // place every few seconds while the field waits empty
        Item {
            id: hint
            anchors.fill: parent
            visible: input.text === ""
            clip: true

            readonly property bool rolling: field.suggestions.length > 0 && !input.activeFocus
                                             && field.visible && Dimensions.animSlow > 0
            property int _next: 0
            property string _shown: field.placeholder

            function _roll() {
                const list = field.suggestions
                if (list.length === 0) return
                hintOld.text = hintNew.text
                hintNew.text = list[hint._next % list.length]
                hint._next++
                rollAnim.restart()
            }
            onRollingChanged: if (!rolling) {
                rollAnim.stop()
                hintNew.text = field.placeholder
                hintNew.y = 0; hintNew.opacity = 1
                hintOld.opacity = 0
            }
            Timer {
                interval: 2600
                repeat: true
                running: hint.rolling && hint.visible
                onTriggered: hint._roll()
            }

            Text {
                id: hintOld
                width: parent.width
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font: input.font
                color: Theme.textTertiary
                opacity: 0
            }
            Text {
                id: hintNew
                width: parent.width
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.PlainText
                text: field.placeholder
                elide: Text.ElideRight
                font: input.font
                color: Theme.textTertiary
            }
            ParallelAnimation {
                id: rollAnim
                NumberAnimation { target: hintOld; property: "y"; from: 0; to: -hint.height * 0.6; duration: 420; easing.type: Easing.InOutCubic }
                NumberAnimation { target: hintOld; property: "opacity"; from: 1; to: 0; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: hintNew; property: "y"; from: hint.height * 0.6; to: 0; duration: 520; easing.type: Easing.OutBack }
                NumberAnimation { target: hintNew; property: "opacity"; from: 0; to: 1; duration: 420; easing.type: Easing.OutCubic }
            }
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
                color: Theme.darkMode ? Theme.bgSecondary : "#FFFFFF"
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
