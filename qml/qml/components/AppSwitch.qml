// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppSwitch — Apple's switch: a capsule that fills with the accent, a white
 * knob that slides across. Emits toggled(); the owner flips `checked`.
 */
Item {
    id: sw

    property bool checked: false
    property bool animate: true
    property string accessibleName: ""

    signal toggled()

    implicitWidth: Dimensions.toggleWidth
    implicitHeight: Dimensions.toggleHeight

    activeFocusOnTab: sw.enabled
    Accessible.role: Accessible.CheckBox
    Accessible.name: sw.accessibleName
    Accessible.checked: sw.checked
    Accessible.onToggleAction: if (sw.enabled) sw.toggled()
    Keys.onSpacePressed: if (sw.enabled) sw.toggled()
    Keys.onReturnPressed: if (sw.enabled) sw.toggled()

    readonly property int _ms: sw.animate ? Dimensions.animNormal : 0

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: sw.checked ? Theme.primary : Theme.fill
        opacity: sw.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: sw._ms; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: height / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.45)
        visible: sw.activeFocus
    }

    Rectangle {
        id: knob
        readonly property real _w: tap.pressed ? Dimensions.toggleKnobSize + 4 : Dimensions.toggleKnobSize
        width: _w
        height: Dimensions.toggleKnobSize
        radius: height / 2
        y: (sw.height - height) / 2
        x: sw.checked ? sw.width - width - 2 : 2
        color: "#FFFFFF"
        border.width: 0.5
        border.color: Qt.rgba(0, 0, 0, 0.08)
        Behavior on x { NumberAnimation { duration: sw._ms; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Dimensions.animFast } }
    }

    HoverHandler { cursorShape: sw.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    // Takes the press: a clickable row under the switch stays still.
    MouseArea {
        id: tap
        anchors.fill: parent
        anchors.margins: -4
        enabled: sw.enabled
        onClicked: sw.toggled()
    }
}
