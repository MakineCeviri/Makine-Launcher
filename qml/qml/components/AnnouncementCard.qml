import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

Rectangle {
    id: root

    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 2
    Layout.preferredHeight: layoutTopRowHeight

    radius: Dimensions.radiusSection
    color: Theme.surface
    clip: true
    border.color: heroMa.containsMouse
        ? Theme.withAlpha(Theme.accentBase, 0.30) : Qt.rgba(1, 1, 1, 0.06)
    border.width: 1
    Behavior on border.color { ColorAnimation { duration: Dimensions.animNormal } }

    // Ambient glow — hover-responsive radial glow
    AmbientGlow {
        anchors.fill: parent; z: 1
        glowColor: Theme.accentBase
        cornerRadius: Dimensions.radiusSection
        originX: 30; originY: height - 20
        intensity: 0.22; hoveredIntensity: 0.50; spread: 0.50
        hovered: heroMa.containsMouse
    }

    // Hero game data — planned localization showcase
    readonly property string _plannedAppId: "3764200"
    readonly property var _heroGame: {
        var found = GameService.getGameBySteamAppId(_plannedAppId)
        if (found && found.id)
            return found
        return {
            id: "steam_" + _plannedAppId,
            steamAppId: _plannedAppId,
            name: "Resident Evil Requiem",
            // Image resolved via ImageCache from GitHub Assets repo
            installPath: "",
            engine: ""
        }
    }
    readonly property bool _isPlanned: (_heroGame.steamAppId || "") === _plannedAppId

    // Background image
    Image {
        id: heroImg
        anchors.fill: parent
        source: ImageCache.resolve(
            root._heroGame.steamAppId || root._heroGame.id || ""
        )
        sourceSize: Qt.size(600, 400)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: false

        Connections {
            target: ImageCache
            function onImageReady(readyId) {
                var myId = root._heroGame.steamAppId || root._heroGame.id || ""
                if (readyId === myId)
                    heroImg.source = ImageCache.resolve(myId)
            }
        }
    }

    // Mask for rounded corners
    Item {
        id: heroMask
        anchors.fill: parent
        visible: false
        layer.enabled: heroImg.status === Image.Ready
        Rectangle { anchors.fill: parent; radius: Dimensions.radiusSection; color: "white" }
    }

    MultiEffect {
        anchors.fill: heroImg
        source: heroImg
        maskEnabled: true
        maskSource: heroMask
        visible: heroImg.status === Image.Ready
    }

    // Placeholder
    Rectangle {
        anchors.fill: parent
        visible: heroImg.status !== Image.Ready
        radius: Dimensions.radiusSection
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.15) }
            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.surface, 0.8) }
        }
    }

    // Dark gradient overlay
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.withAlpha("#000000", 0.2) }
            GradientStop { position: 1.0; color: Theme.withAlpha("#000000", 0.75) }
        }
    }

    // TR badge (top-right) — visible on hover only
    TurkishFlagBadge {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Dimensions.marginBase
        anchors.rightMargin: Dimensions.marginBase
        flagWidth: 22; flagHeight: 14
        z: 2
        opacity: heroMa.containsMouse ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
    }

    // Game name (bottom-left)
    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Dimensions.marginMD
        anchors.rightMargin: Dimensions.marginMD
        anchors.bottomMargin: Dimensions.marginBase
        text: root._heroGame ? (root._heroGame.name || "") : ""
        font.pixelSize: Dimensions.fontSM
        font.weight: Font.Bold
        color: "white"
        elide: Text.ElideRight
        maximumLineCount: 2
        wrapMode: Text.WordWrap
    }

    // Top edge glass highlight
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.rightMargin: 1; anchors.topMargin: 1
        height: 1; radius: Dimensions.radiusSection; z: 5
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.2; color: Qt.rgba(1, 1, 1, 0.08) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.14) }
            GradientStop { position: 0.8; color: Qt.rgba(1, 1, 1, 0.08) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    MouseArea {
        id: heroMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root._heroGame) {
                var g = root._heroGame
                root.gameClicked(g.id || "", g.name || "", g.installPath || "", g.engine || "")
            }
        }
    }
}
