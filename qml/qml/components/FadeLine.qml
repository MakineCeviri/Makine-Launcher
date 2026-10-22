// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * FadeLine — a divider that fades out at both ends instead of stopping dead:
 * between a container's head and its content, between rows of a shelf.
 */
Rectangle {
    id: line

    property color tint: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.10)

    Layout.fillWidth: true
    implicitHeight: 1
    height: 1
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: "transparent" }
        GradientStop { position: 0.2; color: line.tint }
        GradientStop { position: 0.8; color: line.tint }
        GradientStop { position: 1.0; color: "transparent" }
    }
}
