import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: 72

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: qsTr("Tema")
                font.pixelSize: 14
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Text {
                text: qsTr("Uygulama görünümünü seç")
                font.pixelSize: 13
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
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Açık")

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Açık")
                        font.pixelSize: 12
                        font.weight: !SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: !SettingsManager.isDarkMode ? "white" : Theme.textSecondary
                    }

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
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Koyu")

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Koyu")
                        font.pixelSize: 12
                        font.weight: SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: SettingsManager.isDarkMode ? "white" : Theme.textSecondary
                    }

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
