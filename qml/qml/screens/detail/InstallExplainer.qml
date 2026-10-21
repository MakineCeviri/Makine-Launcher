// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
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
    Component.onCompleted: _refresh()

    readonly property string _size: ex._downloadBytes >= 1048576
        ? qsTr("%1 MB").arg((ex._downloadBytes / 1048576).toLocaleString(Qt.locale("tr_TR"), "f", 1))
        : ex._downloadBytes > 0 ? qsTr("%1 KB").arg(Math.max(1, Math.round(ex._downloadBytes / 1024))) : ""

    visible: ex.mode === "download" || ex.mode === "installed"
             || ex.mode === "update" || ex.mode === "external"
    spacing: Dimensions.spacingMD

    component StepPill: Rectangle {
        id: sp
        property string glyph: ""
        property string text: ""
        implicitWidth: spRow.implicitWidth + 24
        implicitHeight: 32
        radius: 16
        color: Theme.textPrimary04
        border.width: 1
        border.color: Theme.textPrimary08
        Row {
            id: spRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingSM
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: sp.glyph
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 12
                color: Theme.accentLight
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: sp.text
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textSecondary
            }
        }
    }
    component Arrow: Text {
        textFormat: Text.PlainText
        text: "\uE76C"
        font.family: "Segoe MDL2 Assets"
        font.pixelSize: 9
        color: Theme.textMuted
    }

    // Before an install: the three steps
    Flow {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: ex.width
        visible: ex.mode === "download"
        spacing: Dimensions.spacingMD

        StepPill {
            glyph: "\uE896"
            text: ex._size !== "" ? qsTr("İndirilir ve doğrulanır (%1)").arg(ex._size)
                                  : qsTr("İndirilir ve doğrulanır")
        }
        Arrow { height: 32; verticalAlignment: Text.AlignVCenter }
        StepPill { glyph: "\uE8C8"; text: qsTr("Değişecek oyun dosyalarının yedeği alınır") }
        Arrow { height: 32; verticalAlignment: Text.AlignVCenter }
        StepPill { glyph: "\uE73E"; text: qsTr("Türkçe dosyalar oyuna kurulur") }
    }

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        lineHeight: 1.35
        font.pixelSize: Dimensions.fontSM
        color: Theme.textMuted
        text: ex.mode === "download"
              ? qsTr("Kaldırırsan yedek geri yüklenir; oyunun kayıtlarına ve ayarlarına dokunulmaz.")
            : ex.mode === "installed"
              ? qsTr("Yamayı kaldırmak için yukarıdaki düğmeye bas: yama silinir, yedeklenen orijinal dosyalar geri yüklenir.")
            : ex.mode === "update"
              ? qsTr("Yamanın yeni bir sürümü yayımlandı. Düğmeye basınca yeni sürüm indirilip kurulur.")
              : qsTr("Bu oyunun Türkçe yaması başka bir çeviri ekibinin sitesinde yayımlanıyor. Düğme seni o sayfaya götürür; indirmeyi ve kurulumu o site anlatır.")
    }
}
