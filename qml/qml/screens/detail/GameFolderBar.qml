// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MakineLauncher 1.0
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
                                      && !bar.vm.isGameInstalled && bar.vm.externalUrl === ""
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
        if (path !== "" && bar._folder.exists && bar.vm.externalUrl === "")
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

    // A patch from another site installs itself; the folder is ours to know only for our own.
    visible: bar.vm.hasTranslation && bar.vm.externalUrl === "" && (bar._located || bar._notFound)
    implicitHeight: bar._notFound ? hint.implicitHeight
                  : bar._notice ? notice.implicitHeight
                  : row.implicitHeight

    FolderDialog {
        id: folderPicker
        title: qsTr("%1 klasörünü seç").arg(bar.vm.gameName)
        onAccepted: bar.inspect(selectedFolder, bar._intent)
    }

    // A text button with a glyph, for the quiet row and the notice.
    component BarButton: Rectangle {
        id: bb
        property string glyph: ""
        property string text: ""
        property bool primary: false
        property color fill: Theme.primary
        signal clicked()

        implicitWidth: bbRow.implicitWidth + (bb.primary ? Dimensions.paddingXL : Dimensions.paddingLG)
        implicitHeight: bb.primary ? 36 : 32
        radius: Dimensions.radiusMD
        color: bb.primary ? (bbHover.hovered ? Qt.darker(bb.fill, 1.12) : bb.fill)
                          : (bbHover.hovered ? Theme.textPrimary10 : "transparent")
        border.width: bb.activeFocus ? 2 : 0
        border.color: Theme.accentLight
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: bb.text
        Keys.onReturnPressed: bb.clicked()
        Keys.onSpacePressed: bb.clicked()

        Row {
            id: bbRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingSM

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: bb.glyph !== ""
                textFormat: Text.PlainText
                text: bb.glyph
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 12
                color: bb.primary ? Theme.textOnColor : Theme.textSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: bb.text
                font.pixelSize: Dimensions.fontBody
                font.weight: bb.primary ? Font.DemiBold : Font.Medium
                color: bb.primary ? Theme.textOnColor : (bbHover.hovered ? Theme.textPrimary : Theme.textSecondary)
            }
        }
        HoverHandler { id: bbHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: bb.clicked() }
    }

    // ── Not found: what the button under which this sits will do ────────────
    Text {
        id: hint
        visible: bar._notFound
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Dimensions.paddingXL
        anchors.rightMargin: Dimensions.paddingXL
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        lineHeight: 1.35
        font.pixelSize: Dimensions.fontBody
        color: Theme.textMuted
        text: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerinde bulamadık. Oyunun klasörünü ya da .exe dosyasının durduğu klasörü göster; yamanın gideceği doğru yeri biz buluruz. Klasörü bu sayfaya sürükleyip bırakabilirsin de.")
    }

    // ── Located: the folder, quietly ────────────────────────────────────────
    Rectangle {
        id: row
        visible: bar._located && !bar._notice
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: 48
        radius: Dimensions.radiusLG
        color: Theme.textPrimary04
        border.width: 1
        border.color: Theme.textPrimary06

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingSM
            spacing: Dimensions.spacingLG

            Text {
                textFormat: Text.PlainText
                text: "\uF12B"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 15
                color: Theme.textMuted
            }

            Text {
                id: pathText
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: bar._folder.shown || ""
                elide: Text.ElideMiddle
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary

                HoverHandler { id: pathHover }
                ToolTip.visible: pathHover.hovered && pathText.truncated
                ToolTip.text: bar._folder.shown || ""
                ToolTip.delay: 400
            }

            // Who found it — or what just happened to it.
            Row {
                spacing: Dimensions.spacingSM
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: bar._note !== "" && !bar._checking && bar._moveAfterUninstall === null
                    textFormat: Text.PlainText
                    text: "\uE73E"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 12
                    color: Theme.success
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    text: bar._checking ? qsTr("Klasöre bakılıyor…")
                        : bar._note !== "" ? bar._note
                        : bar._source
                    font.pixelSize: Dimensions.fontSM
                    color: bar._note !== "" && !bar._checking && bar._moveAfterUninstall === null
                           ? Theme.success : Theme.textMuted
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 20
                color: Theme.textPrimary08
            }

            Row {
                spacing: Dimensions.spacingXS

                BarButton {
                    glyph: "\uED25"
                    text: qsTr("Aç")
                    onClicked: GameService.openGameFolder(bar.vm.gameId)
                }
                BarButton {
                    visible: bar._folder.resettable === true
                    glyph: "\uE777"
                    text: qsTr("Otomatik bulunana dön")
                    onClicked: GameService.resetGameFolder(bar.vm.gameId)
                }
                BarButton {
                    glyph: "\uE70F"
                    text: qsTr("Değiştir")
                    onClicked: bar.pick("change")
                }
            }
        }
    }

    // ── Located, but the user is needed ─────────────────────────────────────
    Rectangle {
        id: notice
        visible: bar._located && bar._notice
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: noticeRow.implicitHeight + Dimensions.paddingLG * 2
        radius: Dimensions.radiusLG
        color: Theme.warning08
        border.width: 1
        border.color: Theme.withAlpha(Theme.warning, 0.30)

        RowLayout {
            id: noticeRow
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
                radius: 18
                color: Theme.withAlpha(Theme.warning, 0.14)

                Text {
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: "\uE7BA"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 16
                    color: Theme.warning
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.fontLG
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
                    lineHeight: 1.3
                    font.pixelSize: Dimensions.fontBody
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
                spacing: Dimensions.spacingXS

                BarButton {
                    Layout.alignment: Qt.AlignRight
                    primary: true
                    fill: Theme.warning
                    enabled: bar._moveAfterUninstall === null
                    text: bar._missing ? qsTr("Yeni yerini göster")
                        : bar._moveAfterUninstall !== null ? qsTr("Taşınıyor…")
                        : bar.vm.packageInstalled ? qsTr("Yamayı taşı")
                        : qsTr("Düzelt")
                    onClicked: bar._missing ? bar.pick("change") : bar._fix()
                }
                BarButton {
                    Layout.alignment: Qt.AlignRight
                    visible: !bar._missing
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
