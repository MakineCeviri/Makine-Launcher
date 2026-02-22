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

    // Ambient glow — single Canvas, repaints on hover state change
    Canvas {
        anchors.fill: parent; z: 1
        property color glowColor: Theme.accentBase
        property bool hovered: heroMa.containsMouse
        onGlowColorChanged: requestPaint()
        onHoveredChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var cr = Dimensions.radiusSection
            ctx.beginPath()
            ctx.moveTo(cr, 0); ctx.lineTo(width - cr, 0)
            ctx.quadraticCurveTo(width, 0, width, cr)
            ctx.lineTo(width, height - cr)
            ctx.quadraticCurveTo(width, height, width - cr, height)
            ctx.lineTo(cr, height)
            ctx.quadraticCurveTo(0, height, 0, height - cr)
            ctx.lineTo(0, cr)
            ctx.quadraticCurveTo(0, 0, cr, 0)
            ctx.closePath(); ctx.clip()
            var gc = glowColor
            var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
            var a0 = hovered ? 0.50 : 0.22
            var a1 = hovered ? 0.25 : 0.10
            var a2 = hovered ? 0.10 : 0.04
            var spread = hovered ? 0.55 : 0.5
            var grad = ctx.createRadialGradient(30, height - 20, 0, 30, height - 20, Math.max(width, height) * spread)
            grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + "," + a0 + ")")
            grad.addColorStop(0.25, "rgba(" + R + "," + G + "," + B + "," + a1 + ")")
            grad.addColorStop(0.5, "rgba(" + R + "," + G + "," + B + "," + a2 + ")")
            grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
            ctx.fillStyle = grad
            ctx.fillRect(0, 0, width, height)
        }
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
            headerImageUrl: "https://cdn.akamai.steamstatic.com/steam/apps/" + _plannedAppId + "/header.jpg",
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
            root._heroGame.steamAppId || root._heroGame.id || "",
            root._heroGame.headerImageUrl || ""
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
                    heroImg.source = ImageCache.resolve(myId, root._heroGame.headerImageUrl || "")
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

    // TR badge (top-right)
    TurkishFlagBadge {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Dimensions.marginBase
        anchors.rightMargin: Dimensions.marginBase
        flagWidth: 22; flagHeight: 14
        z: 2
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
