// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * UninstallPluginDialog.qml - "Eklenti kaldırılsın mı?" with the "delete its data too" choice,
 * shared by the plugin list and a plugin's detail page. A plugin that ships with Makine is
 * hidden rather than deleted (PluginManager::uninstall); the catalogue offers it again.
 */
// ConfirmDialog keeps the header and the buttons; only the body is replaced
// by one with the "delete data too" choice.
ConfirmDialog {
    id: root

    property string pluginId: ""
    property string pluginName: ""
    // Ships with Makine: removing hides it (its files are the launcher's); the catalogue adds it back
    property bool bundled: false

    parent: Overlay.overlay
    title: qsTr("Eklenti kaldırılsın mı?")
    confirmText: qsTr("Kaldır")
    property bool removeData: false
    contentHeight: _uninstallBody.implicitHeight
    onAboutToShow: removeData = false
    onConfirmed: if (PluginManager) PluginManager.uninstall(root.pluginId, removeData)

    contentItem: ColumnLayout {
        id: _uninstallBody
        spacing: Dimensions.spacingMD

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            textFormat: Text.PlainText
            text: root.bundled ? qsTr("“%1” Makine ile gelir; kaldırınca listeden ve menüden kalkar, çalışmaz. İstersen Eklentiler › Keşfet'ten yeniden ekleyebilirsin.").arg(root.pluginName)
                            : qsTr("“%1” kaldırılacak. Eklenti çalışıyorsa önce durdurulur.").arg(root.pluginName)
            font.pixelSize: Dimensions.fontSM
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.5
        }

        CheckBox {
            id: dataBox
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            text: qsTr("Eklentinin verilerini de sil")
            checked: root.removeData
            onToggled: root.removeData = checked

            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: dataBox.leftPadding
                y: (dataBox.height - height) / 2
                radius: Dimensions.radiusSM
                color: dataBox.checked ? Theme.primary : "transparent"
                border.color: dataBox.checked ? Theme.primary : Theme.textPrimary25
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    visible: dataBox.checked
                    textFormat: Text.PlainText
                    text: "\u2713"
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textOnColor
                }
            }

            contentItem: Label {
                leftPadding: dataBox.indicator.width + dataBox.spacing
                textFormat: Text.PlainText
                text: dataBox.text
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                verticalAlignment: Text.AlignVCenter
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            textFormat: Text.PlainText
            text: qsTr("İşaretlemezseniz ayarlar ve eklentinin kaydettiği veriler korunur; yeniden kurarsanız kaldığınız yerden devam eder.")
            font.pixelSize: Dimensions.fontCaption
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }
}
