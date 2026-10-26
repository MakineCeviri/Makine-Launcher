// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * InstallOptionsDialog.qml - Which parts of a patch to install, when it
 * comes in independent pieces (text and dubbing, for example). Several can
 * be chosen; each says what it is.
 *
 * specialMode "eldenRing" tints it gold and says what the launcher renames.
 */
BaseDialog {
    id: root

    property var options: []          // [{id, label, description, icon, defaultSelected}]
    property var selectedOptionIds: []
    property string specialMode: ""   // "" or "eldenRing"
    property string gameName: ""

    signal optionsConfirmed(var selectedIds)

    accentColor: specialMode === "eldenRing" ? "#C8AA6E" : Theme.primary
    readonly property bool isEldenRing: specialMode === "eldenRing"
    readonly property bool multipleSelected: selectedOptionIds.length > 1

    title: qsTr("Neleri kuralım?")

    width: 460
    contentHeight: contentColumn.implicitHeight

    function _toggle(id) {
        var ids = root.selectedOptionIds.slice()
        var idx = ids.indexOf(id)
        if (idx >= 0) ids.splice(idx, 1)
        else ids.push(id)
        root.selectedOptionIds = ids
    }
    function _confirm() {
        if (root.selectedOptionIds.length === 0) return
        root.optionsConfirmed(root.selectedOptionIds)
        root.close()
    }

    header: SheetHeader {
        glyph: Icons.listChecks
        tint: root.accentColor
        title: root.gameName !== "" ? root.gameName : root.title
        subtitle: qsTr("Bu yama birkaç parçadan oluşuyor. Kurmak istediklerini işaretle; sonra istersen kaldırıp başka seçimle yeniden kurabilirsin.")
        onCloseClicked: { root.cancelled(); root.close() }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 0

        Card {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            implicitHeight: optCol.implicitHeight
            radius: Dimensions.radiusLG
            color: Theme.darkMode ? Theme.surfaceLight : Theme.bgPrimary
            hairline: false
            elevated: false

            ColumnLayout {
                id: optCol
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                Repeater {
                    model: root.options

                    Item {
                        id: opt
                        required property int index
                        required property var modelData

                        readonly property bool isChecked: root.selectedOptionIds.indexOf(opt.modelData.id) >= 0
                        readonly property string optIcon: opt.modelData.icon || ""

                        Layout.fillWidth: true
                        implicitHeight: optRow.implicitHeight + 24

                        activeFocusOnTab: true
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: opt.modelData.label || ""
                        Accessible.description: opt.modelData.description || ""
                        Accessible.checked: opt.isChecked
                        Keys.onSpacePressed: root._toggle(opt.modelData.id)
                        Keys.onReturnPressed: root._confirm()

                        Rectangle {
                            visible: opt.index > 0
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 46
                            height: 1
                            color: Theme.separator
                        }
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            radius: Dimensions.radiusLG - 1
                            visible: optMouse.containsMouse || opt.activeFocus
                            color: Theme.fillQuaternary
                        }

                        RowLayout {
                            id: optRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            // Checkbox
                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                Layout.alignment: Qt.AlignTop
                                radius: 6
                                color: opt.isChecked ? root.accentColor : "transparent"
                                border.color: opt.isChecked ? root.accentColor : Theme.textTertiary
                                border.width: 1.5
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Icon {
                                    anchors.centerIn: parent
                                    visible: opt.isChecked
                                    glyph: Icons.check
                                    size: 14
                                    color: "#FFFFFF"
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 6
                                    Icon {
                                        visible: opt.optIcon === "text" || opt.optIcon === "voice"
                                        glyph: opt.optIcon === "voice" ? Icons.messageCircle : Icons.fileText
                                        size: 14
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: opt.modelData.label || ""
                                        font.pixelSize: Dimensions.body
                                        font.weight: opt.isChecked ? Font.DemiBold : Font.Medium
                                        color: Theme.textPrimary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    visible: (opt.modelData.description || "").length > 0
                                    text: opt.modelData.description || ""
                                    font.pixelSize: Dimensions.footnote
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        MouseArea {
                            id: optMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root._toggle(opt.modelData.id)
                        }
                    }
                }
            }
        }

        // Elden Ring: what the launcher does on its own when both are chosen
        RowLayout {
            visible: root.isEldenRing && root.multipleSelected
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            spacing: 8

            Icon {
                Layout.alignment: Qt.AlignTop
                glyph: Icons.info
                size: 15
                color: root.accentColor
            }
            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: qsTr("eldenring.tr.exe kendiliğinden eldenring.exe olarak yeniden adlandırılacak.")
                font.pixelSize: Dimensions.footnote
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
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

            Label {
                Layout.fillWidth: true
                visible: root.selectedOptionIds.length === 0
                textFormat: Text.PlainText
                text: qsTr("En az birini seç.")
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }
            Item { Layout.fillWidth: true; visible: root.selectedOptionIds.length > 0 }

            AppButton {
                icon: Icons.x
                style: "gray"
                text: qsTr("Vazgeç")
                onClicked: { root.cancelled(); root.close() }
            }
            AppButton {
                icon: Icons.download
                style: "filled"
                tint: root.accentColor
                enabled: root.selectedOptionIds.length > 0
                text: root.selectedOptionIds.length > 1 ? qsTr("Seçilenleri kur") : qsTr("Kur")
                onClicked: root._confirm()
            }
        }
    }

    // Initialize default selections from options data
    Component.onCompleted: {
        var defaults = []
        if (root.options) {
            for (var i = 0; i < root.options.length; i++) {
                if (root.options[i].defaultSelected) {
                    defaults.push(root.options[i].id)
                }
            }
        }
        root.selectedOptionIds = defaults
    }
}
