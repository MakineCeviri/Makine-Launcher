// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeSectionHeader — a section's title, one line on what it holds, and
 * its actions on the right (children go there).
 */
Item {
    id: header

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionRow.data

    implicitHeight: Math.max(textCol.implicitHeight, actionRow.implicitHeight)

    Column {
        id: textCol
        anchors.left: parent.left
        anchors.right: actionRow.left
        anchors.rightMargin: Dimensions.spacingXL
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            textFormat: Text.PlainText
            text: header.title
            elide: Text.ElideRight
            font.pixelSize: Dimensions.fontTitle
            font.weight: Font.DemiBold
            font.letterSpacing: -0.2
            color: Theme.textPrimary
            Accessible.role: Accessible.Heading
        }
        Text {
            width: parent.width
            visible: header.subtitle !== ""
            textFormat: Text.PlainText
            text: header.subtitle
            elide: Text.ElideRight
            font.pixelSize: Dimensions.fontBody
            color: Theme.textMuted
        }
    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Dimensions.spacingXS
    }
}
