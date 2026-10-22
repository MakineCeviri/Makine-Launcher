// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * UpdateBanner — a game update broke the patch, or its files are gone.
 * Says which, what happened, and repairs with one button.
 */
Rectangle {
    id: banner

    required property var vm

    readonly property bool _broken: banner.vm.impactLevel === "broken"
    readonly property color _tone: banner._broken ? Theme.error : Theme.warning

    Layout.fillWidth: true
    Layout.maximumWidth: 1240
    Layout.leftMargin: Dimensions.pageMargin
    Layout.rightMargin: Dimensions.pageMargin
    Layout.alignment: Qt.AlignHCenter
    Layout.topMargin: 18
    visible: banner.vm.impactLevel !== "" && banner.vm.impactLevel !== "safe" && banner.vm.impactLevel !== "unknown"
    implicitHeight: row.implicitHeight + 32
    radius: Dimensions.radiusLG
    color: Theme.withAlpha(banner._tone, Theme.darkMode ? 0.12 : 0.07)
    border.color: Theme.withAlpha(banner._tone, 0.32)
    border.width: 1

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        IconTile {
            Layout.alignment: Qt.AlignTop
            size: 34
            glyph: banner._broken ? Icons.triangleAlert : Icons.fileText
            tint: banner._tone
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: banner._broken ? qsTr("Oyun güncellendi, çeviri bozulmuş")
                                     : qsTr("Bazı çeviri dosyaları eksik")
                font.pixelSize: Dimensions.headline
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: banner.vm.updateImpact ? banner.vm.updateImpact.summary : ""
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        AppButton {
            Layout.alignment: Qt.AlignVCenter
            style: "filled"
            tint: banner._tone
            icon: Icons.wrench
            text: qsTr("Onar")
            toolTip: qsTr("Yamayı kaldırıp yeniden kurar; eksik ya da bozulan dosyalar yerine gelir")
            onClicked: {
                GameService.recoverTranslation(banner.vm.gameId)
                banner.vm.updateImpact = null
            }
        }
    }
}
