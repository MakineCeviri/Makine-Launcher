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
        anchors.leftMargin: Dimensions.marginSM
        anchors.rightMargin: Dimensions.marginSM
        spacing: Dimensions.spacingLG

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            Text {
                text: qsTr("Tema")
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Text {
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
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Light theme")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.isDarkMode = false
                    Keys.onSpacePressed: SettingsManager.isDarkMode = false

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Açık")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: !SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: !SettingsManager.isDarkMode ? "white" : Theme.textSecondary
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
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
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Dark theme")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.isDarkMode = true
                    Keys.onSpacePressed: SettingsManager.isDarkMode = true

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Koyu")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: SettingsManager.isDarkMode ? Font.Medium : Font.Normal
                        color: SettingsManager.isDarkMode ? "white" : Theme.textSecondary
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
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
