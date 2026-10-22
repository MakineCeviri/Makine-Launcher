// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PageHeader — a page's large title with the page's own controls on its
 * right (search, buttons), and the line saying what the page is for.
 */
ColumnLayout {
    id: header

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionRow.data

    Layout.fillWidth: true
    spacing: 6

    // The title and the page's controls on one line; the sentence under
    // them gets the whole width instead of wrapping beside a search box.
    RowLayout {
        Layout.fillWidth: true
        spacing: Dimensions.spacingXL

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
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

        RowLayout {
            id: actionRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Dimensions.spacingMD
        }
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
