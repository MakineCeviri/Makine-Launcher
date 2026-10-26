// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * AntiCheatWarningDialog.qml - Asked before installing into a game with an
 * anti-cheat system. Left: the game and the systems found, with their risk.
 * Right: what the risk is, and three statements the user ticks before
 * "Yine de kur" opens — until then the button counts them ("1 / 3
 * onaylandı"). "Vazgeç" holds the focus.
 */
BaseDialog {
    id: root

    // Anti-cheat detection result
    property var detectedSystems: []  // [{name, shortName, description, severity, warning}]
    property string gameName: ""
    property string artUrl: ""
    property string artFallbackUrl: ""

    readonly property var checks: [
        qsTr("Yamalı oyunla çevrim içi oynarsam hesabımın engellenebileceğini biliyorum."),
        qsTr("Yalnız tek oyunculu modda oynasam da engellenme riskinin sıfır olmadığını biliyorum ve kabul ediyorum."),
        qsTr("Olası bir engellemede Makine Çeviri'yi sorumlu tutmayacağımı kabul ediyorum.")
    ]
    property var checked: [false, false, false]
    readonly property int checkedCount: checked.filter(function(c) { return c }).length
    readonly property bool allChecked: checkedCount === checks.length

    signal continueAnyway()

    title: qsTr("Bu oyunda hile koruması var")
    width: parent ? Math.min(760, parent.width - 64) : 760
    contentHeight: body.implicitHeight

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

    function _toggle(i) {
        var next = root.checked.slice()
        next[i] = !next[i]
        root.checked = next
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

    header: Item { implicitHeight: 0 }
    footer: Item { implicitHeight: 0 }

    contentItem: RowLayout {
        id: body
        spacing: 0

        // ===== Left: the game and what was found =====
        ColumnLayout {
            Layout.preferredWidth: 280
            Layout.maximumWidth: 280
            Layout.alignment: Qt.AlignTop
            Layout.margins: 24
            spacing: 0

            Item {
                id: artFrame
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(width * 9 / 16)

                Item {
                    id: art
                    anchors.fill: parent
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: artMask
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }

                    Rectangle { anchors.fill: parent; color: Theme.surfaceLight }

                    Icon {
                        anchors.centerIn: parent
                        visible: artImg.status !== Image.Ready
                        glyph: Icons.shieldAlert
                        size: 28
                        color: Theme.textTertiary
                    }

                    Image {
                        id: artImg
                        anchors.fill: parent
                        property bool _useFallback: false
                        source: _useFallback ? root.artFallbackUrl : root.artUrl
                        fillMode: Image.PreserveAspectCrop
                        verticalAlignment: Image.AlignTop
                        sourceSize.width: 600
                        asynchronous: true
                        opacity: status === Image.Ready ? 1.0 : 0
                        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                        onStatusChanged: {
                            if (status === Image.Error && !_useFallback && root.artFallbackUrl !== "")
                                _useFallback = true
                        }
                    }
                }

                Item {
                    id: artMask
                    anchors.fill: parent
                    visible: false
                    layer.enabled: true
                    Rectangle { anchors.fill: parent; radius: Dimensions.radiusLG; color: "white" }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusLG
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.separator
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 16
                visible: root.gameName !== ""
                textFormat: Text.PlainText
                text: root.gameName
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                font.letterSpacing: Dimensions.trackingHeadline
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 18
                Layout.bottomMargin: 14
                Layout.preferredHeight: 1
                color: Theme.separator
            }

            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: qsTr("Bulunan koruma")
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }

            Repeater {
                model: root.detectedSystems

                RowLayout {
                    id: sys
                    required property var modelData
                    readonly property color tone: root.getSeverityColor(sys.modelData.severity)

                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    spacing: 10

                    IconTile {
                        Layout.alignment: Qt.AlignTop
                        size: 28
                        glyph: sys.modelData.severity === "low" ? Icons.info : Icons.shield
                        tint: sys.tone
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: sys.modelData.name || sys.modelData.shortName || ""
                            font.pixelSize: Dimensions.body
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        StatusBadge {
                            text: root.getSeverityLabel(sys.modelData.severity)
                            tint: sys.tone
                            showDot: false
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
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

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Theme.separator
        }

        // ===== Right: the risk, the statements, the decision =====
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 28
            Layout.topMargin: 24
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Icon {
                    glyph: Icons.shieldAlert
                    size: 16
                    color: root.accentColor
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.highestSeverity === "high" || root.highestSeverity === "critical"
                          ? qsTr("Dikkat") : qsTr("Önemli")
                    font.pixelSize: Dimensions.callout
                    font.weight: Font.DemiBold
                    color: root.accentColor
                }
                DialogCloseButton { onClicked: { root.cancelled(); root.close() } }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                textFormat: Text.PlainText
                text: root.title
                font.family: Dimensions.fontDisplay
                font.pixelSize: Dimensions.title2
                font.weight: Font.Bold
                font.letterSpacing: Dimensions.trackingTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 12
                textFormat: Text.PlainText
                text: qsTr("Çeviri dosyaları oyunun koruma sistemine değiştirilmiş dosya gibi görünebilir. Tek oyunculu modda genelde sorun olmaz; çevrim içi modda oyun açılmayabilir ya da hesabın engellenebilir. Çevrim içi oynamak istediğinde önce yamayı kaldır. Emin değilsen kurma.")
                font.pixelSize: Dimensions.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }

            // The statements, on the dialog's tone
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 20
                implicitHeight: checksCol.implicitHeight + 28
                radius: Dimensions.radiusLG
                color: Theme.withAlpha(root.accentColor, Theme.darkMode ? 0.06 : 0.05)
                border.width: 1
                border.color: Theme.withAlpha(root.accentColor, 0.25)

                ColumnLayout {
                    id: checksCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        Layout.bottomMargin: 4
                        textFormat: Text.PlainText
                        text: qsTr("Devam etmeden önce onayla")
                        font.pixelSize: Dimensions.footnote
                        color: Theme.textMuted
                    }

                    Repeater {
                        model: root.checks

                        Item {
                            id: chk
                            required property int index
                            required property string modelData
                            readonly property bool isChecked: root.checked[chk.index] === true

                            Layout.fillWidth: true
                            implicitHeight: chkRow.implicitHeight + 20

                            activeFocusOnTab: true
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: chk.modelData
                            Accessible.checked: chk.isChecked
                            Keys.onSpacePressed: root._toggle(chk.index)

                            Rectangle {
                                visible: chk.index > 0
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 36
                                height: 1
                                color: Theme.separator
                            }
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: Dimensions.radiusMD
                                visible: chkMouse.containsMouse || chk.activeFocus
                                color: Theme.fillQuaternary
                            }

                            RowLayout {
                                id: chkRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    Layout.alignment: Qt.AlignTop
                                    radius: 6
                                    color: chk.isChecked ? root.accentColor : "transparent"
                                    border.color: chk.isChecked ? root.accentColor : Theme.textTertiary
                                    border.width: 1.5
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    Icon {
                                        anchors.centerIn: parent
                                        visible: chk.isChecked
                                        glyph: Icons.check
                                        size: 14
                                        color: "#FFFFFF"
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: chk.modelData
                                    font.pixelSize: Dimensions.body
                                    color: chk.isChecked ? Theme.textPrimary : Theme.textSecondary
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.2
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                }
                            }

                            MouseArea {
                                id: chkMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root._toggle(chk.index)
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true; Layout.minimumHeight: 24 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                AppButton {
                    icon: Icons.x
                    id: cancelButton
                    style: "gray"
                    text: qsTr("Vazgeç")
                    onClicked: { root.cancelled(); root.close() }
                }
                AppButton {
                    icon: Icons.download
                    Layout.minimumWidth: 150
                    style: "destructive"
                    enabled: root.allChecked
                    text: root.allChecked ? qsTr("Yine de kur")
                                          : qsTr("%1 / %2 onaylandı").arg(root.checkedCount).arg(root.checks.length)
                    onClicked: { root.continueAnyway(); root.close() }
                }
            }
        }
    }

    onOpened: cancelButton.forceActiveFocus()
}
