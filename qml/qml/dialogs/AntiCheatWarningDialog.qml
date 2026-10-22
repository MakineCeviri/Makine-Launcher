// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * AntiCheatWarningDialog.qml - Asked before installing into a game with an
 * anti-cheat system: which systems were found, how risky each is, and what
 * the risk is (online play can get the account banned). "Vazgeç" is the
 * default; "Yine de kur" has to be chosen.
 */
BaseDialog {
    id: root

    // Anti-cheat detection result
    property var detectedSystems: []  // [{name, shortName, description, severity, warning}]
    property string gameName: ""
    property bool animationsEnabled: true

    signal continueAnyway()

    title: qsTr("Bu oyunda hile koruması var")
    width: 480
    contentHeight: contentColumn.implicitHeight

    function getSeverityColor(severity) {
        switch (severity) {
            case "low": return Theme.severityLow
            case "medium": return Theme.severityMedium
            case "high": return Theme.destructive
            case "critical": return Theme.severityCritical
            default: return Theme.severityMedium
        }
    }

    function getSeverityLabel(severity) {
        switch (severity) {
            case "low": return qsTr("Düşük risk")
            case "medium": return qsTr("Orta risk")
            case "high": return qsTr("Yüksek risk")
            case "critical": return qsTr("Kritik risk")
            default: return qsTr("Bilinmiyor")
        }
    }

    // Get highest severity
    readonly property string highestSeverity: {
        var severities = ["low", "medium", "high", "critical"]
        var maxIndex = 0
        for (var i = 0; i < detectedSystems.length; i++) {
            var idx = severities.indexOf(detectedSystems[i].severity)
            if (idx > maxIndex) maxIndex = idx
        }
        return severities[maxIndex]
    }

    accentColor: getSeverityColor(root.highestSeverity)

    header: SheetHeader {
        glyph: Icons.shieldAlert
        tint: root.accentColor
        title: root.title
        subtitle: root.gameName !== "" ? root.gameName + "  ·  " + root.getSeverityLabel(root.highestSeverity)
                                       : root.getSeverityLabel(root.highestSeverity)
        onCloseClicked: { root.cancelled(); root.close() }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 14

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            textFormat: Text.PlainText
            text: qsTr("Çeviri dosyaları oyunun koruma sistemine değiştirilmiş dosya gibi görünebilir. Tek oyunculu modda genelde sorun olmaz; çevrimiçi modda oyun açılmayabilir ya da hesabın engellenebilir. Emin değilsen kurma.")
            font.pixelSize: Dimensions.body
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        Card {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            visible: root.detectedSystems.length > 0
            implicitHeight: sysCol.implicitHeight
            radius: Dimensions.radiusLG
            color: Theme.darkMode ? Theme.surfaceLight : Theme.bgPrimary
            hairline: false
            elevated: false

            ColumnLayout {
                id: sysCol
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                Repeater {
                    model: root.detectedSystems

                    Item {
                        id: sys
                        required property var modelData
                        required property int index
                        readonly property color tone: root.getSeverityColor(sys.modelData.severity)

                        Layout.fillWidth: true
                        implicitHeight: sysRow.implicitHeight + 24

                        Rectangle {
                            visible: sys.index > 0
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 54
                            height: 1
                            color: Theme.separator
                        }

                        RowLayout {
                            id: sysRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            IconTile {
                                Layout.alignment: Qt.AlignTop
                                size: 28
                                glyph: sys.modelData.severity === "low" ? Icons.info : Icons.shield
                                tint: sys.tone
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                RowLayout {
                                    spacing: 8
                                    Text {
                                        textFormat: Text.PlainText
                                        text: sys.modelData.name || sys.modelData.shortName || ""
                                        font.pixelSize: Dimensions.body
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }
                                    StatusBadge {
                                        text: root.getSeverityLabel(sys.modelData.severity)
                                        tint: sys.tone
                                        showDot: false
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: sys.modelData.warning || sys.modelData.description || ""
                                    visible: text !== ""
                                    font.pixelSize: Dimensions.footnote
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                }
                            }
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
                id: cancelButton
                style: "filled"
                text: qsTr("Vazgeç")
                onClicked: { root.cancelled(); root.close() }
            }
            AppButton {
                style: "destructiveTinted"
                text: qsTr("Yine de kur")
                onClicked: { root.continueAnyway(); root.close() }
            }
        }
    }

    onOpened: cancelButton.forceActiveFocus()
}
