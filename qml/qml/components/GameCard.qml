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

    // SIZE
    width: Dimensions.cardWidth
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
            layer.enabled: true

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
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
            asynchronous: true
            cache: true
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

        // Bottom gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.4; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.85) }
            }
        }

        // COMBINED BADGE (TR + ✓)
        Row {
            visible: root.translated || root.verified
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: Dimensions.marginBase
            anchors.rightMargin: Dimensions.marginBase
            spacing: Dimensions.spacingXS

            Rectangle {
                visible: root.translated
                width: 22
                height: 16
                radius: Dimensions.badgeRadius
                color: Theme.turkishRed

                Text {
                    anchors.centerIn: parent
                    text: "TR"
                    font.pixelSize: Dimensions.fontMini
                    font.weight: Font.Bold
                    color: Theme.textOnColor
                }
            }

            Rectangle {
                visible: root.verified
                width: 16
                height: 16
                radius: 8
                color: Theme.primary

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Bold
                    color: Theme.textOnColor
                }
            }
        }


        // GAME NAME
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Dimensions.marginBase
            anchors.rightMargin: Dimensions.marginBase
            anchors.bottomMargin: Dimensions.marginBase
            height: nameText.height + 2

            Text {
                id: nameText
                anchors.bottom: parent.bottom
                width: parent.width
                text: root.gameName
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.DemiBold
                color: Theme.textPrimary
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
        }
    }

    // Focus indicator
    Rectangle {
        anchors.fill: cardContent
        anchors.margins: -2
        radius: cardContent.radius + 2
        color: "transparent"
        border.color: Theme.withAlpha(Theme.primary, 0.6)
        border.width: 2
        visible: root.activeFocus
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
