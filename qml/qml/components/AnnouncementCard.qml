import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AnnouncementCard.qml - Discord community banner inside styled container.
 * Click opens Discord invite.
 */
Rectangle {
    id: root

    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 2
    Layout.preferredHeight: layoutTopRowHeight

    radius: Dimensions.radiusSection
    color: Theme.surface

    GradientBorder {
        cornerRadius: root.radius
    }

    readonly property string _bannerUrl:
        "https://cdn.makineceviri.org/assets/banners/announcement.png?v=" + Date.now()

    // Banner image (hidden — rendered via MultiEffect mask)
    // cache: false — always fetch fresh from CDN (banner changes frequently)
    Image {
        id: bannerImg
        anchors.fill: parent
        source: root._bannerUrl
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(width, height)
        asynchronous: true
        cache: false
        mipmap: true
        visible: false
    }

    // Rounded corner mask
    Item {
        id: bannerMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: Dimensions.radiusSection; color: "white" }
    }

    // Masked banner — image clipped to container radius
    MultiEffect {
        anchors.fill: bannerImg
        source: bannerImg
        maskEnabled: true
        maskSource: bannerMask
        visible: bannerImg.status === Image.Ready
    }

    // Top edge glass highlight
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.rightMargin: 1; anchors.topMargin: 1
        height: 1; radius: Dimensions.radiusSection; z: 5
        opacity: 0.7
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.2; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.18) }
            GradientStop { position: 0.8; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    MouseArea {
        id: bannerMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
    }

    // Discord pill button — slides up from bottom-right on hover
    Item {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 54
        height: 54
        clip: true
        z: 10

        Rectangle {
            id: discordBtn
            width: 40
            height: 40
            radius: Dimensions.radiusMD
            color: Theme.discordColor
            x: 0
            y: bannerMa.containsMouse ? 0 : parent.height

            Behavior on y {
                NumberAnimation {
                    duration: Dimensions.animNormal
                    easing.type: Easing.OutCubic
                }
            }

            Image {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/MakineLauncher/resources/icons/discord-white.svg"
                width: 22; height: 22
                sourceSize: Qt.size(22, 22)
                mipmap: true
            }
        }
    }
}
