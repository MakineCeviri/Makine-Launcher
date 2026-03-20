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

        // ApexYama credit — shown for ALL Apex-sourced games
        Rectangle {
            Layout.fillWidth: true
            visible: contribRoot.isApex
            implicitHeight: apexCol.implicitHeight + 16
            radius: Dimensions.radiusMD
            color: "#15907575"
            border.color: "#30907575"; border.width: 1

            ColumnLayout {
                id: apexCol
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                spacing: 6

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Bu çeviri ApexYama tarafından sağlanmaktadır")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Medium
                    color: "#c0907575"
                }

                RowLayout {
                    spacing: Dimensions.spacingMD

                    Rectangle {
                        width: freeLabel.implicitWidth + 16; height: 22
                        radius: 6; color: "#204ecdc4"; border.color: "#404ecdc4"; border.width: 1
                        Text {
                            id: freeLabel; anchors.centerIn: parent
                            textFormat: Text.PlainText; text: qsTr("Ücretsiz Yama")
                            font.pixelSize: Dimensions.fontCaption - 1; font.weight: Font.DemiBold
                            color: "#4ecdc4"
                        }
                    }

                    Rectangle {
                        width: proLabel.implicitWidth + 16; height: 22
                        radius: 6; color: "#20d4a843"; border.color: "#40d4a843"; border.width: 1
                        Text {
                            id: proLabel; anchors.centerIn: parent
                            textFormat: Text.PlainText; text: qsTr("Profesyonel Yama")
                            font.pixelSize: Dimensions.fontCaption - 1; font.weight: Font.DemiBold
                            color: "#d4a843"
                        }
                    }

                    Item { Layout.fillWidth: true }

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
}
