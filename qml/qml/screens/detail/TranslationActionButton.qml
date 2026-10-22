// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * TranslationActionButton — the patch's action, on the game's art.
 *
 * States (TranslationStateManager): download, update, installing, installed,
 * completed, broken, external — plus "locate" for a game no scan found,
 * which asks for its folder first (the install follows once GameFolderBar
 * binds it).
 *
 * The state is shown and the action is a button saying what it does: an
 * installed patch reads "Türkçe yama kurulu" with a separate "Kaldır" next
 * to it — never a status that turns into a destructive button on hover.
 */
RowLayout {
    id: actionBtn

    required property var vm

    signal translateClicked()
    signal updateClicked()
    signal uninstallClicked()
    signal repairClicked()
    signal locateClicked()

    spacing: 10
    visible: actionBtn.vm.hasTranslation && (actionBtn.vm.isGameInstalled || actionBtn._locate)

    // ── State resolution (C++ TranslationStateManager) ──
    readonly property string _state: _locate ? "locate" : TranslationStateManager.state
    readonly property bool _locate: actionBtn.vm.hasTranslation && !actionBtn.vm.isGameInstalled
                                    && actionBtn.vm.externalUrl === ""

    function _reevaluateState() {
        var source = actionBtn.vm.isHangar ? "hangar" : (actionBtn.vm.isApex ? "apex" : "")
        TranslationStateManager.evaluate(
            actionBtn.vm.hasTranslationUpdate,
            actionBtn.vm.packageInstalled,
            actionBtn.vm.isInstallingTranslation,
            actionBtn.vm.installCompleted,
            actionBtn.vm.impactLevel,
            actionBtn.vm.externalUrl,
            source
        )
    }

    // Re-evaluate whenever any input changes
    Connections {
        target: actionBtn.vm
        function onHasTranslationUpdateChanged() { actionBtn._reevaluateState() }
        function onPackageInstalledChanged()      { actionBtn._reevaluateState() }
        function onIsInstallingTranslationChanged(){ actionBtn._reevaluateState() }
        function onInstallCompletedChanged()      { actionBtn._reevaluateState() }
        function onImpactLevelChanged()           { actionBtn._reevaluateState() }
        function onExternalUrlChanged()           { actionBtn._reevaluateState() }
    }
    Component.onCompleted: _reevaluateState()

    readonly property string _externalName: actionBtn.vm.isHangar && !actionBtn.vm.isApex ? "Hangar Çeviri"
                                          : actionBtn.vm.isApex && !actionBtn.vm.isHangar ? "ApexYama"
                                          : ""

    function _primary() {
        switch (actionBtn._state) {
        case "update":   actionBtn.updateClicked(); break
        case "broken":   actionBtn.vm.impactLevel === "lost" ? actionBtn.repairClicked() : actionBtn.updateClicked(); break
        case "external": Qt.openUrlExternally(actionBtn.vm.externalUrl); break
        case "locate":   actionBtn.locateClicked(); break
        default:         actionBtn.translateClicked(); break
        }
    }

    // ── The main action ──
    AppButton {
        visible: actionBtn._state === "download" || actionBtn._state === "update"
                 || actionBtn._state === "broken" || actionBtn._state === "external"
                 || actionBtn._state === "locate"
        size: "large"
        style: actionBtn._state === "update" || actionBtn._state === "broken" ? "filled" : "white"
        tint: Theme.darkMode ? "#FF9F0A" : "#C93400"
        busy: actionBtn._state === "locate" && actionBtn.vm.folderChecking
        icon: actionBtn._state === "download" ? Icons.download
            : actionBtn._state === "update" ? Icons.refreshCw
            : actionBtn._state === "broken" ? Icons.wrench
            : actionBtn._state === "locate" ? Icons.folderOpen : ""
        trailingIcon: actionBtn._state === "external" ? Icons.arrowUpRight : ""
        text: {
            switch (actionBtn._state) {
            case "update":   return qsTr("Yamayı Güncelle")
            case "broken":   return actionBtn.vm.impactLevel === "lost" ? qsTr("Yamayı Onar") : qsTr("Onar ve Güncelle")
            case "external": return actionBtn._externalName !== ""
                                    ? qsTr("%1 sitesinde aç").arg(actionBtn._externalName)
                                    : qsTr("Yamanın sitesine git")
            case "locate":   return actionBtn.vm.folderChecking ? qsTr("Klasöre bakılıyor…")
                                                                : qsTr("Oyun Klasörünü Seç ve Kur")
            default:         return qsTr("Türkçe Yamayı Kur")
            }
        }
        onClicked: actionBtn._primary()
    }

    // ── Installing: progress in the pill, with a way out ──
    Rectangle {
        id: progressPill
        visible: actionBtn._state === "installing"
        Layout.preferredWidth: Math.max(300, progressLabel.implicitWidth + 96)
        Layout.preferredHeight: 44
        radius: 22
        color: Theme.glassOnArt
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.14)

        Accessible.role: Accessible.ProgressBar
        Accessible.name: qsTr("Kuruluyor, yüzde %1").arg(actionBtn.vm.progressPercent)

        // Fill
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 3
            width: actionBtn.vm.installProgress > 0
                   ? Math.max(height, (parent.width - 6) * Math.min(1, actionBtn.vm.installProgress)) : 0
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.18)
            Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 6
            spacing: 10

            Spinner { size: 16; color: "#FFFFFF" }
            Text {
                id: progressLabel
                Layout.fillWidth: true
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: Dimensions.callout
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                color: "#FFFFFF"
                text: {
                    if (actionBtn.vm.isDownloading && actionBtn.vm.installStatus !== "")
                        return actionBtn.vm.installStatus
                    if (actionBtn.vm.installProgress > 0)
                        return qsTr("Kuruluyor… %%1").arg(actionBtn.vm.progressPercent)
                    return actionBtn.vm.installStatus || qsTr("Hazırlanıyor…")
                }
            }
            AppButton {
                style: "glass"
                size: "small"
                icon: Icons.x
                toolTip: actionBtn.vm.isDownloading ? qsTr("İndirmeyi durdur") : qsTr("Kurulumu iptal et")
                onClicked: {
                    if (actionBtn.vm.isDownloading)
                        TranslationDownloader.cancelDownload(actionBtn.vm.gameId)
                    else
                        GameService.cancelInstallation()
                }
            }
        }
    }

    // ── Installed / just installed: the state, plainly ──
    Rectangle {
        visible: actionBtn._state === "installed" || actionBtn._state === "completed"
        Layout.preferredHeight: 44
        Layout.preferredWidth: stateRow.implicitWidth + 40
        radius: 22
        color: actionBtn._state === "completed" ? Theme.success : Theme.glassOnArt
        border.width: actionBtn._state === "completed" ? 0 : 1
        border.color: Qt.rgba(1, 1, 1, 0.14)
        Behavior on color { ColorAnimation { duration: Dimensions.animNormal } }

        Accessible.role: Accessible.StaticText
        Accessible.name: stateLabel.text

        RowLayout {
            id: stateRow
            anchors.centerIn: parent
            spacing: 8
            Icon {
                glyph: Icons.circleCheck
                size: 18
                color: actionBtn._state === "completed" ? "#FFFFFF" : "#30D158"
            }
            Text {
                id: stateLabel
                textFormat: Text.PlainText
                text: actionBtn._state === "completed" ? qsTr("Yama başarıyla kuruldu")
                                                       : qsTr("Türkçe yama kurulu")
                font.pixelSize: Dimensions.headline
                font.weight: Font.DemiBold
                color: "#FFFFFF"
            }
        }
    }

    // ── Taking it off: its own button, confirmed by the page ──
    AppButton {
        visible: actionBtn._state === "installed" || actionBtn._state === "update"
                 || (actionBtn._state === "broken" && actionBtn.vm.packageInstalled)
        size: "large"
        style: "glass"
        icon: Icons.trash2
        // Its undo program is running (the console below it says what it does).
        busy: actionBtn.vm.isUndoing
        enabled: !actionBtn.vm.isUndoing
        text: qsTr("Kaldır")
        toolTip: qsTr("Türkçe yamayı kaldır; oyunun orijinal dosyaları geri gelir")
        onClicked: actionBtn.uninstallClicked()
    }
}
