// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TopDissolve — a scrolled page fades out under the window's top strip
 * instead of being cut there by a straight line.
 *
 * The page itself is masked (its top `size` pixels go from clear to opaque),
 * so what is under it — the lit background — shows through exactly. Only
 * while the page is scrolled: at rest nothing is layered and nothing costs.
 *
 *   TopDissolve { flick: page }
 *
 * MultiEffect's mask is a threshold, not a multiply: threshold 0.5 with full
 * spread turns the ramp into a smooth fade; threshold 0 makes every non-zero
 * alpha opaque.
 */
Item {
    id: dissolve

    property Flickable flick: null
    property real size: 56

    readonly property bool _on: dissolve.flick !== null && dissolve.flick.contentY > 4

    // Draws nothing itself; the ramp below only feeds the mask.
    height: 0

    Item {
        id: ramp
        width: dissolve.flick ? dissolve.flick.width : 0
        height: dissolve.flick ? dissolve.flick.height : 0
        visible: false
        layer.enabled: dissolve._on
        Rectangle {
            width: parent.width
            height: dissolve.size
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "white" }
            }
        }
        Rectangle {
            y: dissolve.size
            width: parent.width
            height: Math.max(0, parent.height - dissolve.size)
            color: "white"
        }
    }

    Component {
        id: fade
        MultiEffect {
            maskEnabled: true
            maskSource: ramp
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    function _attach() {
        if (!dissolve.flick) return
        dissolve.flick.layer.effect = fade
        dissolve.flick.layer.enabled = Qt.binding(() => dissolve._on)
    }
    onFlickChanged: _attach()
    Component.onCompleted: _attach()
}
