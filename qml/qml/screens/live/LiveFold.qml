// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * LiveFold — a collapsed group: one clickable row; its content shows when open.
 */
ColumnLayout {
    id: fold

    property string title: ""
    property string subtitle: ""
    property string icon: ""
    property bool open: false
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 14

    ListSection {
        ListRow {
            icon: fold.icon
            iconTint: Theme.textMuted
            title: fold.title
            subtitle: fold.subtitle
            clickable: true
            showChevron: false
            onClicked: fold.open = !fold.open
            Icon {
                glyph: fold.open ? Icons.chevronUp : Icons.chevronDown
                size: 16
                color: Theme.textMuted
            }
        }
    }

    ColumnLayout {
        id: body
        visible: fold.open
        Layout.fillWidth: true
        spacing: 16
    }
}
