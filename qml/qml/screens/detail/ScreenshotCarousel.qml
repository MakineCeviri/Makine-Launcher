import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

ColumnLayout {
    id: ssRoot
    Layout.fillWidth: true
    spacing: Dimensions.spacingLG

    // Required properties from parent
    required property var screenshots

    visible: screenshots.length > 0

    Text {
        Layout.leftMargin: Dimensions.marginXL
        text: qsTr("Ekran Görüntüleri")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 220

        ListView {
            id: screenshotList
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: Dimensions.spacingLG; clip: true
            leftMargin: Dimensions.marginXL; rightMargin: Dimensions.marginXL
            boundsBehavior: Flickable.StopAtBounds
            model: ssRoot.screenshots

            delegate: Rectangle {
                required property string modelData
                width: 380; height: 214
                radius: Dimensions.radiusStandard
                color: Theme.surfaceActive; clip: true

                Image {
                    anchors.fill: parent
                    source: modelData
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(760, 428)
                    asynchronous: true; cache: true
                }
                Rectangle {
                    anchors.fill: parent; radius: parent.radius
                    color: "transparent"
                    border.color: Theme.glassBorder; border.width: 1
                }
            }
        }

        // Left arrow
        Rectangle {
            anchors.left: parent.left; anchors.leftMargin: Dimensions.spacingMD
            anchors.verticalCenter: parent.verticalCenter
            width: 36; height: 36; radius: 18
            color: ssLeftMouse.containsMouse ? Theme.withAlpha(Theme.bgPrimary, 0.90) : Theme.withAlpha(Theme.bgPrimary, 0.65)
            border.color: Theme.glassBorder; border.width: 1
            visible: screenshotList.contentX > screenshotList.originX + 10
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Text { anchors.centerIn: parent; text: "\u2190"; font.pixelSize: Dimensions.fontMD; color: Theme.textPrimary }
            MouseArea {
                id: ssLeftMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: screenshotList.contentX = Math.max(screenshotList.originX, screenshotList.contentX - 400)
            }
        }

        // Right arrow
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: Dimensions.spacingMD
            anchors.verticalCenter: parent.verticalCenter
            width: 36; height: 36; radius: 18
            color: ssRightMouse.containsMouse ? Theme.withAlpha(Theme.bgPrimary, 0.90) : Theme.withAlpha(Theme.bgPrimary, 0.65)
            border.color: Theme.glassBorder; border.width: 1
            visible: screenshotList.contentX < screenshotList.contentWidth - screenshotList.width - 10
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Text { anchors.centerIn: parent; text: "\u2192"; font.pixelSize: Dimensions.fontMD; color: Theme.textPrimary }
            MouseArea {
                id: ssRightMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: screenshotList.contentX = Math.min(screenshotList.contentWidth - screenshotList.width, screenshotList.contentX + 400)
            }
        }
    }
}
