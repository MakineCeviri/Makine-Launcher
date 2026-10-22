// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PageHeader — a page's large title, the line saying what the page is for,
 * and the page's own controls on the right (search, buttons).
 */
RowLayout {
    id: header

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionRow.data

    Layout.fillWidth: true
    spacing: Dimensions.spacingXL

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom
        spacing: 4

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: header.title
            elide: Text.ElideRight
            font.family: Dimensions.fontDisplay
            font.pixelSize: Dimensions.largeTitle
            font.weight: Font.Bold
            font.letterSpacing: Dimensions.trackingTitle
            color: Theme.textPrimary
            Accessible.role: Accessible.Heading
            Accessible.name: header.title
        }
        Text {
            visible: header.subtitle !== ""
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: header.subtitle
            wrapMode: Text.WordWrap
            font.pixelSize: Dimensions.body
            color: Theme.textMuted
        }
    }

    RowLayout {
        id: actionRow
        Layout.alignment: Qt.AlignBottom
        spacing: Dimensions.spacingMD
    }
}
