import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

Item {
    id: root

    width: 350
    height: 140

    Image {
        id: logoImage
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
        width: 80
        height: 80
        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
    }

    Text {
        id: titleText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: logoImage.bottom
        anchors.topMargin: 12
        text: "MakineAI"
        font.pixelSize: 36
        font.weight: Font.Bold
        font.letterSpacing: -0.5
        color: Theme.splashGold
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: titleText.bottom
        anchors.topMargin: 8
        implicitWidth: badgeContent.implicitWidth + 16
        implicitHeight: 24
        radius: Dimensions.radiusStandard
        color: Qt.rgba(1.0, 0.84, 0.0, 0.2)

        Row {
            id: badgeContent
            anchors.centerIn: parent
            spacing: 6

            Image {
                source: "qrc:/qt/qml/MakineAI/resources/icons/star.svg"
                width: 12
                height: 12
                sourceSize: Qt.size(12, 12)
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: qsTr("Türkçe Yama")
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.splashGold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
