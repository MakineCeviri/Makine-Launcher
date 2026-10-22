// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SectionContainer.qml - A Card that lays its children out in a column.
 */
Card {
    id: container

    // Content goes here via default property
    default property alias content: contentLayout.data

    // Kept for callers written for the old glow; it no longer draws anything.
    property string glowPosition: "top-right"

    // Expose inner layout for external anchoring/sizing
    property alias contentLayout: contentLayout
    property alias contentSpacing: contentLayout.spacing

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding

    readonly property int _padding: Dimensions.paddingXL

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: container._padding
        spacing: 8
    }
}
