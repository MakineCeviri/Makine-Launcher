// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GlassButton — Apple's liquid glass back button: a drop of glass over the
 * page. What is under it shows through blurred, a touch brighter and richer
 * (`backdrop`: the item it floats over), light catches its top edge and
 * gathers along its foot, a soft shadow lifts it. It rests small and quiet —
 * a round drop with its chevron — and under the pointer (or with focus) it
 * opens to its `text`, where it leads; the chevron leans the way it goes, a
 * press squeezes it.
 *
 *   GlassButton { backdrop: page; text: "Kütüphanem"; onClicked: … }
 */
Item {
    id: glass

    property string text: ""
    // The chevron's way: -1 back, 1 forward, 0 none
    property int direction: -1
    property string toolTip: ""
    // What the glass floats over, for the blur (none: tinted glass alone)
    property Item backdrop: null

    signal clicked()

    readonly property bool _hovered: hover.hovered
    readonly property bool _open: glass.text !== "" && (glass._hovered || glass.activeFocus)
    readonly property real _h: 32

    implicitHeight: glass._h
    implicitWidth: glass._open ? row.x + row.implicitWidth + 14 : glass._h
    Behavior on implicitWidth { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutExpo } }
    width: implicitWidth

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: glass.text !== "" ? glass.text : glass.toolTip
    Keys.onReturnPressed: glass.clicked()
    Keys.onSpacePressed: glass.clicked()

    scale: tap.pressed ? 0.93 : 1
    Behavior on scale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack; easing.overshoot: 2 } }

    RectangularShadow {
        anchors.fill: drop
        radius: drop.radius
        offset.y: glass._hovered ? 4 : 2
        blur: glass._hovered ? 16 : 10
        spread: -3
        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.32 : 0.12)
        Behavior on offset.y { NumberAnimation { duration: Dimensions.animNormal } }
    }

    // ── The drop ──
    Item {
        id: drop
        readonly property real radius: height / 2
        anchors.fill: parent

        // What is under it, blurred and lifted, cut to the capsule
        ShaderEffectSource {
            id: under
            visible: false
            sourceItem: glass.backdrop
            live: true
            sourceRect: {
                if (!glass.backdrop) return Qt.rect(0, 0, 0, 0)
                const p = glass.mapToItem(glass.backdrop, 0, 0)
                return Qt.rect(p.x, p.y, glass.width, glass.height)
            }
        }
        MultiEffect {
            anchors.fill: parent
            visible: glass.backdrop !== null
            source: under
            blurEnabled: true
            // Padding grew the output past the drop and stretched the mask
            // with it: a large grey disc around the button
            autoPaddingEnabled: false
            blurMax: 32
            blur: 0.9
            saturation: 0.35
            brightness: Theme.darkMode ? 0.08 : 0.2
            maskEnabled: true
            maskSource: capsule
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
        Item {
            id: capsule
            anchors.fill: parent
            visible: false
            layer.enabled: true
            Rectangle { anchors.fill: parent; radius: drop.radius; color: "black" }
        }

        // The glass itself: a body, light on its top, light gathering at its foot
        Rectangle {
            anchors.fill: parent
            radius: drop.radius
            color: Theme.darkMode ? Qt.rgba(1, 1, 1, glass._hovered ? 0.12 : 0.06)
                                  : Qt.rgba(1, 1, 1, glass._hovered ? 0.6 : 0.42)
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }
        Rectangle {
            x: 3; y: 1.5
            width: parent.width - 6
            height: parent.height * 0.5
            radius: height / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.16 : 0.6) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
            }
        }
        Rectangle {
            x: 6
            y: parent.height * 0.55
            width: parent.width - 12
            height: parent.height * 0.42
            radius: height / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.06 : 0.25) }
            }
        }
        // Its rim: bright where the light falls, faint below
        Rectangle {
            anchors.fill: parent
            radius: drop.radius
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? (glass._hovered ? 0.24 : 0.13) : 0.7)
        }
    }

    // The chevron stays where it is; the words come in after it, cut by the
    // drop's edge while it opens
    Item {
        anchors.fill: parent
        clip: true
    Row {
        id: row
        x: Math.round((glass._h - chev.width) / 2)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7

        // The chevron, drawn: two round-capped strokes
        Shape {
            id: chev
            visible: glass.direction !== 0
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 16
            preferredRendererType: Shape.CurveRenderer
            transform: Translate { id: lean; x: 0 }
            ShapePath {
                strokeColor: glass._hovered ? Theme.textPrimary : Theme.textSecondary
                strokeWidth: 2.2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: glass.direction < 0 ? 7.5 : 2.5
                startY: 2
                PathLine { x: glass.direction < 0 ? 2.5 : 7.5; y: 8 }
                PathLine { x: glass.direction < 0 ? 7.5 : 2.5; y: 14 }
            }
        }
        Text {
            visible: glass.text !== ""
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: glass.text
            font.pixelSize: Dimensions.footnote
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            opacity: glass._open ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: glass._open ? Dimensions.animSlow : Dimensions.animFast } }
        }
    }
    }

    // Arriving under the pointer, the chevron leans the way it goes
    SequentialAnimation {
        id: leanAnim
        NumberAnimation { target: lean; property: "x"; to: glass.direction * 3; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: lean; property: "x"; to: 0; duration: 380; easing.type: Easing.OutBack }
    }

    // Focus ring
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: height / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.5)
        visible: glass.activeFocus
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
        onHoveredChanged: if (hovered && Dimensions.animNormal > 0) leanAnim.restart()
    }
    MouseArea {
        id: tap
        anchors.fill: parent
        onClicked: glass.clicked()
    }

    StyledToolTip {
        visible: glass.toolTip !== "" && hover.hovered
        delay: 700
        text: glass.toolTip
    }
}
