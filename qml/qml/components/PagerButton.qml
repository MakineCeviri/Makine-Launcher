// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PagerButton — the round arrow on top of cover art that pages a carousel or
 * a shelf (the App Store's): smoked glass lit from above, a soft shadow, a
 * chevron drawn as two strokes so it sits optically centred. Under the
 * pointer it grows a little and the chevron nudges the way it pages; a press
 * sinks it.
 *
 *   PagerButton { direction: 1; onClicked: view.page(1) }
 */
Item {
    id: btn

    // -1 back (chevron left), 1 forward (chevron right)
    property int direction: 1
    property real size: 40
    property string toolTip: direction < 0 ? qsTr("Önceki") : qsTr("Sonraki")

    signal clicked()

    implicitWidth: btn.size
    implicitHeight: btn.size

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: btn.toolTip
    Keys.onReturnPressed: btn.clicked()
    Keys.onSpacePressed: btn.clicked()

    readonly property bool _hovered: hover.hovered
    scale: tap.pressed ? 0.92 : btn._hovered ? 1.07 : 1
    Behavior on scale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }

    RectangularShadow {
        anchors.fill: glass
        radius: glass.radius
        offset.y: 4
        blur: 18
        spread: -2
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    Rectangle {
        id: glass
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(0.06, 0.06, 0.08, btn._hovered ? 0.72 : 0.55)
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        // Lit from above, like the containers
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.16) }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.03) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, btn._hovered ? 0.28 : 0.18)
        }
    }

    // The chevron: two round-capped strokes, a hair off centre towards where
    // it points so its weight sits in the middle
    Shape {
        id: chevron
        readonly property real arm: btn.size * 0.15
        width: btn.size
        height: btn.size
        preferredRendererType: Shape.CurveRenderer
        transform: Translate { id: nudge; x: 0 }
        ShapePath {
            strokeColor: "#FFFFFF"
            strokeWidth: Math.max(1.8, btn.size * 0.055)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: btn.size / 2 - btn.direction * chevron.arm * 0.45
            startY: btn.size / 2 - chevron.arm
            PathLine { x: btn.size / 2 + btn.direction * chevron.arm * 0.55; y: btn.size / 2 }
            PathLine { x: btn.size / 2 - btn.direction * chevron.arm * 0.45; y: btn.size / 2 + chevron.arm }
        }
    }

    // Arriving under the pointer, the chevron leans the way it pages
    SequentialAnimation {
        id: lean
        NumberAnimation { target: nudge; property: "x"; to: btn.direction * 3; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: nudge; property: "x"; to: 0; duration: 360; easing.type: Easing.OutBack }
    }

    // Focus ring
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: width / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.5)
        visible: btn.activeFocus
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
        onHoveredChanged: if (hovered && Dimensions.animNormal > 0) lean.restart()
    }
    // A MouseArea, not a TapHandler: it takes the press, so the card or
    // carousel under the arrow does not open on the same click
    MouseArea {
        id: tap
        anchors.fill: parent
        onClicked: btn.clicked()
    }

    StyledToolTip {
        visible: hover.hovered
        delay: 700
        text: btn.toolTip
    }
}
