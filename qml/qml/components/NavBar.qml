import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * NavBar - Minimal navigation bar: logo, page links, discord, settings.
 */
Item {
    id: navBarRoot

    property int currentIndex: 0
    property bool animationsEnabled: true

    signal homeClicked()
    signal libraryClicked()
    signal settingsClicked()

    readonly property color _bgColor: Theme.withAlpha(Theme.surface, 0.7)

    // Main background
    Rectangle {
        anchors.fill: parent
        color: navBarRoot._bgColor
    }

    // Bottom-left outward curve
    Canvas {
        x: 0; y: parent.height
        width: Dimensions.radiusSection; height: Dimensions.radiusSection
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = navBarRoot._bgColor.toString()
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(0, height)
            ctx.quadraticCurveTo(0, 0, width, 0)
            ctx.closePath()
            ctx.fill()
        }
    }

    // Bottom-right outward curve
    Canvas {
        x: parent.width - Dimensions.radiusSection; y: parent.height
        width: Dimensions.radiusSection; height: Dimensions.radiusSection
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = navBarRoot._bgColor.toString()
            ctx.beginPath()
            ctx.moveTo(width, 0)
            ctx.lineTo(width, height)
            ctx.quadraticCurveTo(width, 0, 0, 0)
            ctx.closePath()
            ctx.fill()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginLG
        anchors.rightMargin: Dimensions.marginLG
        spacing: Dimensions.spacingXL

        // Logo
        Item {
            Layout.preferredWidth: 44; Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignVCenter
            scale: logoMouse.containsMouse ? 1.05 : 1.0
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            // Radial glow behind logo
            Canvas {
                id: logoGlow
                anchors.centerIn: parent
                width: 72; height: 72
                opacity: logoMouse.containsMouse ? 0.9 : 0.5
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var cx = width / 2, cy = height / 2
                    var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, width / 2)
                    grad.addColorStop(0.0, "rgba(139, 92, 246, 0.35)")
                    grad.addColorStop(0.4, "rgba(139, 92, 246, 0.15)")
                    grad.addColorStop(0.7, "rgba(99, 102, 241, 0.06)")
                    grad.addColorStop(1.0, "rgba(99, 102, 241, 0.0)")
                    ctx.fillStyle = grad
                    ctx.beginPath()
                    ctx.arc(cx, cy, width / 2, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: Dimensions.navbarIconSizeLogo; height: Dimensions.navbarIconSizeLogo
                radius: Dimensions.navbarIconSizeLogo * 0.25
                color: "transparent"

                Image {
                    anchors.fill: parent
                    source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }
            }

            MouseArea {
                id: logoMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.homeClicked()
            }
        }

        NavItem {
            text: qsTr("Kütüphanem")
            selected: navBarRoot.currentIndex === 1
            onClicked: navBarRoot.libraryClicked()
        }

        Item { Layout.fillWidth: true }

        // Settings
        Item {
            id: settingsItem
            Layout.preferredWidth: 36; Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter

            property bool hovered: settingsMouse.containsMouse
            property bool isSelected: navBarRoot.currentIndex === 2
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            rotation: hovered ? 30 : 0
            Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\uE713"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 17
                color: settingsItem.isSelected ? Theme.primary
                     : settingsItem.hovered    ? Theme.textPrimary
                     : Theme.textMuted
                opacity: settingsItem.isSelected ? 1.0
                       : settingsItem.hovered    ? 0.9
                       : 0.6
            }

            MouseArea {
                id: settingsMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.settingsClicked()
            }
        }
    }
}
