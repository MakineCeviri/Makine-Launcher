// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SidebarStatus — the sidebar's foot: what the launcher is doing on its
 * own. A launcher update gets a small card with its one action; a running
 * scan or update check gets a quiet line with a spinner; otherwise only
 * the version.
 */
ColumnLayout {
    id: status

    signal openSettings()

    spacing: 8

    readonly property int _state: UpdateService.state
    readonly property bool _updateCard: UpdateService.indicatorVisible
    readonly property bool _checking: _state === UpdateService.Checking

    // ── Launcher update ──────────────────────────────────────────────────
    Rectangle {
        Layout.fillWidth: true
        visible: status._updateCard
        implicitHeight: updateCol.implicitHeight + 24
        radius: Dimensions.radiusLG
        color: Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.10 : 0.08)
        border.width: Theme.darkMode ? 1 : 0
        border.color: Theme.withAlpha(Theme.primaryText, 0.16)

        ColumnLayout {
            id: updateCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                IconTile {
                    size: 24
                    glyph: status._state === UpdateService.Ready ? Icons.circleCheck : Icons.circleArrowDown
                    tint: status._state === UpdateService.Ready ? Theme.success : Theme.primary
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: status._state === UpdateService.Ready ? qsTr("Güncelleme hazır")
                            : status._state === UpdateService.Installing ? qsTr("Güncelleniyor")
                            : qsTr("Yeni sürüm")
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.callout
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: UpdateService.version !== "" ? "v" + UpdateService.version : UpdateService.statusText
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.caption1
                        color: Theme.textMuted
                    }
                }
            }

            // Download progress
            Rectangle {
                Layout.fillWidth: true
                visible: status._state === UpdateService.Downloading || status._state === UpdateService.Verifying
                Layout.preferredHeight: 4
                radius: 2
                color: Theme.fillTertiary
                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, UpdateService.progress))
                    height: parent.height
                    radius: 2
                    color: Theme.primary
                    Behavior on width { NumberAnimation { duration: Dimensions.animNormal } }
                }
            }

            AppButton {
                Layout.fillWidth: true
                visible: UpdateService.actionable
                size: "small"
                style: status._state === UpdateService.Available || status._state === UpdateService.Ready
                       ? "filled" : "gray"
                fillWidth: true
                text: status._state === UpdateService.Available ? qsTr("İndir")
                    : status._state === UpdateService.Ready ? qsTr("Yeniden başlat ve kur")
                    : qsTr("Vazgeç")
                onClicked: {
                    switch (status._state) {
                    case UpdateService.Available: UpdateService.download(); break
                    case UpdateService.Ready: UpdateService.install(); break
                    case UpdateService.Downloading:
                    case UpdateService.Verifying: UpdateService.cancel(); break
                    }
                }
            }
            AppButton {
                Layout.fillWidth: true
                visible: status._state === UpdateService.Available
                size: "small"
                style: "plain"
                fillWidth: true
                text: qsTr("Sonra")
                onClicked: UpdateService.dismiss()
            }
        }
    }

    // ── Background work ──────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        visible: GameService.isScanning || status._checking
        spacing: 8
        Spinner { size: 14; color: Theme.textMuted }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: GameService.isScanning ? qsTr("Oyunların taranıyor…") : qsTr("Güncellemeler denetleniyor…")
            elide: Text.ElideRight
            font.pixelSize: Dimensions.caption1
            color: Theme.textMuted
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        textFormat: Text.PlainText
        text: qsTr("Sürüm %1").arg(Dimensions.appVersion)
        elide: Text.ElideMiddle
        font.pixelSize: Dimensions.caption2
        color: Theme.textTertiary
    }
}
