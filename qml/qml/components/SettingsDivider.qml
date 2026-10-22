// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsDivider.qml - A hairline between rows.
 * variant "settings" (default): inset from the left, as between list rows
 * variant "section": full width, between a heading and its content
 */
Rectangle {
    property string variant: "settings"

    Layout.fillWidth: true
    Layout.leftMargin: variant === "section" ? 0 : 16
    Layout.preferredHeight: 1
    color: Theme.separator
}
