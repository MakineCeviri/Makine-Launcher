// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * GameFolderBar — where the game is, and the one way to say where it is.
 *
 * A quiet line under the install button: the folder (its real location, not
 * the MSIX view), who found it, "Aç" / "Değiştir". It turns into a notice
 * only when the user is needed: the folder is gone, or the game turns out to
 * sit somewhere other than the folder the launcher knows — a repack unpacked
 * into "<Game>/<Game>", where a patch installed one level up never loads.
 *
 * It also owns the pick. The install button of a game no scan found, a folder
 * dropped on the page and the notice buttons all end in inspect():
 * GameService finds the root the patch needs from wherever the user pointed.
 * A folder that checks out is bound at once — and when the user was
 * installing, the install goes on; only a doubtful or refused folder, or a
 * patch that has to move, stops at GameFolderDialog.
 */
Item {
    id: bar

    required property var vm

    // The bound folder is ready for the install the user asked for.
    signal installRequested()

    property var _folder: ({})
    property bool _checking: false
    // install: the pick came from "Seç ve Kur" · change: anything else
    property string _intent: "change"
    // Where the quiet look on opening says the game really is; null when all is well.
    property var _suggestion: null
    property string _auditedPath: ""
    // {root, engine}: bound and reinstalled once the uninstall in progress ends.
    property var _moveAfterUninstall: null
    property string _note: ""

    readonly property bool _located: (bar._folder.path || "") !== ""
    readonly property bool _missing: bar._located && bar._folder.exists === false
    readonly property bool _notFound: !bar._located && bar.vm.hasTranslation
                                      && !bar.vm.isGameInstalled && !bar.vm.comingSoon
    readonly property bool _notice: bar._missing || bar._suggestion !== null
    // Where the game is, said from the folder we know.
    readonly property string _where: bar._suggestion === null ? ""
        : bar._isInside(bar._suggestion.root, bar._folder.path || "") ? qsTr("bir alt klasörde")
        : bar._isInside(bar._folder.path || "", bar._suggestion.root) ? qsTr("bir üst klasörde")
        : qsTr("başka bir klasörde")
    readonly property string _source: bar._folder.chosen ? qsTr("Senin seçtiğin klasör")
        : bar._folder.source === "steam" ? qsTr("Steam kütüphanesi")
        : bar._folder.source === "epic" ? qsTr("Epic Games kütüphanesi")
        : bar._folder.source === "gog" ? qsTr("GOG kütüphanesi")
        : bar._folder.source === "xbox" ? qsTr("Xbox kütüphanesi")
        : bar._folder.source === "ubisoft" ? qsTr("Ubisoft Connect kütüphanesi")
        : qsTr("Otomatik bulundu")

    // ── Entry points ─────────────────────────────────────────────────────────
    function pick(intent) {
        bar._intent = intent || "change"
        if (bar._located && !bar._missing)
            folderPicker.currentFolder = "file:///" + String(bar._folder.path)
        folderPicker.open()
    }
    function inspect(url, intent) {
        bar._intent = intent || "change"
        bar._note = ""
        bar._checking = true
        bar.vm.folderChecking = true
        GameService.inspectGameFolder(bar.vm.gameId, url.toString())
    }

    // `again`: look at the same folder again (the patch's data has arrived).
    function _refresh(again) {
        bar._folder = bar.vm.gameId !== "" ? GameService.gameFolderInfo(bar.vm.gameId) : ({})
        const path = bar._folder.path || ""
        if (path === bar._auditedPath && !again) return
        if (path !== bar._auditedPath) bar._suggestion = null
        bar._auditedPath = path
        if (path !== "" && bar._folder.exists && !bar.vm.comingSoon)
            GameService.inspectGameFolder(bar.vm.gameId, path, true)
    }
    function _isInside(child, parentPath) {
        const c = String(child).toLowerCase(), p = String(parentPath).toLowerCase()
        return p !== "" && c.startsWith(p + "/")
    }
    function _bind(root, engine, install) {
        if (!GameService.setGameFolder(bar.vm.gameId, root, engine)) return
        bar.vm.isGameInstalled = true
        if (install && !bar.vm.packageInstalled)
            bar.installRequested()
        else
            bar._flash(qsTr("Oyun klasörü kaydedildi"))
    }
    function _flash(text) {
        bar._note = text
        noteTimer.restart()
    }
    // The notice's button: a patch in the wrong folder has to come off first,
    // which the dialog spells out; with nothing installed the folder just moves.
    function _fix() {
        const s = bar._suggestion
        if (!s) return
        if (s.patchedElsewhere || bar.vm.packageInstalled)
            verdictLoader.show(s, "move")
        else
            bar._bind(s.root, "", false)
    }
    function _move(root, engine) {
        bar._moveAfterUninstall = { root: root, engine: engine }
        bar._note = qsTr("Yama kaldırılıyor; ardından doğru klasöre kurulacak…")
        GameService.uninstallTranslation(bar.vm.gameId)
    }

    Timer {
        id: noteTimer
        interval: 4000
        onTriggered: bar._note = ""
    }

    Connections {
        target: bar.vm
        function onGameIdChanged() {
            bar._moveAfterUninstall = null
            bar._checking = false
            bar._note = ""
            bar._refresh()
        }
        function onIsGameInstalledChanged() { bar._refresh() }
        // Where the patch is decides what the look says about the folder.
        function onPackageInstalledChanged() { bar._refresh(true) }
    }
    // The recipe and the exe names the look is weighed against arrive with the
    // game's detail, often after the page has opened: look again then.
    Connections {
        target: CoreBridge
        function onPackageDetailEnriched(appId) {
            if (appId === bar.vm.gameId || appId === bar.vm.steamAppId)
                bar._refresh(true)
        }
    }
    Connections {
        target: GameService
        function onGameFolderChanged(gameId) { if (gameId === bar.vm.gameId) bar._refresh() }
        function onGameListChanged() { bar._refresh() }
        function onTranslationUninstalled(gameId, success, message) {
            const move = bar._moveAfterUninstall
            if (gameId !== bar.vm.gameId || !move) return
            bar._moveAfterUninstall = null
            bar._note = ""
            if (!success) return
            if (GameService.setGameFolder(gameId, move.root, move.engine)) {
                bar.vm.isGameInstalled = true
                // After the page has taken in the uninstall (packageInstalled).
                Qt.callLater(bar.installRequested)
            }
        }
        function onGameFolderInspected(gameId, result) {
            if (gameId !== bar.vm.gameId) return
            if (result.audit) {
                // Deeper is the structural case (nothing runnable where we
                // are); anywhere else needs the patch's own evidence.
                const root = result.root || ""
                const better = root !== "" && !result.same
                    && (result.verdict !== "refused" || result.patchedElsewhere)
                    && (bar._isInside(root, bar._folder.path || "")
                        || (result.exe || "") !== "" || (result.filesFound || 0) > 0)
                bar._suggestion = better ? result : null
                return
            }
            bar._checking = false
            bar.vm.folderChecking = false
            if (result.verdict === "ok" && !result.patchedElsewhere) {
                if (result.same) bar._flash(qsTr("Bu zaten oyunun klasörü"))
                else bar._bind(result.root, result.engine || "", bar._intent === "install")
                return
            }
            verdictLoader.show(result, bar._intent)
        }
    }
    Component.onCompleted: _refresh()

    // A patch we do not have yet ("Yakında") needs no folder.
    visible: bar.vm.hasTranslation && !bar.vm.comingSoon && (bar._located || bar._notFound)
    implicitHeight: bar._notFound ? hint.implicitHeight
                  : bar._notice ? notice.implicitHeight
                  : row.implicitHeight

    FolderDialog {
        id: folderPicker
        title: qsTr("%1 klasörünü seç").arg(bar.vm.gameName)
        onAccepted: bar.inspect(selectedFolder, bar._intent)
    }

    // ── Not found: what the button above will do ────────────────────────────
    RowLayout {
        id: hint
        visible: bar._notFound
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 10

        Icon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            glyph: Icons.folderSearch
            size: 16
            color: Theme.textMuted
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            lineHeight: 1.2
            font.pixelSize: Dimensions.callout
            color: Theme.textMuted
            text: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerinde bulamadık. Oyunun klasörünü ya da .exe dosyasının durduğu klasörü göster; yamanın gideceği doğru yeri biz buluruz. Klasörü bu sayfaya sürükleyip bırakabilirsin de.")
        }
    }

    // ── Located: the folder, quietly ────────────────────────────────────────
    Card {
        id: row
        visible: bar._located && !bar._notice
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: 60
        radius: Dimensions.radiusLG

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 10
            spacing: 12

            IconTile { size: 32; glyph: Icons.folder; tint: Theme.primary; solid: false }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    id: pathText
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: bar._folder.shown || ""
                    elide: Text.ElideMiddle
                    font.pixelSize: Dimensions.body
                    font.weight: Font.Medium
                    color: Theme.textPrimary

                    HoverHandler { id: pathHover }
                    StyledToolTip {
                        visible: pathHover.hovered && pathText.truncated
                        text: bar._folder.shown || ""
                    }
                }
                // Who found it — or what just happened to it.
                RowLayout {
                    spacing: 5
                    readonly property bool _done: bar._note !== "" && !bar._checking && bar._moveAfterUninstall === null
                    Icon {
                        visible: parent._done
                        glyph: Icons.check
                        size: 12
                        color: Theme.success
                    }
                    Spinner { visible: bar._checking; size: 12 }
                    Text {
                        textFormat: Text.PlainText
                        text: bar._checking ? qsTr("Klasöre bakılıyor…")
                            : bar._note !== "" ? bar._note
                            : qsTr("Oyunun klasörü · %1").arg(bar._source)
                        font.pixelSize: Dimensions.footnote
                        color: parent._done ? Theme.success : Theme.textMuted
                    }
                }
            }

            AppButton {
                size: "small"
                style: "gray"
                icon: Icons.folderOpen
                text: qsTr("Aç")
                toolTip: qsTr("Klasörü Dosya Gezgini'nde aç")
                onClicked: GameService.openGameFolder(bar.vm.gameId)
            }
            AppButton {
                visible: bar._folder.resettable === true
                size: "small"
                style: "gray"
                icon: Icons.rotateCcw
                text: qsTr("Otomatik bulunana dön")
                onClicked: GameService.resetGameFolder(bar.vm.gameId)
            }
            AppButton {
                icon: Icons.folderSearch
                size: "small"
                style: "gray"
                text: qsTr("Değiştir")
                toolTip: qsTr("Oyun başka bir klasördeyse onu seç")
                onClicked: bar.pick("change")
            }
        }
    }

    // ── Located, but the user is needed ─────────────────────────────────────
    Rectangle {
        id: notice
        visible: bar._located && bar._notice
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: noticeRow.implicitHeight + 32
        radius: Dimensions.radiusLG
        color: Theme.withAlpha(Theme.warning, Theme.darkMode ? 0.12 : 0.08)
        border.width: 1
        border.color: Theme.withAlpha(Theme.warning, 0.32)

        RowLayout {
            id: noticeRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            IconTile {
                Layout.alignment: Qt.AlignTop
                size: 34
                glyph: Icons.triangleAlert
                tint: Theme.warning
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    text: bar._missing ? qsTr("Oyun klasörü yerinde değil")
                        : bar.vm.packageInstalled ? qsTr("Yama oyunun klasörüne gitmemiş")
                        : qsTr("Oyunun asıl klasörü başka yerde")
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    lineHeight: 1.2
                    font.pixelSize: Dimensions.callout
                    color: Theme.textSecondary
                    text: bar._missing
                          ? qsTr("%1 artık yok; oyun taşınmış ya da kaldırılmış olabilir. Yeni yerini göster, yama oraya kurulsun.")
                                .arg(bar._folder.shown || "")
                          : bar._suggestion === null ? ""
                          : bar.vm.packageInstalled
                            ? qsTr("Yama %1 klasörüne kurulmuş, oyun ise %2. Oyun yamayı görmüyor; taşırsan yama kaldırılıp oyunun klasörüne yeniden kurulur.")
                                  .arg(bar._folder.shown || "").arg(bar._where)
                            : qsTr("Oyun %1: %2. Yama oraya kurulmalı, yoksa oyun onu görmez.")
                                  .arg(bar._where).arg(bar._suggestion.rootShown)
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 4

                AppButton {
                    icon: Icons.folderSearch
                    Layout.alignment: Qt.AlignRight
                    style: "filled"
                    tint: Theme.warning
                    enabled: bar._moveAfterUninstall === null
                    busy: bar._moveAfterUninstall !== null
                    text: bar._missing ? qsTr("Yeni yerini göster")
                        : bar._moveAfterUninstall !== null ? qsTr("Taşınıyor…")
                        : bar.vm.packageInstalled ? qsTr("Yamayı taşı")
                        : qsTr("Düzelt")
                    onClicked: bar._missing ? bar.pick("change") : bar._fix()
                }
                AppButton {
                    icon: Icons.folderSearch
                    Layout.alignment: Qt.AlignRight
                    visible: !bar._missing
                    size: "small"
                    style: "plain"
                    text: qsTr("Başka klasör seç")
                    onClicked: bar.pick("change")
                }
            }
        }
    }

    // ── What a picked folder turned out to be, when it needs a word ────────
    Loader {
        id: verdictLoader
        property var result: ({})
        property string intent: "change"
        function show(r, intent) {
            verdictLoader.result = r
            verdictLoader.intent = intent
            verdictLoader.active = true
        }
        active: false
        sourceComponent: Component {
            GameFolderDialog {
                parent: Overlay.overlay
                report: verdictLoader.result
                intent: verdictLoader.intent
                gameName: bar.vm.gameName
                onUseFolder: (root, engine) => bar._bind(root, engine, verdictLoader.intent === "install")
                onPickAnother: Qt.callLater(bar.pick, verdictLoader.intent === "install" ? "install" : "change")
                onMovePatch: (root, engine) => bar._move(root, engine)
                onClosed: verdictLoader.active = false
                Component.onCompleted: open()
            }
        }
    }
}
