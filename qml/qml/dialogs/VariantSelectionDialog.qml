// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * VariantSelectionDialog.qml - Which version (or platform) of the game is
 * on this PC, asked before an install whose files differ between them.
 *
 * Usage:
 *   VariantSelectionDialog {
 *       variants: ["1.00", "1.04", "1.05"]
 *       variantType: "version"  // or "platform"
 *       onVariantSelected: function(variant) { install(variant) }
 *   }
 */
BaseDialog {
    id: root

    property var variants: []
    property string variantType: "version" // "version" or "platform"
    property int selectedIndex: -1

    signal variantSelected(string variant)

    title: variantType === "platform" ? qsTr("Hangi platformda oynuyorsun?") : qsTr("Oyunun hangi sürümü kurulu?")

    width: 420
    contentHeight: contentColumn.implicitHeight

    function _accept() {
        if (root.selectedIndex < 0) return
        root.variantSelected(root.variants[root.selectedIndex])
        root.close()
    }

    header: SheetHeader {
        glyph: root.variantType === "platform" ? Icons.monitor : Icons.layers
        title: root.title
        subtitle: root.variantType === "platform"
                  ? qsTr("Bu yamanın her platform için ayrı dosyaları var. Oyunu aldığın mağazayı seç.")
                  : qsTr("Bu yamanın oyunun her sürümü için ayrı dosyaları var. Yanlış sürüm oyunu açılmaz hale getirebilir; emin değilsen oyunun başlangıç ekranındaki ya da mağazadaki sürüm numarasına bak.")
        onCloseClicked: { root.cancelled(); root.close() }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 0

        Card {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.bottomMargin: 4
            implicitHeight: listCol.implicitHeight
            radius: Dimensions.radiusLG
            color: Theme.darkMode ? Theme.surfaceLight : Theme.bgPrimary
            hairline: false
            elevated: false

            ColumnLayout {
                id: listCol
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                Repeater {
                    model: root.variants

                    Item {
                        id: option
                        required property int index
                        required property string modelData
                        readonly property bool isSelected: root.selectedIndex === option.index

                        Layout.fillWidth: true
                        implicitHeight: 44

                        activeFocusOnTab: true
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: option.modelData
                        Accessible.checked: option.isSelected
                        Keys.onSpacePressed: root.selectedIndex = option.index
                        Keys.onReturnPressed: { root.selectedIndex = option.index; root._accept() }

                        Rectangle {
                            visible: option.index > 0
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 44
                            height: 1
                            color: Theme.separator
                        }
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            visible: optionMouse.containsMouse || option.activeFocus
                            color: Theme.fillQuaternary
                            radius: Dimensions.radiusLG - 1
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            // Radio
                            Rectangle {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                radius: 9
                                color: option.isSelected ? Theme.primary : "transparent"
                                border.color: option.isSelected ? Theme.primary : Theme.textTertiary
                                border.width: 1.5
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 7; height: 7; radius: 3.5
                                    color: "#FFFFFF"
                                    visible: option.isSelected
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: option.modelData
                                font.pixelSize: Dimensions.body
                                font.weight: option.isSelected ? Font.DemiBold : Font.Medium
                                color: Theme.textPrimary
                            }
                        }

                        MouseArea {
                            id: optionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedIndex = option.index
                            onDoubleClicked: { root.selectedIndex = option.index; root._accept() }
                        }
                    }
                }
            }
        }
    }

    footer: Item {
        implicitHeight: 80

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 10

            Item { Layout.fillWidth: true }

            AppButton {
                icon: Icons.x
                style: "gray"
                text: qsTr("Vazgeç")
                onClicked: { root.cancelled(); root.close() }
            }
            AppButton {
                icon: Icons.download
                style: "filled"
                enabled: root.selectedIndex >= 0
                text: qsTr("Bu sürümle kur")
                onClicked: root._accept()
            }
        }
    }
}
