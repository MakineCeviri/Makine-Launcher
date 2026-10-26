// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Card — a container: every section, list and panel sits on one.
 *
 * Dark: translucent near-black over the lit page, a hairline, and a top edge
 * catching the light (brightest in the middle, gone at the corners) — the
 * edge 0.1.5's containers had. `glow` adds a soft accent light in a corner
 * for the container that leads a page. Light: white with a soft shadow.
 *
 * Each time it comes into view (its page opens) it rises into place: a
 * short fade and lift, later the lower it sits, so a page assembles from
 * the top down. `reveal: false` keeps it still.
 */
Rectangle {
    id: card

    property bool elevated: !Theme.darkMode
    property bool hairline: true
    property bool lit: Theme.darkMode
    property bool glow: false
    // "topRight" | "topLeft" | "bottomLeft" | "bottomRight"
    property string glowCorner: "topRight"
    property real glowStrength: 0.18
    property bool reveal: true

    transform: Translate { id: rise }
    onVisibleChanged: if (visible) _reveal()
    Component.onCompleted: _reveal()
    function _reveal() {
        if (!card.reveal || Dimensions.animSlow === 0) return
        card.opacity = 0
        rise.y = 14
        arrive.restart()
    }
    SequentialAnimation {
        id: arrive
        // Lower on the page, a little later (capped: a long page does not wait)
        PauseAnimation { duration: Math.round(Math.min(Math.max(card.y, 0) * 0.12, 200)) }
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: 320; easing.type: Easing.OutCubic }
            NumberAnimation { target: rise; property: "y"; to: 0; duration: 560; easing.type: Easing.OutExpo }
        }
    }

    radius: Dimensions.radiusCard
    color: Theme.container
    border.width: card.hairline ? 1 : 0
    border.color: Theme.containerBorder

    RectangularShadow {
        z: -1
        visible: card.elevated && card.visible
        anchors.fill: parent
        radius: card.radius
        offset.y: 1
        blur: 12
        spread: 0
        color: Qt.rgba(0, 0, 0, 0.07)
    }

    // Accent light in a corner, shaped like the card itself
    Shape {
        id: glowShape
        anchors.fill: parent
        visible: card.glow && Theme.darkMode
        readonly property real _gx: card.glowCorner === "topLeft" || card.glowCorner === "bottomLeft"
                                    ? card.width * 0.08 : card.width * 0.92
        readonly property real _gy: card.glowCorner === "bottomLeft" || card.glowCorner === "bottomRight"
                                    ? card.height : 0
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: glowShape._gx; centerY: glowShape._gy
                centerRadius: Math.max(card.width, card.height) * 0.6
                focalX: glowShape._gx; focalY: glowShape._gy
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.accentBase, card.glowStrength) }
                GradientStop { position: 0.35; color: Theme.withAlpha(Theme.accentBase, card.glowStrength * 0.4) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            PathRectangle {
                x: 1; y: 1; width: card.width - 2; height: card.height - 2
                radius: Math.max(0, card.radius - 1)
            }
        }
    }

    // The lit top edge
    Rectangle {
        visible: card.lit && card.hairline
        x: card.radius * 0.7
        width: card.width - card.radius * 1.4
        y: 0
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: Theme.containerHighlight }
            GradientStop { position: 0.7; color: Theme.containerHighlight }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
