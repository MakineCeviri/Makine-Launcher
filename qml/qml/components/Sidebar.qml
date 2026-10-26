// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Sidebar — the launcher's source list, the way macOS apps navigate: the
 * window's frame beside the page panel, the app's name on top, the game
 * search, its places below, help and the
 * community further down — each group under a hairline — and what the
 * launcher is busy with (an update, a scan) at the foot.
 *
 * Indexes match Main.qml: 0 Ana Sayfa · 1 Kütüphanem · 2 Ayarlar ·
 * 3 Tüm Oyunlar · 4 Nasıl çalışır? · 5 Yenilikler · 6 Topluluk
 */
Card {
    id: sidebar

    property int currentIndex: 0
    property Window windowRef: null

    signal navigate(int index)
    // The search searches the catalogue: Main.qml shows "Tüm Oyunlar" with it
    signal searchEdited(string text)
    signal searchAccepted()

    function focusSearch() { search.focusField() }
    function setSearch(text) { if (search.text !== text) search.text = text }

    // Straight on the window's frame (Theme.chrome); the page panel beside it
    // is the lit surface (Main.qml)
    color: "transparent"
    hairline: false
    lit: false
    elevated: false

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

    // The selection: one accent pill that slides to the chosen place (the
    // way Apex and macOS move a selection), under the items
    property Item _selected: null
    Rectangle {
        id: selection
        readonly property Item target: sidebar._selected
        visible: target !== null
        x: column.x + (target ? target.x : 0)
        y: column.y + (target ? target.y : 0)
        width: target ? target.width : 0
        height: target ? target.height : 0
        radius: Dimensions.radiusMD
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.tint(Theme.primary, Qt.rgba(1, 1, 1, 0.10)) }
            GradientStop { position: 1.0; color: Theme.primary }
        }
        Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutExpo } }
        Behavior on height { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    ColumnLayout {
        id: column
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
            Layout.bottomMargin: 16
            spacing: 10

            AppLogo { size: 30 }
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
                    font.family: Dimensions.fontText
                    color: Theme.textMuted
                }
            }
        }

        SearchField {
            id: search
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            placeholder: qsTr("Oyun ara")
            // Games to try: this PC's first, then some the catalogue has
            suggestions: {
                const mine = GameService.library.map(g => g.name).filter(n => !!n)
                const known = ["Red Dead Redemption 2", "Assassin's Creed Odyssey", "Alan Wake 2",
                               "A Plague Tale: Requiem", "Sekiro: Shadows Die Twice", "Heavy Rain",
                               "Alien: Isolation", "Mad Max", "Deathloop"]
                return mine.concat(known.filter(n => mine.indexOf(n) < 0))
            }
            onTextChanged: sidebar.searchEdited(text)
            onAccepted: sidebar.searchAccepted()
            Keys.onDownPressed: sidebar.searchAccepted()
        }

        SidebarItem {
            glyph: Icons.house
            iconName: "house"
            text: qsTr("Ana Sayfa")
            shortcutHint: "Ctrl+1"
            selected: sidebar.currentIndex === 0
            onClicked: sidebar.navigate(0)
        }
        SidebarItem {
            glyph: Icons.layoutGrid
            iconName: "grid"
            text: qsTr("Tüm Oyunlar")
            shortcutHint: "Ctrl+3"
            selected: sidebar.currentIndex === 3
            onClicked: sidebar.navigate(3)
        }
        SidebarItem {
            glyph: Icons.gamepad2
            iconName: "gamepad"
            text: qsTr("Kütüphanem")
            shortcutHint: "Ctrl+2"
            count: sidebar._pending
            selected: sidebar.currentIndex === 1
            onClicked: sidebar.navigate(1)
        }

        SidebarHeading { text: qsTr("Yardım") }

        SidebarItem {
            glyph: Icons.circleHelp
            iconName: "help"
            text: qsTr("Nasıl çalışır?")
            shortcutHint: "F1"
            selected: sidebar.currentIndex === 4
            onClicked: sidebar.navigate(4)
        }
        SidebarItem {
            glyph: Icons.sparkles
            iconName: "sparkles"
            text: qsTr("Yenilikler")
            // A newer launcher is out, or this one's notes are not read yet
            dot: UpdateService.state === UpdateService.Available || !UpdateService.notesSeen
            selected: sidebar.currentIndex === 5
            onClicked: sidebar.navigate(5)
        }
        SidebarItem {
            glyph: Icons.settings
            iconName: "settings"
            text: qsTr("Ayarlar")
            shortcutHint: "Ctrl+,"
            selected: sidebar.currentIndex === 2
            onClicked: sidebar.navigate(2)
        }

        SidebarHeading { text: qsTr("Topluluk") }

        SidebarItem {
            glyph: Icons.users
            iconName: "users"
            text: qsTr("Topluluk")
            selected: sidebar.currentIndex === 6
            onClicked: sidebar.navigate(6)
        }
        SidebarItem {
            glyph: Icons.discord
            text: qsTr("Discord")
            external: true
            onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
        }
        SidebarItem {
            glyph: Icons.youtube
            text: qsTr("YouTube")
            external: true
            onClicked: Qt.openUrlExternally(Dimensions.youtubeUrl)
        }
        SidebarItem {
            glyph: Icons.heart
            iconName: "heart"
            text: qsTr("Destek ol")
            external: true
            onClicked: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
        }

        Item { Layout.fillHeight: true }

        SidebarStatus {
            Layout.fillWidth: true
            onOpenSettings: sidebar.navigate(2)
            onOpenWhatsNew: sidebar.navigate(5)
        }
    }

    // ── Parts ────────────────────────────────────────────────────────────

    // A group's name under the lit hairline that ends the group before it
    component SidebarHeading: ColumnLayout {
        property alias text: label.text
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 4
        spacing: 12

        FadeLine { lit: true }
        Text {
            id: label
            Layout.fillWidth: true
            Layout.leftMargin: 10
            textFormat: Text.PlainText
            font.pixelSize: Dimensions.caption1
            font.family: Dimensions.fontText
            font.weight: Font.DemiBold
            color: Theme.textMuted
        }
    }

    component SidebarItem: Item {
        id: item
        property string glyph: ""
        // NavIcon's drawing ("" moves the glyph as one)
        property string iconName: ""
        property string text: ""
        property string shortcutHint: ""
        property bool selected: false
        property bool external: false
        property int count: 0
        property bool dot: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 34

        activeFocusOnTab: true
        Accessible.role: item.external ? Accessible.Link : Accessible.PageTab
        Accessible.name: item.text
        Accessible.checked: item.selected
        Keys.onReturnPressed: item.clicked()
        Keys.onSpacePressed: item.clicked()

        // Selected: the sidebar's sliding pill is under it (white on the
        // accent); unselected, a faint fill under the pointer
        onSelectedChanged: if (selected) {
            sidebar._selected = item
            // Chosen from the keyboard: the icon answers as it does to the pointer
            if (!itemHover.hovered) navIcon.play()
        }
        Component.onCompleted: if (selected) sidebar._selected = item
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusMD
            color: !item.selected && itemHover.hovered ? Theme.fillQuaternary : "transparent"
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

            // Comes alive under the pointer (Resend's sidebar does the same)
            Item {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                NavIcon {
                    id: navIcon
                    anchors.centerIn: parent
                    name: item.iconName
                    glyph: item.glyph
                    size: 17
                    color: item.selected ? "#FFFFFF"
                         : itemHover.hovered ? Theme.textSecondary : Theme.primaryText
                }
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: item.text
                elide: Text.ElideRight
                font.pixelSize: Dimensions.body
                font.weight: item.selected ? Font.DemiBold : Font.Medium
                color: item.selected ? "#FFFFFF"
                     : itemHover.hovered ? Theme.textPrimary : Theme.textSecondary
            }
            Rectangle {
                visible: item.dot
                implicitWidth: 7
                implicitHeight: 7
                radius: 3.5
                color: item.selected ? "#FFFFFF" : Theme.primary
            }
            // A count in a capsule, as Mail and the App Store show one
            Rectangle {
                visible: item.count > 0
                implicitWidth: Math.max(implicitHeight, countText.implicitWidth + 12)
                implicitHeight: 18
                radius: 9
                color: item.selected ? Qt.rgba(1, 1, 1, 0.24) : Theme.primary
                Text {
                    id: countText
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: item.count
                    font.pixelSize: Dimensions.caption1
                    font.family: Dimensions.fontText
                    font.weight: Font.Bold
                    font.features: { "tnum": 1 }
                    color: "#FFFFFF"
                }
            }
            Icon {
                visible: item.external && itemHover.hovered
                glyph: Icons.arrowUpRight
                size: 13
                color: Theme.textMuted
            }
        }

        HoverHandler {
            id: itemHover
            cursorShape: Qt.PointingHandCursor
            onHoveredChanged: if (hovered) navIcon.play()
        }
        TapHandler { onTapped: item.clicked() }

        StyledToolTip {
            visible: itemHover.hovered && item.shortcutHint !== ""
            delay: 900
            text: item.text + "  ·  " + item.shortcutHint
        }
    }
}
