import QtQuick
import MakineAI 1.0

/**
 * GameCard.qml - Minimal game card for catalog strips.
 * Image + name overlay. No effects, no glow, no animations per card.
 */
Item {
    id: root

    property string gameId: ""
    property string gameName: ""
    property string imageUrl: ""
    property string steamAppId: ""
    property string installPath: ""
    property bool verified: false
    property bool translated: false
    property bool hasUpdate: false

    signal clicked()

    // SIZE — responsive, preserves aspect ratio (130:185)
    readonly property real _aspectRatio: 130.0 / 185.0
    width: Math.round(height * _aspectRatio)
    height: Dimensions.cardHeight

    // Delegate recycling support (reuseItems)
    ListView.onPooled: gameImage.source = ""
    ListView.onReused: {
        var id = root.steamAppId || root.gameId
        root.imageUrl = ImageCache.resolve(id)
    }

    // Reconnect when image downloads finish
    Connections {
        target: gameImage.status !== Image.Ready ? ImageCache : null
        function onImageReady(readyId) {
            var myId = root.steamAppId || root.gameId
            if (readyId === myId)
                root.imageUrl = ImageCache.resolve(myId)
        }
    }

    // Card background
    Rectangle {
        id: cardBg
        anchors.fill: parent
        radius: Dimensions.cardBorderRadius
        color: mouseArea.containsMouse
            ? Theme.withAlpha(Theme.textPrimary, 0.08)
            : Theme.surfaceLight
        clip: true

        // Game image
        Image {
            id: gameImage
            anchors.fill: parent
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(260, 370)
            asynchronous: true
            cache: true
            retainWhileLoading: true
        }

        // Fallback initials (no image)
        Text {
            anchors.centerIn: parent
            visible: gameImage.status !== Image.Ready && gameImage.status !== Image.Loading
            text: root.gameName.substring(0, 2).toUpperCase()
            font.pixelSize: Dimensions.fontHero
            font.weight: Font.Bold
            color: Theme.textMuted
        }

        // Bottom gradient for name readability
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.35
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.7) }
            }
        }

        // Game name
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Dimensions.marginBase
            text: root.gameName
            font.pixelSize: Dimensions.fontCaption
            font.weight: Font.DemiBold
            color: "#fff"
            maximumLineCount: 2
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
        }

        // Update/warning dot (top-right, minimal)
        Rectangle {
            visible: root.translated && (!root.verified || root.hasUpdate)
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 6
            width: 8; height: 8; radius: 4
            color: root.hasUpdate ? Theme.warning : Theme.textMuted
        }
    }

    // Mouse area
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
