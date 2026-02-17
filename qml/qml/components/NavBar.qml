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
    signal projectsClicked()
    signal translationClicked()
    signal settingsClicked()
    signal notificationClicked()

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
                opacity: logoMouse.containsMouse ? 0.9
                       : navBarRoot.currentIndex === 0 ? 0.5
                       : 0.2
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

            ToolTip {
                visible: logoMouse.containsMouse
                text: qsTr("Ana Menü")
                delay: 500
            }
        }

        NavItem {
            text: qsTr("Kütüphane")
            selected: navBarRoot.currentIndex === 2
            onClicked: navBarRoot.translationClicked()
        }

        NavItem {
            text: qsTr("Projelerimiz")
            selected: navBarRoot.currentIndex === 1
            onClicked: navBarRoot.projectsClicked()
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

            property real pulse: 0.8
            SequentialAnimation on pulse {
                loops: Animation.Infinite
                running: !discordItem.hovered && navBarRoot.animationsEnabled
                NumberAnimation { from: 0.8; to: 0.95; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.95; to: 0.8; duration: 3000; easing.type: Easing.InOutSine }
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

            ToolTip {
                visible: discordMouse.containsMouse
                text: "Discord"
                delay: 400
            }
        }

        // Notification bell icon
        Item {
            id: notifBellItem
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Bildirimler")
            activeFocusOnTab: true
            Keys.onReturnPressed: navBarRoot.notificationClicked()
            Keys.onSpacePressed: navBarRoot.notificationClicked()

            property bool hovered: bellMouse.containsMouse
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

            Canvas {
                id: bellCanvas
                anchors.centerIn: parent
                width: 18; height: 18
                property bool hov: notifBellItem.hovered
                property int uc: NotificationService.unreadCount
                onHovChanged: requestPaint()
                onUcChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var c = hov ? Theme.textPrimary : Theme.textMuted
                    ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, hov ? 0.9 : 0.6)
                    ctx.lineWidth = 1.5
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"

                    // Bell shape
                    ctx.beginPath()
                    ctx.moveTo(4, 12)
                    ctx.quadraticCurveTo(4, 6, 9, 3)
                    ctx.quadraticCurveTo(14, 6, 14, 12)
                    ctx.lineTo(15, 13)
                    ctx.lineTo(3, 13)
                    ctx.closePath()
                    ctx.stroke()

                    // Clapper
                    ctx.beginPath()
                    ctx.moveTo(7, 14)
                    ctx.quadraticCurveTo(9, 17, 11, 14)
                    ctx.stroke()

                    // Top nub
                    ctx.beginPath()
                    ctx.arc(9, 2.5, 1, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            // Unread count badge
            Rectangle {
                visible: NotificationService.unreadCount > 0
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 2
                anchors.rightMargin: 2
                width: Math.max(14, unreadLbl.width + 6)
                height: 14
                radius: 7
                color: Theme.destructive

                Text {
                    id: unreadLbl
                    anchors.centerIn: parent
                    text: NotificationService.unreadCount > 9 ? "9+" : NotificationService.unreadCount.toString()
                    font.pixelSize: 8
                    font.weight: Font.Bold
                    color: Theme.textOnColor
                }
            }

            MouseArea {
                id: bellMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.notificationClicked()
            }

            ToolTip {
                visible: bellMouse.containsMouse
                text: NotificationService.unreadCount > 0
                    ? qsTr("Bildirimler (%1)").arg(NotificationService.unreadCount)
                    : qsTr("Bildirimler")
                delay: 400
            }
        }

        // Separator dot
        Rectangle {
            Layout.preferredWidth: 3; Layout.preferredHeight: 3
            Layout.alignment: Qt.AlignVCenter
            radius: 2
            color: Theme.withAlpha(Theme.textPrimary, 0.15)
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
            property bool isSelected: navBarRoot.currentIndex === 3
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

            rotation: hovered ? 30 : 0
            Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

            Canvas {
                id: gearCanvas
                anchors.centerIn: parent
                width: 18; height: 18
                property bool sel: settingsItem.isSelected
                property bool hov: settingsItem.hovered
                onSelChanged: requestPaint()
                onHovChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var c = sel ? Theme.primary : (hov ? Theme.textPrimary : Theme.textMuted)
                    ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, sel ? 1.0 : (hov ? 0.9 : 0.6))
                    ctx.lineWidth = 1.5
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"

                    var cx = 9, cy = 9, outerR = 8, innerR = 6
                    var teeth = 6

                    // Gear outer shape
                    ctx.beginPath()
                    for (var i = 0; i < teeth; i++) {
                        var a1 = (i / teeth) * Math.PI * 2 - Math.PI / 2
                        var a2 = a1 + (0.3 / teeth) * Math.PI * 2
                        var a3 = a1 + (0.5 / teeth) * Math.PI * 2
                        var a4 = a1 + (0.8 / teeth) * Math.PI * 2
                        var a5 = a1 + (1.0 / teeth) * Math.PI * 2

                        if (i === 0) ctx.moveTo(cx + Math.cos(a1) * innerR, cy + Math.sin(a1) * innerR)
                        ctx.lineTo(cx + Math.cos(a2) * outerR, cy + Math.sin(a2) * outerR)
                        ctx.lineTo(cx + Math.cos(a3) * outerR, cy + Math.sin(a3) * outerR)
                        ctx.lineTo(cx + Math.cos(a4) * innerR, cy + Math.sin(a4) * innerR)
                        ctx.lineTo(cx + Math.cos(a5) * innerR, cy + Math.sin(a5) * innerR)
                    }
                    ctx.closePath()
                    ctx.stroke()

                    // Center circle
                    ctx.beginPath()
                    ctx.arc(cx, cy, 3, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            MouseArea {
                id: settingsMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.settingsClicked()
            }

            ToolTip {
                visible: settingsMouse.containsMouse
                text: qsTr("Ayarlar")
                delay: 400
            }
        }
    }
}
