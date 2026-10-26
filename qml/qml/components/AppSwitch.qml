// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppSwitch — Apple's switch, with some life: the capsule fills with the
 * accent lit from above and glows softly while on; the white knob springs
 * across (a little past, then back), stretches the way it is about to go
 * while pressed, and carries a small check while on. Emits toggled(); the
 * owner flips `checked`.
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
    readonly property bool _hovered: hover.hovered && sw.enabled
    readonly property real _gap: 2

    // A soft accent glow under the capsule while on
    RectangularShadow {
        anchors.fill: track
        radius: track.radius
        blur: 14
        spread: -2
        color: Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.55 : 0.35)
        opacity: sw.checked && sw.enabled ? (sw._hovered ? 1 : 0.7) : 0
        Behavior on opacity { NumberAnimation { duration: sw._ms } }
    }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: sw.checked ? Theme.primary
             : sw._hovered ? Theme.fillSecondary : Theme.fill
        opacity: sw.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: sw._ms; easing.type: Easing.OutCubic } }

        // Lit from above while on; a faint inner shade while off
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: sw.checked ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.10) }
                GradientStop { position: 0.6; color: Qt.rgba(1, 1, 1, 0) }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: sw.checked ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, Theme.darkMode ? 0.06 : 0)
        }
    }

    // Focus ring
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: height / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.45)
        visible: sw.activeFocus
    }

    // The knob: its shadow, then itself
    RectangularShadow {
        anchors.fill: knob
        radius: knob.radius
        offset.y: 1.5
        blur: 6
        color: Qt.rgba(0, 0, 0, 0.32)
    }
    Rectangle {
        id: knob
        readonly property real _rest: Dimensions.toggleKnobSize
        // Pressed, it stretches towards where it will go
        width: tap.pressed ? _rest + 6 : _rest
        height: _rest
        radius: height / 2
        y: (sw.height - height) / 2
        x: sw.checked ? sw.width - width - sw._gap : sw._gap
        scale: sw._hovered && !tap.pressed ? 1.04 : 1
        color: "#FFFFFF"
        border.width: 0.5
        border.color: Qt.rgba(0, 0, 0, 0.08)
        Behavior on x { NumberAnimation { duration: sw._ms * 1.3; easing.type: Easing.OutBack; easing.overshoot: 1.6 } }
        Behavior on width { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Dimensions.animFast } }

        // A small check while on, in the accent
        Icon {
            anchors.centerIn: parent
            glyph: Icons.check
            size: Math.round(knob.height * 0.55)
            color: Theme.primary
            opacity: sw.checked ? 1 : 0
            scale: sw.checked ? 1 : 0.4
            Behavior on opacity { NumberAnimation { duration: sw._ms } }
            Behavior on scale { NumberAnimation { duration: sw._ms * 1.3; easing.type: Easing.OutBack } }
        }
    }

    HoverHandler { id: hover; cursorShape: sw.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    // Takes the press: a clickable row under the switch stays still.
    MouseArea {
        id: tap
        anchors.fill: parent
        anchors.margins: -4
        enabled: sw.enabled
        onClicked: sw.toggled()
    }
}
