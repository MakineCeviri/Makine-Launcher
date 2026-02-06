import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * GameCard.qml - Oyun kartı komponenti
 *
 * Özellikler:
 * - Hover scale 1.05 + glow
 * - Yumuşak köşeler (radius 8)
 * - Birleşik TR+✓ badge
 * - Gradient overlay
 * - Hover "Detay" button
 */
Item {
    id: root

    // PUBLIC PROPERTIES
    property string gameId: ""
    property string gameName: ""
    property string imageUrl: ""
    property bool verified: false
    property bool translated: false

    // SIZE - Dimensions'dan al
    width: Dimensions.cardWidth
    height: Dimensions.cardHeight

    signal clicked()

    // HOVER STATE
    readonly property bool isHovered: mouseArea.containsMouse

    // HOVER SCALE
    scale: isHovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // Glow for hover - simplified single glow
    Rectangle {
        id: glowRect
        anchors.fill: cardContent
        anchors.margins: -12
        radius: 12
        color: Theme.primary
        opacity: root.isHovered ? 0.15 : 0
        z: -1

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    // CARD CONTENT
    Rectangle {
        id: cardContent
        anchors.fill: parent
        radius: 8  // Yumuşak köşeler
        color: Theme.surfaceLight

        // Mask item for rounded corners - must be separate and have layer.enabled
        Item {
            id: imageMask
            anchors.fill: parent
            visible: false
            layer.enabled: true

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "white"
            }
        }

        // IMAGE LAYER - with rounded corners via mask
        Image {
            id: gameImage
            anchors.fill: parent
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: false  // Hide original, show masked version
        }

        // Masked image with rounded corners
        MultiEffect {
            anchors.fill: gameImage
            source: gameImage
            maskEnabled: true
            maskSource: imageMask
            visible: gameImage.status === Image.Ready
        }

        // Loading placeholder
        Rectangle {
            anchors.fill: parent
            visible: gameImage.status === Image.Loading
            radius: 8
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
            radius: 8
            color: Theme.surface

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }
        }

        // GRADIENT OVERLAY - daha yumuşak
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.4; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.85) }
            }
        }

        // COMBINED BADGE (TR + ✓) - tek badge sağ üstte
        Rectangle {
            visible: root.translated || root.verified
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.rightMargin: 8
            width: badgeContent.width + 10
            height: 22
            radius: 11
            color: Qt.rgba(0, 0, 0, 0.75)

            Row {
                id: badgeContent
                anchors.centerIn: parent
                spacing: 4

                // TR badge
                Rectangle {
                    visible: root.translated
                    width: 22
                    height: 16
                    radius: 4
                    color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "TR"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: "white"
                    }
                }

                // Verified checkmark
                Rectangle {
                    visible: root.verified
                    width: 16
                    height: 16
                    radius: 8
                    color: Theme.primary
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: "white"
                    }
                }
            }
        }

        // GAME NAME - 2 satıra kadar göster
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            height: nameText.height + 2

            Text {
                id: nameText
                anchors.bottom: parent.bottom
                width: parent.width
                text: root.gameName
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: "white"
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight

                // Tooltip for truncated names
                ToolTip {
                    visible: root.isHovered && nameText.truncated
                    delay: 400
                    text: root.gameName
                    font.pixelSize: 12

                    background: Rectangle {
                        color: Qt.rgba(0.1, 0.1, 0.1, 0.95)
                        radius: 4
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                    }
                }
            }
        }

        // HOVER OVERLAY
        Rectangle {
            anchors.fill: parent
            radius: 8
            visible: root.isHovered
            opacity: root.isHovered ? 1.0 : 0.0

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 0.84, 0.0, 0.1) }
                GradientStop { position: 1.0; color: Qt.rgba(1.0, 0.41, 0.71, 0.15) }
            }

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            // "Detay" button
            Rectangle {
                anchors.centerIn: parent
                width: detayText.width + 28
                height: detayText.height + 14
                radius: 6

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.splashGold }
                    GradientStop { position: 1.0; color: Theme.pink }
                }

                Text {
                    id: detayText
                    anchors.centerIn: parent
                    text: "Detay"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: "white"
                }
            }
        }

        // Hover border glow
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"
            border.color: root.isHovered ? Theme.withAlpha(Theme.primary, 0.5) : "transparent"
            border.width: 2

            Behavior on border.color {
                ColorAnimation { duration: 200 }
            }
        }
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
