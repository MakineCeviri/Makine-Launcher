// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * Library.qml - This PC's games with a Turkish patch, and what each needs next
 *
 * A shelf of the games themselves, the way a console's library looks: each
 * one a card of its Steam art with its name, where it was found, its state
 * and its one action — install, update, the patch's own site, its page — and
 * the card opens the game's page. The last tile adds a game the scan missed.
 * The segmented control filters; GameService.library orders the cards: what
 * needs a step first.
 */
Item {
    id: libraryRoot

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    // Install (or update) right away: the game's page opens and starts it.
    signal installRequested(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    // "Nasıl çalışır?": what the states and buttons here mean
    signal helpRequested()

    property string filter: "all"   // all | ready | update | installed

    readonly property var _lib: GameService.library
    readonly property int _ready: _lib.filter(g => g.status === "ready").length
    readonly property int _updates: _lib.filter(g => g.status === "update").length
    readonly property int _patched: _lib.filter(g => g.status === "installed" || g.status === "update").length
    readonly property int _soon: _lib.filter(g => g.status === "soon").length
    readonly property var _rows: libraryRoot.filter === "all" ? _lib
        : libraryRoot.filter === "installed"
          ? _lib.filter(g => g.status === "installed" || g.status === "update")
        : _lib.filter(g => g.status === libraryRoot.filter)

    // A filter whose rows are gone (the last update installed) falls back.
    on_RowsChanged: if (_rows.length === 0 && filter !== "all") filter = "all"

    readonly property var _filters: {
        const out = [ { value: "all", text: qsTr("Tümü"), count: _lib.length } ]
        if (_ready > 0 || filter === "ready") out.push({ value: "ready", text: qsTr("Kurulmayı bekliyor"), count: _ready })
        if (_updates > 0 || filter === "update") out.push({ value: "update", text: qsTr("Güncelleme var"), count: _updates })
        if (_patched > 0 || filter === "installed") out.push({ value: "installed", text: qsTr("Yama kurulu"), count: _patched })
        return out
    }

    function _shownPath(p) {
        return String(p || "").split("/").join(String.fromCharCode(92))
    }
    function _source(g) {
        switch (g.source) {
        case "steam": return "Steam"
        case "epic": return "Epic Games"
        case "gog": return "GOG"
        case "xbox": return "Xbox"
        case "ubisoft": return "Ubisoft Connect"
        case "manual": return qsTr("Elle eklendi")
        default: return qsTr("Otomatik bulundu")
        }
    }

    onVisibleChanged: if (visible) flickable.contentY = 0

    Flickable {
        id: flickable
        anchors.fill: parent
        contentHeight: contentColumn.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: flickable.parent
            anchors.top: flickable.top; anchors.right: flickable.right; anchors.bottom: flickable.bottom
        }

        ColumnLayout {
            id: contentColumn
            width: Math.min(flickable.width - Dimensions.pageMargin * 2, Dimensions.contentMaxWidth)
            x: (flickable.width - width) / 2
            spacing: 20

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Kütüphanem")
                subtitle: GameService.isScanning && libraryRoot._lib.length === 0
                          ? qsTr("Oyunların taranıyor…")
                          : libraryRoot._lib.length === 0
                            ? qsTr("Bilgisayarında Türkçe yaması olan bir oyun bulamadık.")
                            : (libraryRoot._lib.length > libraryRoot._soon
                               ? qsTr("Bilgisayarında Türkçe yaması olan %1 oyun var; %2 tanesinde yama kurulu.")
                                     .arg(libraryRoot._lib.length - libraryRoot._soon).arg(libraryRoot._patched)
                               : qsTr("Bilgisayarındaki oyunların Türkçe yaması henüz hazır değil."))
                              + (libraryRoot._soon > 0 ? " " + qsTr("%1 oyunun yaması yakında.").arg(libraryRoot._soon) : "")

                AppButton {
                    style: "gray"
                    icon: Icons.circleHelp
                    toolTip: qsTr("Nasıl çalışır? Buradaki durumlar ve düğmeler ne demek")
                    onClicked: libraryRoot.helpRequested()
                }
                AppButton {
                    style: "gray"
                    icon: Icons.refreshCw
                    busy: GameService.isScanning
                    text: GameService.isScanning ? qsTr("Taranıyor…") : qsTr("Yeniden tara")
                    toolTip: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine yeniden bak")
                    onClicked: GameService.scanAllLibraries()
                }
            }

            SegmentedControl {
                visible: libraryRoot._lib.length > 0 && libraryRoot._filters.length > 1
                model: libraryRoot._filters
                currentValue: libraryRoot.filter
                onActivated: (value) => libraryRoot.filter = value
            }

            // The games
            GridLayout {
                id: grid
                Layout.fillWidth: true
                Layout.topMargin: 4
                visible: libraryRoot._lib.length > 0
                columns: contentColumn.width >= 960 ? 3 : 2
                columnSpacing: 20
                rowSpacing: 20

                readonly property real _cardW: (contentColumn.width - columnSpacing * (columns - 1)) / columns
                readonly property real _cardH: Math.round(Math.max(200, Math.min(300, _cardW * 0.52)))

                Repeater {
                    model: libraryRoot._rows
                    delegate: GameCard {}
                }
            }

            // Under them, the way to add one the scan missed
            AddTile {
                visible: libraryRoot._lib.length > 0
                Layout.topMargin: 4
            }

            // Nothing yet: what to do about it
            Card {
                Layout.fillWidth: true
                visible: libraryRoot._lib.length === 0 && !GameService.isScanning
                implicitHeight: emptyCol.implicitHeight + 64

                ColumnLayout {
                    id: emptyCol
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 64, 520)
                    spacing: 12

                    IconTile {
                        Layout.alignment: Qt.AlignHCenter
                        size: 56
                        glyph: Icons.gamepad2
                        tint: Theme.primary
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        text: qsTr("Kütüphanende henüz oyun yok")
                        font.pixelSize: Dimensions.title3
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.2
                        font.pixelSize: Dimensions.callout
                        color: Theme.textMuted
                        text: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine baktık. Oyunun başka bir yerdeyse klasörünü ekle ya da pencereye sürükleyip bırak; hangi oyun olduğunu biz buluruz.")
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        style: "filled"
                        icon: Icons.folderOpen
                        text: qsTr("Oyun klasörünü seç")
                        onClicked: libraryRoot.manualFolderRequested()
                    }
                }
            }
        }
    }

    // ── Parts ────────────────────────────────────────────────────────────

    // A game: its art, its name and where it lives, its state, its one action
    component GameCard: Item {
        id: card
        required property var modelData
        required property int index

        readonly property string status: card.modelData.status || "ready"
        readonly property color tone: card.status === "update" ? Theme.warning
                                    : card.status === "installed" ? Theme.success
                                    : card.status === "soon" ? Theme.textMuted
                                    : Theme.primary
        readonly property string appId: card.modelData.steamAppId || ""
        readonly property bool _lifted: cardHover.hovered || card.activeFocus

        function open() {
            libraryRoot.gameSelected(card.modelData.id, card.modelData.name || "",
                                     card.modelData.installPath || "", card.modelData.engine || "")
        }
        function act() {
            if (card.status === "installed" || card.status === "soon")
                card.open()
            else
                libraryRoot.installRequested(card.modelData.id, card.modelData.name || "",
                                             card.modelData.installPath || "", card.modelData.engine || "")
        }

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: grid._cardH

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: card.modelData.name || ""
        Keys.onReturnPressed: card.open()
        Keys.onSpacePressed: card.open()

        // The cover from the image cache, for a game without Steam art
        property string _cover: ""
        function _resolveCover() {
            const id = card.appId || card.modelData.id || ""
            card._cover = id ? ImageCache.resolve(id) : ""
        }
        readonly property bool fallback: card.appId === "" || shot.status === Image.Error
        onFallbackChanged: if (fallback) _resolveCover()
        Component.onCompleted: {
            if (fallback) _resolveCover()
            _arrive()
        }
        // Fixed target, gated by `enabled` (NATIVE-74).
        Connections {
            target: ImageCache
            enabled: card.fallback && card._cover === ""
            function onImageReady(readyId) {
                if (readyId === (card.appId || card.modelData.id)) card._resolveCover()
            }
        }

        // Rises into place when it arrives, and a little under the pointer
        transform: [
            Translate { id: rise; y: 0 },
            Translate {
                y: card._lifted ? -4 : 0
                Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            }
        ]

        RectangularShadow {
            anchors.fill: art
            radius: Dimensions.radiusSection
            offset.y: card._lifted ? 14 : 8
            blur: card._lifted ? 36 : 26
            spread: -6
            color: Qt.rgba(0, 0, 0, Theme.darkMode ? (card._lifted ? 0.6 : 0.45) : 0.18)
            Behavior on offset.y { NumberAnimation { duration: Dimensions.animNormal } }
        }

        // ── Art (rounded by a mask) ──
        Item {
            id: art
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: artMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }

            Rectangle { anchors.fill: parent; color: Theme.surface }

            Image {
                id: shot
                anchors.fill: parent
                source: card.appId !== "" ? GameService.steamHeroUrl(card.appId) : ""
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 1000
                asynchronous: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow } }
                // A slow push in while the card is lifted
                scale: card._lifted ? 1.05 : 1
                Behavior on scale { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }
            }

            // No Steam art: the cover blurred into a backdrop, the cover beside it
            Image {
                id: backdrop
                anchors.fill: parent
                visible: false
                source: card.fallback ? card._cover : ""
                fillMode: Image.PreserveAspectCrop
                sourceSize: Qt.size(260, 370)
                asynchronous: true
            }
            MultiEffect {
                anchors.fill: parent
                visible: card.fallback && backdrop.status === Image.Ready
                source: backdrop
                blurEnabled: true
                blurMax: 64
                blur: 1.0
                brightness: -0.2
                saturation: 0.2
            }
            Image {
                visible: card.fallback
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height - 44
                width: height * 130 / 185
                source: card._cover
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                mipmap: true
            }

            // Legibility: the state on a dark head, the name on a dark foot
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.35) }
                    GradientStop { position: 0.28; color: Qt.rgba(0, 0, 0, 0.0) }
                    GradientStop { position: 0.5; color: Qt.rgba(0, 0, 0, 0.08) }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.86) }
                }
            }
        }
        Item {
            id: artMask
            anchors.fill: art
            visible: false
            layer.enabled: true
            Rectangle { anchors.fill: parent; radius: Dimensions.radiusSection; color: "black" }
        }

        // Hairline, lit along the top
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusSection
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, card._lifted ? 0.2 : 0.1)
            Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
        }
        Rectangle {
            x: Dimensions.radiusSection
            width: parent.width - Dimensions.radiusSection * 2
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Qt.rgba(1, 1, 1, 0.24) }
                GradientStop { position: 0.7; color: Qt.rgba(1, 1, 1, 0.24) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Focus ring
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Dimensions.radiusSection + 3
            color: "transparent"
            border.width: 3
            border.color: Theme.withAlpha(Theme.primary, 0.5)
            visible: card.activeFocus
        }

        // The state, on the art's head
        StatusBadge {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 14
            onArt: true
            tint: card.tone
            text: card.status === "update" ? qsTr("Güncelleme var")
                : card.status === "installed" ? qsTr("Yama kurulu")
                : card.status === "soon" ? qsTr("Yakında")
                : qsTr("Yama hazır, kurulmadı")
        }

        // The name, where it was found, and the one action
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 18
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: card.modelData.name || ""
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: Dimensions.title3
                    font.weight: Font.Bold
                    font.letterSpacing: Dimensions.trackingHeadline
                    color: "#FFFFFF"
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: libraryRoot._source(card.modelData) + "  ·  " + libraryRoot._shownPath(card.modelData.installPath)
                    elide: Text.ElideMiddle
                    font.pixelSize: Dimensions.footnote
                    font.family: Dimensions.fontText
                    color: Qt.rgba(1, 1, 1, 0.62)
                }
            }
            AppButton {
                Layout.alignment: Qt.AlignBottom
                style: card.status === "ready" || card.status === "update" ? "filled" : "glass"
                tint: card.status === "update" ? Theme.warning : Theme.primary
                icon: card.status === "ready" ? Icons.download
                    : card.status === "update" ? Icons.refreshCw
                    : card.status === "soon" ? Icons.clock : ""
                trailingIcon: card.status === "installed" ? Icons.chevronRight : ""
                text: card.status === "ready" ? qsTr("Türkçe Yamayı Kur")
                    : card.status === "update" ? qsTr("Güncelle")
                    : qsTr("Sayfasını aç")
                onClicked: card.act()
            }
        }

        HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: card.open() }

        // Cards rise into place one after another
        opacity: 0
        function _arrive() {
            if (Dimensions.animSlow === 0) { card.opacity = 1; return }
            arrive.restart()
        }
        SequentialAnimation {
            id: arrive
            PauseAnimation { duration: Math.min(card.index, 8) * 60 }
            ParallelAnimation {
                NumberAnimation { target: card; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
                NumberAnimation { target: rise; property: "y"; from: 18; to: 0; duration: 560; easing.type: Easing.OutExpo }
            }
        }
    }

    // A dashed drop zone: a game the scan did not find
    component AddTile: Item {
        id: add
        Layout.fillWidth: true
        implicitHeight: 84

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Oyun ekle")
        Keys.onReturnPressed: libraryRoot.manualFolderRequested()
        Keys.onSpacePressed: libraryRoot.manualFolderRequested()

        readonly property bool _hot: addHover.hovered || add.activeFocus

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: add._hot ? Theme.withAlpha(Theme.primary, 0.75) : Theme.separatorStrong
                strokeWidth: 1.5
                strokeStyle: ShapePath.DashLine
                dashPattern: [4, 3]
                fillColor: add._hot ? Theme.withAlpha(Theme.primary, 0.07) : Theme.withAlpha(Theme.textPrimary, 0.02)
                PathRectangle {
                    x: 0.75; y: 0.75
                    width: add.width - 1.5
                    height: add.height - 1.5
                    radius: Dimensions.radiusSection
                }
            }
        }

        RowLayout {
            anchors.centerIn: parent
            spacing: 14

            IconTile {
                size: 38
                glyph: Icons.plus
                tint: Theme.primary
                scale: add._hot ? 1.08 : 1
                Behavior on scale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }
            }
            ColumnLayout {
                spacing: 2
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Oyunun listede yok mu?")
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Klasörünü seçmek için tıkla ya da pencereye sürükleyip bırak.")
                    font.pixelSize: Dimensions.footnote
                    font.family: Dimensions.fontText
                    color: Theme.textMuted
                }
            }
        }

        HoverHandler { id: addHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: libraryRoot.manualFolderRequested() }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: flickable
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
