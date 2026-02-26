import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: contribRoot
    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL
    Layout.rightMargin: Dimensions.marginXL
    spacing: Dimensions.spacingLG

    // Required properties from parent
    required property var contributors  // [{name, role}]

    Text {
        textFormat: Text.PlainText
        text: qsTr("Özel Teşekkür")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    // Contributors list (when available)
    Rectangle {
        Layout.fillWidth: true
        visible: contribRoot.contributors.length > 0
        implicitHeight: contributorsCol.height + Dimensions.marginML * 2
        radius: Dimensions.radiusStandard
        color: Theme.glassBackground
        border.color: Theme.glassBorder; border.width: 1

        ColumnLayout {
            id: contributorsCol
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: Dimensions.marginML
            spacing: Dimensions.spacingLG

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
        }
    }

    // Placeholder when no contributors
    Rectangle {
        Layout.fillWidth: true
        visible: contribRoot.contributors.length === 0
        implicitHeight: 56; radius: Dimensions.radiusStandard
        color: Theme.glassBackground; border.color: Theme.glassBorder; border.width: 1
        Row {
            anchors.centerIn: parent; spacing: Dimensions.spacingLG
            // Turkish flag mini
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
}
