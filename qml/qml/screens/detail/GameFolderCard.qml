// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameFolderCard — where the game is, and a way to say where it is.
 *
 * Found: the folder (its real location, not the MSIX view), who found it, and
 * "Klasörü aç" / "Değiştir". Not found: why there is no install button, and
 * one button to point at the game. Pointing works with anything around the
 * game — its folder, the folder the .exe sits in, the exe, the library that
 * holds it, dropped onto the card or picked — GameService.inspectGameFolder
 * finds the root the patch needs and GameFolderDialog shows the evidence
 * before anything is bound.
 */
Rectangle {
    id: card

    required property var vm

    property var _folder: ({})
    property bool _checking: false
    // The game's own folder looks to be inside the one we know (an archive
    // unpacked into a folder of its own name) — found by a quiet look when
    // the page opens; null when all is well.
    property var _suggestion: null
    property string _auditedPath: ""
    // Set by "Yamayı kaldır ve klasörü düzelt": bound once the uninstall ends.
    property var _moveAfterUninstall: null
    readonly property bool _located: (card._folder.path || "") !== ""
    readonly property bool _missing: card._located && card._folder.exists === false
    // A catalogue game with a patch the launcher can install, that no scan found.
    readonly property bool _notFound: !card._located && card.vm.hasTranslation
                                      && !card.vm.isGameInstalled && card.vm.externalUrl === ""

    function _refresh() {
        card._folder = card.vm.gameId !== "" ? GameService.gameFolderInfo(card.vm.gameId) : ({})
        const path = card._folder.path || ""
        if (path !== card._auditedPath) {
            card._auditedPath = path
            card._suggestion = null
            if (path !== "" && card._folder.exists)
                GameService.inspectGameFolder(card.vm.gameId, path, true)
        }
    }
    function _isInside(child, parentPath) {
        const c = String(child).toLowerCase(), p = String(parentPath).toLowerCase()
        return p !== "" && c.startsWith(p + "/")
    }
    function _inspect(url) {
        card._checking = true
        GameService.inspectGameFolder(card.vm.gameId, url.toString())
    }
    function _pick() {
        if (card._located)
            folderPicker.currentFolder = "file:///" + String(card._folder.path)
        folderPicker.open()
    }

    component CardButton: Rectangle {
        id: cb
        property string text: ""
        property bool primary: false
        signal clicked()
        width: cbLabel.implicitWidth + Dimensions.paddingLG * 1.5
        height: 34
        radius: Dimensions.radiusStandard
        color: cb.primary ? (cbHover.hovered ? Theme.primaryHover : Theme.primary)
                          : (cbHover.hovered ? Theme.textPrimary12 : Theme.textPrimary06)
        Accessible.role: Accessible.Button
        Accessible.name: cb.text
        activeFocusOnTab: true
        Keys.onReturnPressed: cb.clicked()
        Keys.onSpacePressed: cb.clicked()

        Label {
            id: cbLabel
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: cb.text
            font.pixelSize: Dimensions.fontSM
            font.weight: Font.DemiBold
            color: cb.primary ? Theme.textOnColor : Theme.textPrimary
        }
        HoverHandler { id: cbHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: cb.clicked() }
    }

    Connections {
        target: card.vm
        function onGameIdChanged() { card._refresh() }
        function onIsGameInstalledChanged() { card._refresh() }
    }
    Connections {
        target: GameService
        function onGameFolderChanged(gameId) { if (gameId === card.vm.gameId) card._refresh() }
        function onGameListChanged() { card._refresh() }
        function onTranslationUninstalled(gameId, success, message) {
            const move = card._moveAfterUninstall
            if (gameId !== card.vm.gameId || !move) return
            card._moveAfterUninstall = null
            if (success && GameService.setGameFolder(gameId, move.root, move.engine))
                card.vm.isGameInstalled = true
        }
        function onGameFolderInspected(gameId, result) {
            if (gameId !== card.vm.gameId) return
            if (result.audit) {
                card._suggestion = (result.root && !result.same
                                    && card._isInside(result.root, card._folder.path || ""))
                                   ? result : null
                return
            }
            card._checking = false
            verdictLoader.result = result
            verdictLoader.active = true
        }
    }
    Component.onCompleted: _refresh()

    visible: card.vm.hasTranslation && (card._located || card._notFound)
    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + Dimensions.paddingLG * 2
    radius: Dimensions.radiusMD
    color: dropArea.containsDrag ? Theme.primary08
         : card._notFound || card._missing ? Theme.textPrimary04 : "transparent"
    border.width: 1
    border.color: dropArea.containsDrag ? Theme.primary
                : card._missing ? Theme.withAlpha(Theme.warning, 0.35) : Theme.textPrimary08

    // Drop a folder (or the game's exe) from Explorer onto the card.
    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                card._inspect(drop.urls[0])
                drop.accept(Qt.CopyAction)
            }
        }
    }

    FolderDialog {
        id: folderPicker
        title: qsTr("%1 klasörünü seç").arg(card.vm.gameName)
        onAccepted: card._inspect(selectedFolder)
    }

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Dimensions.paddingLG
        anchors.rightMargin: Dimensions.paddingLG
        spacing: Dimensions.spacingLG

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: Dimensions.radiusMD
            color: card._missing ? Theme.withAlpha(Theme.warning, 0.12) : Theme.textPrimary06

            Label {
                anchors.centerIn: parent
                textFormat: Text.PlainText
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 16
                text: card._missing ? "\uE7BA" : "\uE838"
                color: card._missing ? Theme.warning : Theme.textSecondary
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Label {
                textFormat: Text.PlainText
                text: card._notFound ? qsTr("OYUN BU BİLGİSAYARDA BULUNAMADI")
                    : card._missing ? qsTr("OYUN KLASÖRÜ ARTIK YOK")
                    : qsTr("OYUN KLASÖRÜ")
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
                color: card._missing ? Theme.warning : Theme.textMuted
            }

            // Found: the folder itself, selectable for a support message.
            TextEdit {
                Layout.fillWidth: true
                visible: card._located
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.WrapAnywhere
                text: card._folder.shown || ""
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
                text: card._checking ? qsTr("Klasöre bakılıyor…")
                    : card._notFound
                      ? qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine baktık. Oyun başka bir yerdeyse klasörünü göster: oyunun klasörü, .exe'nin olduğu klasör ya da oyunların durduğu klasör olabilir — yamanın gideceği doğru yeri biz buluruz. Klasörü buraya sürükleyip bırakabilirsin de.")
                    : card._missing
                      ? qsTr("Oyun taşınmış ya da kaldırılmış olabilir. Yeni yerini göster, yama oraya kurulsun.")
                    : card._folder.chosen
                      ? qsTr("Senin seçtiğin klasör.")
                    : card._folder.source === "steam" ? qsTr("Steam kütüphanende bulundu.")
                    : card._folder.source === "epic" ? qsTr("Epic Games kütüphanende bulundu.")
                    : card._folder.source === "gog" ? qsTr("GOG kütüphanende bulundu.")
                    : card._folder.source === "xbox" ? qsTr("Xbox kütüphanende bulundu.")
                    : qsTr("Otomatik bulundu.")
            }

            // The quiet look found the game one level in.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Dimensions.spacingSM
                visible: card._suggestion !== null
                spacing: Dimensions.spacingSM

                Label {
                    Layout.alignment: Qt.AlignTop
                    textFormat: Text.PlainText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 13
                    text: "\uE7BA"
                    color: Theme.warning
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.warning
                    text: card._suggestion
                          ? qsTr("Oyunun kendisi bir alt klasörde görünüyor: %1. Yama bu klasöre değil oraya kurulmalı; yoksa oyun yamayı görmez.")
                                .arg(card._suggestion.rootShown)
                          : ""
                }
            }
        }

        // Actions
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Dimensions.spacingSM

            CardButton {
                visible: card._suggestion !== null
                primary: true
                text: qsTr("Düzelt")
                onClicked: {
                    verdictLoader.result = card._suggestion
                    verdictLoader.active = true
                }
            }
            CardButton {
                visible: card._located && !card._missing
                text: qsTr("Klasörü aç")
                onClicked: GameService.openGameFolder(card.vm.gameId)
            }
            CardButton {
                visible: card._folder.resettable === true
                text: qsTr("Otomatik bulunana dön")
                onClicked: GameService.resetGameFolder(card.vm.gameId)
            }
            CardButton {
                primary: card._notFound || card._missing
                text: card._notFound ? qsTr("Oyun klasörünü seç")
                    : card._missing ? qsTr("Yeni yerini göster")
                    : qsTr("Değiştir")
                onClicked: card._pick()
            }
        }
    }

    // ===== What the picked folder turned out to be =====
    Loader {
        id: verdictLoader
        property var result: ({})
        active: false
        sourceComponent: Component {
            GameFolderDialog {
                parent: Overlay.overlay
                report: verdictLoader.result
                gameName: card.vm.gameName
                onUseFolder: (root, engine) => {
                    if (GameService.setGameFolder(card.vm.gameId, root, engine))
                        card.vm.isGameInstalled = true
                }
                onPickAnother: Qt.callLater(card._pick)
                onUninstallAndUse: (root, engine) => {
                    card._moveAfterUninstall = { root: root, engine: engine }
                    GameService.uninstallTranslation(card.vm.gameId)
                }
                onClosed: verdictLoader.active = false
                Component.onCompleted: open()
            }
        }
    }
}
