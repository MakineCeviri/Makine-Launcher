import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

Rectangle {
    id: contribRoot

    // Required properties from parent
    required property var contributors  // [{name, role}]

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

        // Contributors list (when available)
        Repeater {
            model: contribRoot.contributors
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

        // Placeholder when no contributors
        RowLayout {
            Layout.fillWidth: true
            visible: contribRoot.contributors.length === 0
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
    }
}
