import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Rectangle {
    id: contribRoot

    // Required properties from parent
    required property var contributors  // [{name, role}]
    property string externalUrl: ""
    property bool isApex: false
    property bool isHangar: false
    property string apexTier: ""  // "pro" or "both"

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding

    readonly property int _padding: Dimensions.paddingXL

    // Group contributors by role for display (filter out placeholder)
    readonly property var _grouped: {
        const groups = []
        const map = {}
        if (!contribRoot.contributors) return groups
        for (let i = 0; i < contribRoot.contributors.length; ++i) {
            const c = contribRoot.contributors[i]
            if (!c || !c.name) continue
            // Skip the static community placeholder — already shown above
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

    radius: Dimensions.radiusSection
    color: Theme.textPrimary03
    border.color: Theme.textPrimary06
    border.width: 1

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: contribRoot._padding
        spacing: Dimensions.spacingLG

        Text {
            textFormat: Text.PlainText
            text: qsTr("Özel Teşekkür")
            font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        SettingsDivider { variant: "section" }

        // Community credit — clean, minimal
        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: qsTr("Türkiye Oyuncu Topluluğu")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }

            TurkishFlagBadge {
                Layout.alignment: Qt.AlignVCenter
                flagWidth: 26; flagHeight: 17
            }
        }

        // Contributors by role — minimal list
        Repeater {
            model: contribRoot._grouped

            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 4

                Text {
                    textFormat: Text.PlainText
                    text: modelData.role
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    color: Theme.textSecondary
                    opacity: 0.7
                }

                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: modelData.names.join("  ·  ")
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ApexYama credit
        Rectangle {
            Layout.fillWidth: true
            visible: contribRoot.isApex
            implicitHeight: apexRow.implicitHeight + 16
            radius: Dimensions.radiusMD
            color: "#15907575"
            border.color: "#30907575"; border.width: 1

            RowLayout {
                id: apexRow
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                spacing: Dimensions.spacingSM

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: contribRoot.apexTier === "pro"
                        ? qsTr("Bu çeviri Apex Yama'da profesyonel olarak bulunmaktadır.")
                        : qsTr("Bu çeviri Apex Yama'da profesyonel ve ücretsiz olarak bulunmaktadır.")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }

                Text {
                    textFormat: Text.PlainText
                    text: "apexyama.com \u2192"
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    color: "#d4a843"

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://apexyama.com")
                    }
                }
            }
        }

        // HangarCeviri credit
        Rectangle {
            Layout.fillWidth: true
            visible: contribRoot.isHangar
            implicitHeight: hangarRow.implicitHeight + 16
            radius: Dimensions.radiusMD
            color: "#158B5CF6"
            border.color: "#308B5CF6"; border.width: 1

            RowLayout {
                id: hangarRow
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                spacing: Dimensions.spacingSM

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Bu çeviri Hangar Çeviri'de bulunmaktadır.")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }

                Text {
                    textFormat: Text.PlainText
                    text: "hangarceviri.com \u2192"
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    color: "#8B5CF6"

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://www.hangarceviri.com")
                    }
                }
            }
        }
    }
}
