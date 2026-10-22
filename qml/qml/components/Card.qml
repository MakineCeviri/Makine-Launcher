// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Card — a rounded content surface. Dark: a lifted grey with a hairline.
 * Light: white on the grey page with a soft shadow, as on Apple's sites.
 */
Rectangle {
    id: card

    property bool elevated: !Theme.darkMode
    property bool hairline: Theme.darkMode

    radius: Dimensions.radiusCard
    color: Theme.surface
    border.width: card.hairline ? 1 : 0
    border.color: Theme.separator

    RectangularShadow {
        z: -1
        visible: card.elevated && card.visible
        anchors.fill: parent
        radius: card.radius
        offset.y: 1
        blur: 10
        spread: 0
        color: Qt.rgba(0, 0, 0, 0.07)
    }
}
