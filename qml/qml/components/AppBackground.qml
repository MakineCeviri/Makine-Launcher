// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppBackground — the page under everything: near-black, lit by two soft
 * orbs of the accent (upper right, lower left) and darkened at the rim, as
 * 0.1.5 had it. Gradient fills on the GPU: nothing is painted on resize.
 */
Rectangle {
    id: bg

    color: Theme.bgPrimary

    readonly property color _a: Theme.accentLight
    readonly property color _b: Theme.accentDark
    readonly property real _dim: Math.max(width, height)
    readonly property real _k: Theme.darkMode ? 1.0 : 0.45

    component Orb: Shape {
        id: orb
        property real cx: 0
        property real cy: 0
        property real r: 100
        property color tint: "white"
        property real strength: 0.18

        anchors.fill: parent
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: orb.cx; centerY: orb.cy; centerRadius: orb.r
                focalX: orb.cx; focalY: orb.cy
                GradientStop { position: 0.00; color: Theme.withAlpha(orb.tint, orb.strength) }
                GradientStop { position: 0.12; color: Theme.withAlpha(orb.tint, orb.strength * 0.72) }
                GradientStop { position: 0.30; color: Theme.withAlpha(orb.tint, orb.strength * 0.30) }
                GradientStop { position: 0.55; color: Theme.withAlpha(orb.tint, orb.strength * 0.08) }
                GradientStop { position: 1.00; color: "transparent" }
            }
            PathRectangle { x: 0; y: 0; width: orb.width; height: orb.height }
        }
    }

    Orb {
        cx: bg.width * 0.78; cy: bg.height * 0.12; r: bg._dim * 0.85
        tint: bg._a; strength: 0.22 * bg._k
    }
    Orb {
        cx: bg.width * 0.16; cy: bg.height * 0.92; r: bg._dim * 0.75
        tint: bg._b; strength: 0.19 * bg._k
    }

    // Vignette: the rim a touch darker, the way a lit room falls off
    Shape {
        anchors.fill: parent
        visible: Theme.darkMode
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: bg.width * 0.5; centerY: bg.height * 0.45; centerRadius: bg._dim * 0.95
                focalX: bg.width * 0.5; focalY: bg.height * 0.45
                GradientStop { position: 0.55; color: "transparent" }
                GradientStop { position: 0.85; color: Qt.rgba(0, 0, 0, 0.10) }
                GradientStop { position: 1.00; color: Qt.rgba(0, 0, 0, 0.22) }
            }
            PathRectangle { x: 0; y: 0; width: bg.width; height: bg.height }
        }
    }
}
