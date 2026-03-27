import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: 72

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginSM
        anchors.rightMargin: Dimensions.marginSM
        spacing: Dimensions.spacingLG

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            Text {
                textFormat: Text.PlainText
                text: qsTr("Tema")
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Text {
                textFormat: Text.PlainText
                text: qsTr("Uygulama görünümünü seç")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }
        }

        Rectangle {
            Layout.preferredWidth: themeRow.implicitWidth + 8
            Layout.preferredHeight: 36
            radius: Dimensions.radiusStandard
            color: Theme.surfaceLight

            RowLayout {
                id: themeRow
                anchors.centerIn: parent
                spacing: 0

                Rectangle {
                    width: 60
                    height: 28
                    radius: Dimensions.radiusStandard
                    color: !SettingsManager.isDarkMode ? Theme.primary : "transparent"
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Açık tema")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.isDarkMode = false
                    Keys.onSpacePressed: SettingsManager.isDarkMode = false

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("Açık")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: !SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: !SettingsManager.isDarkMode ? Theme.textOnColor : Theme.textSecondary
                    }

                    // Focus indicator
                    FocusRing { offset: -1 }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SettingsManager.isDarkMode = false
                    }
                }

                Rectangle {
                    width: 60
                    height: 28
                    radius: Dimensions.radiusStandard
                    color: SettingsManager.isDarkMode ? Theme.primary : "transparent"
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Koyu tema")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.isDarkMode = true
                    Keys.onSpacePressed: SettingsManager.isDarkMode = true

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("Koyu")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: SettingsManager.isDarkMode ? Theme.textOnColor : Theme.textSecondary
                    }

                    // Focus indicator
                    FocusRing { offset: -1 }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SettingsManager.isDarkMode = true
                    }
                }
            }
        }
    }
}
