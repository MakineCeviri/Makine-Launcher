// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * EdgeFade — where scrolling content meets a container's edge it fades out
 * instead of being cut off by a straight line: a shelf's covers at its sides,
 * a grid's rows under the bar above it. Lay it over the content, along the
 * edge it fades towards; `color` is the container's (Theme.containerSolid).
 */
Item {
    id: fade

    // "left" | "right" | "top" | "bottom"
    property string edge: "top"
    property real size: 28
    property color color: Theme.containerSolid
    // Hidden while nothing is under it (scrolled to that end)
    property bool active: true

    readonly property bool _horizontal: fade.edge === "left" || fade.edge === "right"

    width: fade._horizontal ? fade.size : 0
    height: fade._horizontal ? 0 : fade.size
    z: 5
    opacity: fade.active ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }

    Rectangle {
        id: ramp
        anchors.fill: parent
        // Gradient runs left→right or top→bottom; from the edge inwards.
        readonly property bool _fromStart: fade.edge === "left" || fade.edge === "top"
        gradient: Gradient {
            orientation: fade._horizontal ? Gradient.Horizontal : Gradient.Vertical
            GradientStop { position: 0.0; color: ramp._fromStart ? fade.color : "transparent" }
            GradientStop { position: ramp._fromStart ? 0.4 : 0.6; color: Theme.withAlpha(fade.color, 0.6) }
            GradientStop { position: 1.0; color: ramp._fromStart ? "transparent" : fade.color }
        }
    }
}
