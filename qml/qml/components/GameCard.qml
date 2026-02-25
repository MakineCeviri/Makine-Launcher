import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * GameCard.qml - Oyun kartı komponenti
 * No MultiEffect — pure scene graph compositing for smooth scroll.
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

    // Settle guard — skip decode for cards that scroll past within 120ms
    property bool _settled: false

    Timer {
        id: _settleTimer
        interval: 120; repeat: false
        onTriggered: root._settled = true
    }
    Component.onCompleted: _settleTimer.start()

    Connections {
        // Disconnect once image is loaded — avoids O(N*M) signal dispatch
        target: (gameImage.status !== Image.Ready && root._settled) ? ImageCache : null
        function onImageReady(readyId) {
            var myId = root.steamAppId || root.gameId
            if (readyId === myId)
                root.imageUrl = ImageCache.resolve(myId)
        }
    }

    // Delegate recycling support (reuseItems)
    ListView.onPooled: { _settleTimer.stop(); _settled = false }
    ListView.onReused: _settleTimer.start()

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

    // Ambient glow — deferred until card settles (skip during fast scroll)
    AmbientGlow {
        anchors.fill: cardContent; z: 1
        visible: root._settled
        glowColor: Theme.accentBase
        cornerRadius: Dimensions.cardBorderRadius
        originX: 8; originY: height - 4
        intensity: 0.22; hoveredIntensity: 0.50; spread: 0.55
        hovered: root.isHovered
    }

    // CARD CONTENT — image already has baked rounded corners (no FBO needed)
    Rectangle {
        id: cardContent
        anchors.fill: parent
        radius: Dimensions.cardBorderRadius
        color: Theme.surfaceLight
        clip: true

        // Game image
        Image {
            id: gameImage
            anchors.fill: parent
            source: root._settled ? root.imageUrl : ""
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(260, 370)
            asynchronous: true
            cache: true
            retainWhileLoading: true
            opacity: 0

            Behavior on opacity {
                NumberAnimation { duration: Dimensions.fadeTransitionDuration; easing.type: Easing.OutCubic }
            }

            onStatusChanged: {
                if (status === Image.Ready)
                    opacity = 1
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

        // Integrity warning badge
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

            Loader {
                active: root.isHovered && warningBadge.visible
                sourceComponent: ToolTip {
                    visible: true
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

            Loader {
                active: root.isHovered && nameText.truncated
                sourceComponent: ToolTip {
                    visible: true
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
        }

        // TR badge — safe state only
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
