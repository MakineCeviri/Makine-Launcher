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
    property string apexTier: ""  // "pro" or "both"

    // Filter out Apex translator names from display
    readonly property var _filteredContributors: {
        if (!contributors || contributors.length === 0) return []
        var skip = {"Herald": true, "Oracle": true, "Profesyonel": true, "PlayCeviri": true}
        var result = []
        for (var i = 0; i < contributors.length; i++) {
            if (!skip[contributors[i].name])
                result.push(contributors[i])
        }
        return result
    }

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding

    readonly property int _padding: Dimensions.paddingXL

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

        // Contributors list (filtered, non-Apex names only)
        Repeater {
            model: contribRoot._filteredContributors
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Dimensions.spacingLG

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXXS
                    Text {
                        textFormat: Text.PlainText
                        text: modelData.name || ""
                        font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Türkiye Oyuncu Topluluğu")
                        font.pixelSize: Dimensions.fontCaption
                        color: Theme.textMuted
                    }
                }

                TurkishFlagBadge {
                    Layout.alignment: Qt.AlignVCenter
                    flagWidth: 26; flagHeight: 17
                }
            }
        }

        // Placeholder when no contributors (and not Apex)
        RowLayout {
            Layout.fillWidth: true
            visible: contribRoot._filteredContributors.length === 0 && !contribRoot.isApex
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

        // ApexYama credit — shown for ALL Apex-sourced games
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
    }
}
