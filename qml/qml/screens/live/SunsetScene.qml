// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SunsetScene — a 16:9 sunset for the subtitle previews in console mode: our
 * own gradient, no game imagery. Bright sky, a burning horizon and dark ground
 * right where the subtitle sits: the hardest thing GTA VI puts behind a line.
 * `active` switches it on (GTA VI look only).
 */
Rectangle {
    property bool active: false

    anchors.fill: parent
    radius: Dimensions.radiusLG
    visible: active
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.gta6Sky }
        GradientStop { position: 0.42; color: Theme.gta6Sun[2] }
        GradientStop { position: 0.62; color: Theme.gta6Sun[1] }
        GradientStop { position: 0.76; color: Theme.gta6Sun[0] }
        GradientStop { position: 0.77; color: Theme.gta6Ink }
        GradientStop { position: 1.0; color: Theme.gta6Ink }
    }
}
