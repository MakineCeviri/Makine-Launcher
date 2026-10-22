// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsCard.qml - A grouped-list card; children are stacked with no gap.
 * For new pages prefer ListSection.
 */
Card {
    default property alias content: _cc.data
    implicitHeight: _cc.implicitHeight
    radius: Dimensions.radiusLG
    ColumnLayout { id: _cc; anchors.fill: parent; spacing: 0 }
}
