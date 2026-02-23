import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MakineAI 1.0

/**
 * GameCard.qml - Oyun kartı komponenti
 */
Item {
    id: root

    // PUBLIC PROPERTIES
    property string gameId: ""
    property string gameName: ""
    property string imageUrl: ""
    property string steamAppId: ""
    property string installPath: ""
    property bool verified: false
    property bool translated: false
    property bool hasUpdate: false

    // Settle guard — skip decode for cards that scroll past within 60ms
    property bool _settled: false

    Timer {
        id: _settleTimer
        interval: 60; repeat: false
        onTriggered: root._settled = true
    }
    Component.onCompleted: _settleTimer.start()

    Connections {
        target: ImageCache
        function onImageReady(readyId) {
            var myId = root.steamAppId || root.gameId
            if (readyId === myId)
                root.imageUrl = ImageCache.resolve(myId, "")
        }
    }

    // Delegate recycling support (reuseItems)
    ListView.onPooled: _settleTimer.stop()

    // SIZE — responsive, preserves aspect ratio (130:185)
    readonly property real _aspectRatio: 130.0 / 185.0
    width: Math.round(height * _aspectRatio)
    height: Dimensions.cardHeight

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.gameName
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    // HOVER STATE
    readonly property bool isHovered: mouseArea.containsMouse

    // Hover: subtle scale only
    transform: Scale {
        origin.x: root.width / 2; origin.y: root.height / 2
        xScale: root.isHovered ? 1.02 : 1.0; yScale: root.isHovered ? 1.02 : 1.0
        Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    // Ambient glow — single Canvas, repaints on hover state change
    Canvas {
        anchors.fill: cardContent; z: 1
        property color glowColor: Theme.accentBase
        property bool hovered: root.isHovered
        onGlowColorChanged: requestPaint()
        onHoveredChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var cr = Dimensions.cardBorderRadius
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
            var spread = hovered ? 0.6 : 0.55
            var grad = ctx.createRadialGradient(8, height - 4, 0, 8, height - 4, Math.max(width, height) * spread)
            grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + "," + a0 + ")")
            grad.addColorStop(0.25, "rgba(" + R + "," + G + "," + B + "," + a1 + ")")
            grad.addColorStop(0.5, "rgba(" + R + "," + G + "," + B + "," + a2 + ")")
            grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
            ctx.fillStyle = grad
            ctx.fillRect(0, 0, width, height)
        }
    }

    // CARD CONTENT
    Rectangle {
        id: cardContent
        anchors.fill: parent
        radius: Dimensions.cardBorderRadius
        color: Theme.surfaceLight
        clip: true

        // Mask for rounded corners
        Item {
            id: imageMask
            anchors.fill: parent
            visible: false
            layer.enabled: gameImage.status === Image.Ready

            Rectangle {
                anchors.fill: parent
                radius: Dimensions.cardBorderRadius
                color: "white"
            }
        }

        // Game image — sourceSize limits decoded resolution to 2x card size for HiDPI
        Image {
            id: gameImage
            anchors.fill: parent
            source: root._settled ? root.imageUrl : ""
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(260, 370)
            asynchronous: true
            cache: true
            retainWhileLoading: true
            visible: false
        }

        // Masked image with fade-in on load
        MultiEffect {
            id: maskedImage
            anchors.fill: gameImage
            source: gameImage
            maskEnabled: true
            maskSource: imageMask
            visible: gameImage.status === Image.Ready
            opacity: 0

            // Fade in when image loads
            states: State {
                name: "loaded"
                when: gameImage.status === Image.Ready
                PropertyChanges { target: maskedImage; opacity: 1 }
            }
            transitions: Transition {
                NumberAnimation { property: "opacity"; duration: Dimensions.fadeTransitionDuration; easing.type: Easing.OutCubic }
            }
        }

        // Loading placeholder
        Rectangle {
            anchors.fill: parent
            visible: gameImage.status === Image.Loading
            radius: Dimensions.cardBorderRadius
            color: Theme.surfaceLight

            BusyIndicator {
                anchors.centerIn: parent
                running: gameImage.status === Image.Loading
                width: 24
                height: 24
            }
        }

        // Error fallback
        Rectangle {
            anchors.fill: parent
            visible: gameImage.status === Image.Error || root.imageUrl === ""
            radius: Dimensions.cardBorderRadius
            color: Theme.surface

            Column {
                anchors.centerIn: parent
                spacing: Dimensions.spacingMD

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: Dimensions.fontHero
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }
        }

        // Integrity warning badge (top-right, always visible when unsafe)
        Rectangle {
            id: warningBadge
            visible: root.translated && (!root.verified || root.hasUpdate)
            anchors.top: parent.top; anchors.right: parent.right
            anchors.topMargin: Dimensions.marginBase; anchors.rightMargin: Dimensions.marginBase
            width: 18; height: 12; radius: Dimensions.badgeRadius
            color: root.hasUpdate ? Theme.warning : Theme.withAlpha(Theme.textMuted, 0.7)
            z: 2

            Text {
                anchors.centerIn: parent
                text: root.hasUpdate ? "\u0021" : "\u003F"
                font.pixelSize: 9; font.weight: Font.Bold
                color: Theme.textOnColor
            }

            ToolTip {
                visible: root.isHovered && warningBadge.visible
                delay: 300
                text: root.hasUpdate
                    ? qsTr("Oyun g\u00FCncellendi \u2014 yama etkilenmi\u015F olabilir")
                    : qsTr("Do\u011Frulanmam\u0131\u015F yama \u2014 dikkatli olun")
                font.pixelSize: Dimensions.fontSM
                background: Rectangle {
                    color: Theme.withAlpha(Theme.surface, 0.95)
                    radius: Dimensions.radiusStandard
                    border.color: Theme.withAlpha(
                        root.hasUpdate ? Theme.warning : Theme.textMuted, 0.3)
                }
            }
        }

        // Game name
        Text {
            id: nameText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Dimensions.marginBase
            anchors.rightMargin: Dimensions.marginBase
            anchors.bottomMargin: Dimensions.marginBase
            text: root.gameName
            font.pixelSize: Dimensions.fontCaption
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            style: Text.Raised
            styleColor: Qt.rgba(0, 0, 0, 0.7)
            maximumLineCount: 2
            wrapMode: Text.WordWrap
            elide: Text.ElideRight

            ToolTip {
                visible: root.isHovered && nameText.truncated
                delay: 250
                text: root.gameName
                font.pixelSize: Dimensions.fontSM

                background: Rectangle {
                    color: Theme.withAlpha(Theme.surface, 0.95)
                    radius: Dimensions.radiusStandard
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.1)
                }
            }
        }

        // TR badge — safe state only (verified + no update), hover to reveal
        TurkishFlagBadge {
            id: trBadgeBottom
            visible: root.translated && root.verified && !root.hasUpdate
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: Dimensions.marginBase
            anchors.bottomMargin: Dimensions.marginBase
            flagWidth: 18; flagHeight: 12
            opacity: root.isHovered ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
            }
        }
    }

    // Focus indicator
    FocusRing {
        target: root
        z: Dimensions.zBase
    }

    // MOUSE AREA
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
