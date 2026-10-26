// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * EdgeFade — where scrolling content meets a container's edge it fades out
 * instead of being cut off by a straight line: a shelf's covers at its sides,
 * a grid's rows under the bar above it and at the container's foot.
 *
 * The content itself is masked (its edges go from opaque to clear), so what
 * is under it — a translucent container, the lit background — shows through
 * exactly; a band painted in the container's colour never quite matched it.
 * One axis at a time: top and bottom, or left and right. Layered only while
 * an edge fades.
 *
 *   EdgeFade { target: grid; topFade: grid.atYBeginning ? 0 : 26; bottomFade: grid.atYEnd ? 0 : 34 }
 */
Item {
    id: fade

    property Item target: null
    // How far each edge fades (0: not at all); a change eases in and out
    property real topFade: 0
    property real bottomFade: 0
    property real leftFade: 0
    property real rightFade: 0

    Behavior on topFade { NumberAnimation { duration: Dimensions.animFast } }
    Behavior on bottomFade { NumberAnimation { duration: Dimensions.animFast } }
    Behavior on leftFade { NumberAnimation { duration: Dimensions.animFast } }
    Behavior on rightFade { NumberAnimation { duration: Dimensions.animFast } }

    readonly property bool _horizontal: fade.leftFade > 0 || fade.rightFade > 0
    readonly property bool _on: fade.target !== null && fade.target.visible
                                && (fade.topFade > 0.5 || fade.bottomFade > 0.5 || fade.leftFade > 0.5 || fade.rightFade > 0.5)
    readonly property real _length: fade.target ? (fade._horizontal ? fade.target.width : fade.target.height) : 0

    // Draws nothing itself; the ramp only feeds the mask.
    width: 0
    height: 0

    Rectangle {
        id: ramp
        width: fade.target ? fade.target.width : 0
        height: fade.target ? fade.target.height : 0
        visible: false
        layer.enabled: fade._on
        readonly property real _start: fade._length > 0 ? (fade._horizontal ? fade.leftFade : fade.topFade) / fade._length : 0
        readonly property real _end: fade._length > 0 ? (fade._horizontal ? fade.rightFade : fade.bottomFade) / fade._length : 0
        gradient: Gradient {
            orientation: fade._horizontal ? Gradient.Horizontal : Gradient.Vertical
            GradientStop { position: 0.0; color: ramp._start > 0 ? "transparent" : "white" }
            GradientStop { position: Math.max(0.001, ramp._start); color: "white" }
            GradientStop { position: Math.min(0.999, 1 - ramp._end); color: "white" }
            GradientStop { position: 1.0; color: ramp._end > 0 ? "transparent" : "white" }
        }
    }

    // MultiEffect's mask is a threshold, not a multiply: threshold 0.5 with
    // full spread turns the ramp into a smooth fade (as TopDissolve does).
    Component {
        id: mask
        MultiEffect {
            maskEnabled: true
            maskSource: ramp
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    property Item _attached: null
    function _attach() {
        if (fade._attached && fade._attached !== fade.target)
            fade._attached.layer.enabled = false
        fade._attached = fade.target
        if (!fade.target) return
        fade.target.layer.effect = mask
        fade.target.layer.enabled = Qt.binding(() => fade._on)
    }
    onTargetChanged: _attach()
    Component.onCompleted: _attach()
}
