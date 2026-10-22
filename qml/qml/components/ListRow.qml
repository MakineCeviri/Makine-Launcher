// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ListRow — one row of a ListSection: optional icon tile, a title with a
 * line of explanation under it, and whatever control belongs on the right
 * (switch, button, value). `clickable` makes the whole row the target and
 * adds the chevron of a row that leads somewhere.
 */
Item {
    id: row

    property string title: ""
    property string subtitle: ""
    property string icon: ""
    property color iconTint: Theme.primary
    property bool clickable: false
    property bool showChevron: clickable
    property bool destructive: false
    property int minHeight: row.subtitle !== "" ? 58 : 46
    default property alias trailing: trailingRow.data

    signal clicked()

    // Set by ListSection
    readonly property bool _isListRow: true
    property bool _isFirst: true
    property bool _isLast: true
    property bool _watched: false

    Layout.fillWidth: true
    implicitHeight: Math.max(row.minHeight, body.implicitHeight + 22)

    activeFocusOnTab: row.clickable
    Accessible.role: row.clickable ? Accessible.Button : Accessible.ListItem
    Accessible.name: row.title
    Accessible.description: row.subtitle
    Keys.onReturnPressed: if (row.clickable) row.clicked()
    Keys.onSpacePressed: if (row.clickable) row.clicked()

    readonly property int _inset: row.icon !== "" ? 16 + 28 + 12 : 16

    // Hover / press wash, rounded where the row meets the card's corners
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        visible: row.clickable && (hover.hovered || row.activeFocus)
        color: tap.pressed ? Theme.fillTertiary : Theme.fillQuaternary
        topLeftRadius: row._isFirst ? Dimensions.radiusLG - 1 : 0
        topRightRadius: row._isFirst ? Dimensions.radiusLG - 1 : 0
        bottomLeftRadius: row._isLast ? Dimensions.radiusLG - 1 : 0
        bottomRightRadius: row._isLast ? Dimensions.radiusLG - 1 : 0
    }

    Rectangle {
        visible: !row._isFirst
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: row._inset
        height: 1
        color: Theme.separator
    }

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        spacing: 12

        IconTile {
            visible: row.icon !== ""
            Layout.alignment: Qt.AlignVCenter
            glyph: row.icon
            tint: row.destructive ? Theme.error : row.iconTint
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: row.title
                elide: Text.ElideRight
                font.pixelSize: Dimensions.body
                font.weight: Font.Medium
                color: row.destructive ? Theme.error : Theme.textPrimary
            }
            Text {
                visible: row.subtitle !== ""
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: row.subtitle
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                lineHeight: 1.12
                color: Theme.textMuted
            }
        }

        RowLayout {
            id: trailingRow
            Layout.alignment: Qt.AlignVCenter
            spacing: 8
        }

        Icon {
            visible: row.showChevron
            Layout.alignment: Qt.AlignVCenter
            glyph: Icons.chevronRight
            size: 15
            color: Theme.textTertiary
        }
    }

    HoverHandler {
        id: hover
        enabled: row.clickable
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        id: tap
        enabled: row.clickable
        onTapped: row.clicked()
    }
}
