import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineAI 1.0

/**
 * NavBar - Main navigation bar with logo, page links, discord, notifications, and settings
 */
Rectangle {
    id: navBarRoot

    property int currentIndex: 0
    property bool animationsEnabled: true

    signal homeClicked()
    signal libraryClicked()
    signal settingsClicked()

    color: Theme.withAlpha(Theme.surface, 0.7)

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.withAlpha(Theme.textPrimary, 0.08)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginLG
        anchors.rightMargin: Dimensions.marginLG
        spacing: Dimensions.spacingXL

        Item {
            id: logoContainer
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignVCenter
            scale: logoMouse.containsMouse ? 1.05 : 1.0
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Home")
            activeFocusOnTab: true
            Keys.onReturnPressed: navBarRoot.homeClicked()
            Keys.onSpacePressed: navBarRoot.homeClicked()

            Behavior on scale {
                NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic }
            }

            AnimatedGradientGlow {
                anchors.centerIn: parent
                width: 52; height: 52
                active: true
                animationsEnabled: navBarRoot.animationsEnabled
                opacity: logoMouse.containsMouse ? 1.0
                       : navBarRoot.currentIndex === 0 ? 0.7
                       : 0.4
                Behavior on opacity { NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic } }
            }

            Rectangle {
                id: logoClip
                anchors.centerIn: parent
                width: Dimensions.navbarIconSizeLogo
                height: Dimensions.navbarIconSizeLogo
                radius: Dimensions.navbarIconSizeLogo * 0.25
                color: "transparent"
                clip: true

                Image {
                    id: logoImage
                    anchors.fill: parent
                    source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    visible: logoImage.status !== Image.Ready
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.logoGold }
                        GradientStop { position: 0.5; color: Theme.logoCoral }
                        GradientStop { position: 1.0; color: Theme.logoGreen }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "M"
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.Bold
                        color: Theme.textOnColor
                    }
                }
            }


            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.homeClicked()
            }

            Loader {
                active: logoMouse.containsMouse
                sourceComponent: ToolTip {
                    visible: true
                    text: qsTr("Ana Menü")
                    delay: 500
                }
            }
        }

        NavItem {
            text: qsTr("Kütüphanem")
            selected: navBarRoot.currentIndex === 1
            onClicked: navBarRoot.libraryClicked()
        }

        Item { Layout.fillWidth: true }

        Item {
            id: discordItem
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            Accessible.role: Accessible.Link
            Accessible.name: "Discord"
            activeFocusOnTab: true
            Keys.onReturnPressed: Qt.openUrlExternally(Dimensions.discordUrl)
            Keys.onSpacePressed: Qt.openUrlExternally(Dimensions.discordUrl)

            property bool hovered: discordMouse.containsMouse
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

            readonly property bool _windowVisible: discordItem.Window.window !== null
                                                    && discordItem.Window.window.visibility !== Window.Minimized
                                                    && discordItem.Window.window.visibility !== Window.Hidden
            property real pulse: 0.8
            SequentialAnimation on pulse {
                loops: Animation.Infinite
                running: !discordItem.hovered && navBarRoot.animationsEnabled
                         && discordItem._windowVisible
                NumberAnimation { from: 0.8; to: 0.95; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.95; to: 0.8; duration: 3000; easing.type: Easing.InOutSine }
                onRunningChanged: {
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.registerAnimation("discordPulse", running)
                }
            }

            Image {
                id: discordIcon
                anchors.centerIn: parent
                width: 20; height: 20
                source: "qrc:/qt/qml/MakineAI/resources/icons/discord.svg"
                sourceSize: Qt.size(20, 20)
                opacity: discordItem.hovered ? 1.0 : discordItem.pulse
            }

            MouseArea {
                id: discordMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }

            Loader {
                active: discordMouse.containsMouse
                sourceComponent: ToolTip {
                    visible: true
                    text: "Discord"
                    delay: 400
                }
            }
        }

        // Settings gear icon
        Item {
            id: settingsItem
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Ayarlar")
            activeFocusOnTab: true
            Keys.onReturnPressed: navBarRoot.settingsClicked()
            Keys.onSpacePressed: navBarRoot.settingsClicked()

            property bool hovered: settingsMouse.containsMouse
            property bool isSelected: navBarRoot.currentIndex === 2
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

            rotation: hovered ? 30 : 0
            Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

            // Gear icon — Segoe MDL2 Assets glyph (no Canvas repaint loop)
            Text {
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
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
            }

            MouseArea {
                id: settingsMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.settingsClicked()
            }

            Loader {
                active: settingsMouse.containsMouse
                sourceComponent: ToolTip {
                    visible: true
                    text: qsTr("Ayarlar")
                    delay: 400
                }
            }
        }
    }
}
