import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: ssRoot
    Layout.fillWidth: true
    spacing: Dimensions.spacingMD

    // Required properties from parent
    required property var screenshots

    visible: screenshots.length > 0

    Text {
        textFormat: Text.PlainText
        text: qsTr("Ekran Görüntüleri")
        font.pixelSize: Dimensions.fontSM
        font.weight: Font.DemiBold
        color: Theme.textMuted
        font.letterSpacing: 0.5
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 140

        ListView {
            id: screenshotList
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: Dimensions.spacingMD; clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: ssRoot.screenshots

            delegate: Rectangle {
                required property string modelData
                required property int index
                width: 240; height: 135
                radius: Dimensions.radiusMD
                color: Theme.surfaceActive
                border.color: delegateMouse.containsMouse ? Theme.accent30 : Theme.glassBorder
                border.width: 1
                scale: delegateMouse.containsMouse ? 1.02 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                Image {
                    anchors.fill: parent; anchors.margins: 1
                    source: modelData
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(480, 270)
                    asynchronous: true

                    // Rounded clip via layer
                    layer.enabled: true
                    layer.effect: Item {
                        Rectangle {
                            anchors.fill: parent
                            radius: Dimensions.radiusMD - 1
                        }
                    }
                }

                MouseArea {
                    id: delegateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        // Left arrow
        Rectangle {
            anchors.left: parent.left; anchors.leftMargin: Dimensions.spacingXS
            anchors.verticalCenter: parent.verticalCenter
            width: 30; height: 30; radius: 15
            color: ssLeftMouse.containsMouse ? Theme.bgPrimary90 : Theme.bgPrimary70
            border.color: Theme.glassBorder; border.width: 1
            visible: screenshotList.contentX > screenshotList.originX + 10
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2190"
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
            }
            MouseArea {
                id: ssLeftMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: screenshotList.contentX = Math.max(screenshotList.originX, screenshotList.contentX - 260)
            }
        }

        // Right arrow
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: Dimensions.spacingXS
            anchors.verticalCenter: parent.verticalCenter
            width: 30; height: 30; radius: 15
            color: ssRightMouse.containsMouse ? Theme.bgPrimary90 : Theme.bgPrimary70
            border.color: Theme.glassBorder; border.width: 1
            visible: screenshotList.contentX < screenshotList.contentWidth - screenshotList.width - 10
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2192"
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
            }
            MouseArea {
                id: ssRightMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: screenshotList.contentX = Math.min(screenshotList.contentWidth - screenshotList.width, screenshotList.contentX + 260)
            }
        }
    }
}
