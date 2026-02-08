import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: titleBarRoot
    property var windowRef
    property bool translationMode: false
    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()

    implicitHeight: Dimensions.titlebarHeight
    color: Theme.withAlpha(Theme.surface, 0.7)

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: 120
        property point dragPos

        onPressed: function(mouse) {
            dragPos = Qt.point(mouse.x, mouse.y)
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var delta = Qt.point(mouse.x - dragPos.x, mouse.y - dragPos.y)
                windowRef.x += delta.x
                windowRef.y += delta.y
            }
        }
        onDoubleClicked: titleBarRoot.maximizeClicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 0
        spacing: 8

        Rectangle {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            radius: 2
            visible: titleBarRoot.translationMode
            color: Theme.turkishRed

            Rectangle {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: -2
                width: 10
                height: 10
                radius: 5
                color: "white"

                Rectangle {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 2
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.turkishRed
                }
            }
        }

        Image {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            visible: !titleBarRoot.translationMode
            source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
            sourceSize: Qt.size(18, 18)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true

            Rectangle {
                anchors.fill: parent
                radius: Dimensions.radiusStandard
                visible: parent.status !== Image.Ready
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.gold }
                    GradientStop { position: 0.5; color: Theme.olive }
                    GradientStop { position: 1.0; color: Theme.pastelBlue }
                }

                Text {
                    anchors.centerIn: parent
                    text: "M"
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Bold
                    color: "white"
                }
            }
        }

        Label {
            text: "MakineAI"
            font.pixelSize: Dimensions.fontSM
            font.weight: Font.Medium
            color: Theme.textSecondary
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 0

            WindowButton {
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/minimize.svg"
                onClicked: titleBarRoot.minimizeClicked()
            }

            WindowButton {
                iconSource: windowRef.visibility === Window.Maximized
                    ? "qrc:/qt/qml/MakineAI/resources/icons/minimize.svg"
                    : "qrc:/qt/qml/MakineAI/resources/icons/maximize.svg"
                onClicked: titleBarRoot.maximizeClicked()
            }

            WindowButton {
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/close.svg"
                isCloseButton: true
                onClicked: titleBarRoot.closeClicked()
            }
        }
    }
}
