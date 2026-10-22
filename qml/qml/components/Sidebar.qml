// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Sidebar — the launcher's source list, the way macOS apps navigate: a
 * floating panel with the app's name on top, its places below, help and
 * the community further down, and what the launcher is busy with (an
 * update, a scan) at the foot.
 *
 * Indexes match Main.qml: 0 Ana Sayfa · 1 Kütüphanem · 2 Ayarlar ·
 * 3 Tüm Oyunlar · 4 Nasıl çalışır?
 */
Card {
    id: sidebar

    property int currentIndex: 0
    property Window windowRef: null

    signal navigate(int index)

    radius: Dimensions.radiusSection
    glow: true
    glowCorner: "topLeft"
    glowStrength: 0.10

    // Games on this PC whose patch waits for a step (install or update):
    // the count Mail would show next to an inbox.
    readonly property int _pending: {
        let n = 0
        for (const g of GameService.library)
            if (g.status === "ready" || g.status === "update") ++n
        return n
    }

    // The top strip is the window's title bar: it drags the window.
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        DragHandler {
            target: null
            onActiveChanged: if (active && sidebar.windowRef) sidebar.windowRef.startSystemMove()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 12
        spacing: 0

        // App identity
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.bottomMargin: 18
            spacing: 10

            Image {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
                sourceSize: Qt.size(60, 60)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                mipmap: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text {
                    textFormat: Text.PlainText
                    text: "Makine Çeviri"
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.Bold
                    font.letterSpacing: Dimensions.trackingHeadline
                    color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Türkçe yama başlatıcısı")
                    font.pixelSize: Dimensions.caption1
                    color: Theme.textMuted
                }
            }
        }

        SidebarItem {
            glyph: Icons.house
            text: qsTr("Ana Sayfa")
            shortcutHint: "Ctrl+1"
            selected: sidebar.currentIndex === 0
            onClicked: sidebar.navigate(0)
        }
        SidebarItem {
            glyph: Icons.layoutGrid
            text: qsTr("Tüm Oyunlar")
            shortcutHint: "Ctrl+3"
            selected: sidebar.currentIndex === 3
            onClicked: sidebar.navigate(3)
        }
        SidebarItem {
            glyph: Icons.gamepad2
            text: qsTr("Kütüphanem")
            shortcutHint: "Ctrl+2"
            count: sidebar._pending
            selected: sidebar.currentIndex === 1
            onClicked: sidebar.navigate(1)
        }

        SidebarHeading { text: qsTr("Yardım") }

        SidebarItem {
            glyph: Icons.circleHelp
            text: qsTr("Nasıl çalışır?")
            shortcutHint: "F1"
            selected: sidebar.currentIndex === 4
            onClicked: sidebar.navigate(4)
        }
        SidebarItem {
            glyph: Icons.settings
            text: qsTr("Ayarlar")
            shortcutHint: "Ctrl+,"
            selected: sidebar.currentIndex === 2
            onClicked: sidebar.navigate(2)
        }

        SidebarHeading { text: qsTr("Topluluk") }

        SidebarItem {
            glyph: Icons.messagesSquare
            text: qsTr("Discord")
            external: true
            onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
        }
        SidebarItem {
            glyph: Icons.heart
            text: qsTr("Destek ol")
            external: true
            onClicked: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
        }

        Item { Layout.fillHeight: true }

        SidebarStatus {
            Layout.fillWidth: true
            onOpenSettings: sidebar.navigate(2)
        }
    }

    // ── Parts ────────────────────────────────────────────────────────────

    component SidebarHeading: Text {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.topMargin: 18
        Layout.bottomMargin: 4
        textFormat: Text.PlainText
        font.pixelSize: Dimensions.caption1
        font.weight: Font.DemiBold
        color: Theme.textMuted
    }

    component SidebarItem: Item {
        id: item
        property string glyph: ""
        property string text: ""
        property string shortcutHint: ""
        property bool selected: false
        property bool external: false
        property int count: 0
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 34

        activeFocusOnTab: true
        Accessible.role: item.external ? Accessible.Link : Accessible.PageTab
        Accessible.name: item.text
        Accessible.checked: item.selected
        Keys.onReturnPressed: item.clicked()
        Keys.onSpacePressed: item.clicked()

        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusMD
            color: item.selected ? Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.16 : 0.12)
                 : itemHover.hovered ? Theme.fillQuaternary : "transparent"
            border.width: item.selected && Theme.darkMode ? 1 : 0
            border.color: Theme.withAlpha(Theme.primaryText, 0.14)
            Behavior on color { ColorAnimation { duration: Dimensions.animVeryFast } }
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: Dimensions.radiusMD + 2
            color: "transparent"
            border.width: 2
            border.color: Theme.withAlpha(Theme.primary, 0.5)
            visible: item.activeFocus
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            Icon {
                Layout.preferredWidth: 20
                glyph: item.glyph
                size: 17
                color: item.selected ? Theme.primaryText
                     : itemHover.hovered ? Theme.textSecondary : Theme.textMuted
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: item.text
                elide: Text.ElideRight
                font.pixelSize: Dimensions.body
                font.weight: item.selected ? Font.DemiBold : Font.Medium
                color: item.selected || itemHover.hovered ? Theme.textPrimary : Theme.textSecondary
            }
            Text {
                visible: item.count > 0
                textFormat: Text.PlainText
                text: item.count
                font.pixelSize: Dimensions.footnote
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                color: Theme.textMuted
            }
            Icon {
                visible: item.external && itemHover.hovered
                glyph: Icons.arrowUpRight
                size: 13
                color: Theme.textMuted
            }
        }

        HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: item.clicked() }

        StyledToolTip {
            visible: itemHover.hovered && item.shortcutHint !== ""
            delay: 900
            text: item.text + "  ·  " + item.shortcutHint
        }
    }
}
