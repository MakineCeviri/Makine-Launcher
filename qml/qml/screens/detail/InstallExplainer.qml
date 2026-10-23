// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * InstallExplainer — what the install button is about to do, said under it.
 *
 * Before an install: the three things that happen (download and check, back
 * up the files the patch changes, put the Turkish files in) and that taking
 * it off brings the originals back. Installed: how to take it off. Update
 * waiting, or a patch on another site: what the button does then. Nothing
 * while an install runs — the button speaks for itself.
 */
ColumnLayout {
    id: ex

    required property var vm
    // TranslationActionButton._state: download | update | installed | external | …
    property string mode: ""

    property real _downloadBytes: 0
    function _refresh() {
        const e = ex.vm.steamAppId !== "" ? GameService.getCatalogEntry(ex.vm.steamAppId) : ({})
        ex._downloadBytes = Number(e.downloadSize || e.size || 0)
    }
    Connections {
        target: ex.vm
        function onGameIdChanged() { ex._refresh() }
    }
    // The recipe arrives with the game's detail, often after the page opened.
    Connections {
        target: CoreBridge
        function onPackageDetailEnriched(appId) {
            if (appId === ex.vm.gameId || appId === ex.vm.steamAppId) {
                ex.vm.patchProgram = GameService.patchProgram(ex.vm.gameId)
                ex.vm.patchUndoes = GameService.patchProgramUndoes(ex.vm.gameId)
            }
        }
    }
    readonly property bool _ownProgram: ex.vm.patchProgram !== ""
    Component.onCompleted: _refresh()

    readonly property string _size: ex._downloadBytes >= 1048576
        ? qsTr("%1 MB").arg((ex._downloadBytes / 1048576).toLocaleString(Qt.locale("tr_TR"), "f", 1))
        : ex._downloadBytes > 0 ? qsTr("%1 KB").arg(Math.max(1, Math.round(ex._downloadBytes / 1024))) : ""

    visible: ex.mode === "download" || ex.mode === "installed"
             || ex.mode === "update" || ex.mode === "external"
    spacing: 12

    component Step: Card {
        id: step
        property int number: 1
        property string glyph: ""
        property string title: ""
        property string detail: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: stepCol.implicitHeight + 28
        radius: Dimensions.radiusLG

        ColumnLayout {
            id: stepCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 8

            RowLayout {
                spacing: 8
                IconTile { size: 26; glyph: step.glyph; tint: Theme.primary; solid: false }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("%1. adım").arg(step.number)
                    font.pixelSize: Dimensions.caption1
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: step.title
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.callout
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: step.detail
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }
        }
    }

    // Before an install: the three steps
    RowLayout {
        Layout.fillWidth: true
        visible: ex.mode === "download"
        spacing: 12

        Step {
            number: 1
            glyph: Icons.cloudDownload
            title: ex._size !== "" ? qsTr("İndirilir ve doğrulanır · %1").arg(ex._size)
                                   : qsTr("İndirilir ve doğrulanır")
            detail: qsTr("Paket makineceviri.org'dan gelir; bozuk ya da değiştirilmiş inerse kurulmaz.")
        }
        Step {
            number: 2
            glyph: ex._ownProgram ? Icons.folderCheck : Icons.archive
            title: ex._ownProgram ? qsTr("Oyunun klasörü doğrulanır")
                                  : qsTr("Değişecek dosyaların yedeği alınır")
            detail: ex._ownProgram
                ? qsTr("Yamanın beklediği oyun dosyaları klasörde aranır; yanlış klasörse hiçbir şey çalıştırılmaz.")
                : qsTr("Yamanın üzerine yazacağı oyun dosyaları önce bir kenara kopyalanır.")
        }
        Step {
            number: 3
            glyph: Icons.languages
            title: ex._ownProgram ? qsTr("Yamanın programı kurar") : qsTr("Türkçe dosyalar oyuna kurulur")
            detail: ex._ownProgram
                ? (ex.vm.patchUndoes
                   ? qsTr("%1, oyunun bulunduğu klasörle çalışır; yazdıkları bu sayfada görünür. Kaldırırsan aynı program geri alır.")
                   : qsTr("%1, oyunun bulunduğu klasörle çalışır. Bu yamanın katalog kaydında programın kaldırma komutu yazılı değil; launcher onu bu hâliyle kurmaz."))
                      .arg(ex.vm.patchProgram)
                : qsTr("Kaldırırsan yedek geri yüklenir; kayıtlarına ve ayarlarına dokunulmaz.")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: ex.mode !== "download"
        spacing: 10

        Icon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            glyph: ex.mode === "external" ? Icons.externalLink : Icons.info
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
            text: ex.mode === "installed" && ex._ownProgram
                  ? qsTr("Yamayı kaldırmak için \"Kaldır\"a bas: yamanın kendi programı açılır ve oyunda yaptıklarını geri alır.")
                : ex.mode === "installed"
                  ? qsTr("Yamayı kaldırmak için \"Kaldır\"a bas: yama silinir, yedeklenen orijinal dosyalar geri yüklenir.")
                : ex.mode === "update"
                  ? qsTr("Yamanın yeni bir sürümü yayımlandı. \"Yamayı Güncelle\" yeni sürümü indirip eskisinin yerine kurar.")
                  : qsTr("Bu oyunun Türkçe yaması başka bir çeviri ekibinin sitesinde yayımlanıyor. Düğme seni o sayfaya götürür; indirmeyi ve kurulumu o site anlatır.")
        }
    }
}
