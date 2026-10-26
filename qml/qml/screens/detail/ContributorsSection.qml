// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * ContributorsSection — who made the Turkish patch, grouped by role, and
 * where a patch published by another team lives. The last part of the
 * detail page's one container (HeroSection).
 */
Item {
    id: contribRoot

    required property var contributors  // [{name, role}]
    property string externalUrl: ""
    property bool isApex: false
    property bool isHangar: false
    property string apexTier: ""  // "pro" or "both"

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 40

    // Group contributors by role for display (filter out placeholder)
    readonly property var _grouped: {
        const groups = []
        const map = {}
        if (!contribRoot.contributors) return groups
        for (let i = 0; i < contribRoot.contributors.length; ++i) {
            const c = contribRoot.contributors[i]
            if (!c || !c.name) continue
            // The community credit has its own line below
            if (c.name === "Türkiye Oyuncu Topluluğu") continue
            const role = c.role || "Çevirmen"
            if (!(role in map)) {
                map[role] = groups.length
                groups.push({ role: role, names: [] })
            }
            groups[map[role]].names.push(c.name)
        }
        return groups
    }

    ColumnLayout {
        id: contentLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            IconTile { size: 26; glyph: Icons.heartHandshake; tint: Theme.turkishRed }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: qsTr("Emeği geçenler")
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            TurkishFlagBadge {
                Layout.alignment: Qt.AlignVCenter
                flagWidth: 24; flagHeight: 16
            }
        }

        // Contributors by role
        Flow {
            Layout.fillWidth: true
            spacing: 28
            visible: contribRoot._grouped.length > 0

            Repeater {
                model: contribRoot._grouped

                ColumnLayout {
                    required property var modelData
                    width: Math.min(contentLayout.width, Math.max(160, namesText.implicitWidth))
                    spacing: 3

                    Text {
                        textFormat: Text.PlainText
                        text: modelData.role
                        font.pixelSize: Dimensions.caption1
                        font.family: Dimensions.fontText
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.4
                        font.capitalization: Font.AllUppercase
                        color: Theme.textMuted
                    }
                    Text {
                        id: namesText
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: modelData.names.join(", ")
                        font.pixelSize: Dimensions.body
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: qsTr("Özel teşekkür: Türkiye Oyuncu Topluluğu")
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }

        // Where the patch is published, when another team publishes it
        ListRow {
            visible: contribRoot.isApex || contribRoot.isHangar
            Layout.leftMargin: -16
            Layout.rightMargin: -14
            icon: Icons.globe
            iconTint: contribRoot.isHangar && !contribRoot.isApex ? "#8B5CF6" : "#D4A843"
            clickable: true
            showChevron: false
            title: contribRoot.isHangar && !contribRoot.isApex ? qsTr("Hangar Çeviri")
                 : qsTr("Apex Yama")
            subtitle: contribRoot.isHangar && !contribRoot.isApex
                      ? qsTr("Bu çeviri Hangar Çeviri'de yayımlanıyor.")
                      : contribRoot.apexTier === "pro"
                        ? qsTr("Bu çeviri Apex Yama'da profesyonel olarak bulunuyor.")
                        : qsTr("Bu çeviri Apex Yama'da profesyonel ve ücretsiz olarak bulunuyor.")
            onClicked: Qt.openUrlExternally(contribRoot.isHangar && !contribRoot.isApex
                                            ? "https://www.hangarceviri.com" : "https://apexyama.com")
            Icon { glyph: Icons.arrowUpRight; size: 15; color: Theme.textMuted }
        }
    }
}
