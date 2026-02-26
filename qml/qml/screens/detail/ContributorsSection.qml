import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

SectionContainer {
    id: contribRoot
    contentSpacing: Dimensions.spacingLG

    // Required properties from parent
    required property var contributors  // [{name, role}]

    Text {
        textFormat: Text.PlainText
        text: qsTr("Özel Teşekkür")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    SettingsDivider { variant: "section" }

    // Contributors list (when available)
    Repeater {
        model: contribRoot.contributors
        RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            // Turkish flag icon
            TurkishFlagBadge {
                flagWidth: 26; flagHeight: 17
            }

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
        }
    }

    // Placeholder when no contributors
    Row {
        visible: contribRoot.contributors.length === 0
        spacing: Dimensions.spacingLG
        TurkishFlagBadge {
            flagWidth: 26; flagHeight: 17
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            textFormat: Text.PlainText
            text: qsTr("Türkiye Oyuncu Topluluğu")
            font.pixelSize: Dimensions.fontBody
            color: Theme.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
