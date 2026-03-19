import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TitleBar - Custom frameless window title bar with logo, title, and window controls
 */
Rectangle {
    id: titleBarRoot

    required property Window windowRef
    property bool libraryMode: false
    signal minimizeClicked()
    signal closeClicked()
    signal trayClicked()

    color: Theme.bgPrimary90

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.textPrimary08
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: 200

        onPressed: titleBarRoot.windowRef.startSystemMove()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginMS
        anchors.rightMargin: 0
        spacing: Dimensions.spacingMD

        Rectangle {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            radius: Dimensions.radiusStandard
            visible: titleBarRoot.libraryMode
            color: Theme.turkishRed

            Rectangle {
                x: 3; y: 4.5
                width: 9; height: 9
                radius: 4.5
                color: Theme.textOnColor
            }
            Rectangle {
                x: 5; y: 5.3
                width: 7.5; height: 7.5
                radius: 3.75
                color: Theme.turkishRed
            }
            Canvas {
                x: 9.5; y: 5
                width: 8; height: 8
                onPaint: {
                    var ctx = getContext("2d")
                    var cx = 4, cy = 4
                    var R = 3.5
                    var r = R * 0.382
                    ctx.beginPath()
                    for (var i = 0; i < 5; i++) {
                        var oa = i * 72 * Math.PI / 180
                        var ia = (i * 72 + 36) * Math.PI / 180
                        if (i === 0) ctx.moveTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                        else ctx.lineTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                        ctx.lineTo(cx - r * Math.cos(ia), cy - r * Math.sin(ia))
                    }
                    ctx.closePath()
                    ctx.fillStyle = "white"
                    ctx.fill()
                }
            }
        }

        Image {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            visible: !titleBarRoot.libraryMode
            source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
            sourceSize: Qt.size(18, 18)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
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
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: "M"
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Bold
                    color: Theme.textOnColor
                }
            }
        }

        Label {
            textFormat: Text.PlainText
            text: "Makine Launcher"
            font.pixelSize: Dimensions.fontCaption
            font.weight: Font.Medium
            color: Theme.textMuted
        }

        Label {
            textFormat: Text.PlainText
            text: Dimensions.appVersionFull
            font.pixelSize: Dimensions.fontMicro
            color: Theme.textMuted
            opacity: 0.5
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 0

            WindowButton {
                icon: "\uE70D"
                tooltip: qsTr("Gizle")
                onClicked: titleBarRoot.trayClicked()
            }

            WindowButton {
                icon: "\uE921"
                tooltip: qsTr("K\u00fc\u00e7\u00fclt")
                onClicked: titleBarRoot.minimizeClicked()
            }

            WindowButton {
                icon: "\uE8BB"
                isClose: true
                tooltip: qsTr("Kapat")
                onClicked: titleBarRoot.closeClicked()
            }
        }
    }

    // Internal WindowButton component (flat, no borders)
    component WindowButton: Rectangle {
        property string icon: ""
        property bool isClose: false
        property string tooltip: ""
        signal clicked()

        Accessible.role: Accessible.Button
        Accessible.name: tooltip
        Accessible.onPressAction: clicked()
        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: 46
        height: 32
        color: btnMouse.containsMouse
            ? (isClose ? Theme.closeButtonHover : Theme.glassBorder)
            : "transparent"
        radius: 0

        Behavior on color {
            ColorAnimation { duration: Dimensions.animFast }
        }

        Label {
            textFormat: Text.PlainText
            anchors.centerIn: parent
            text: icon
            font.pixelSize: Dimensions.fontCaption
            font.family: "Segoe MDL2 Assets"
            color: btnMouse.containsMouse && isClose ? Theme.textOnColor : Theme.textSecondary

            Behavior on color {
                ColorAnimation { duration: Dimensions.animFast }
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onClicked: parent.clicked()
        }

        StyledToolTip {
            visible: btnMouse.containsMouse && tooltip !== ""
            text: tooltip
            delay: 500
        }
    }
}
