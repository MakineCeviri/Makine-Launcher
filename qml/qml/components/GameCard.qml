import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * GameCard.qml - Flutter game_card.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/game_card.dart
 *
 * Library style portrait kart (140x200)
 *
 * Ozellikler:
 * - Hover scale 1.05
 * - Hover glow (pink + gold)
 * - Gradient overlay
 * - Verified badge
 * - Hover "Detay" button
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter GameCard birebir
    // =========================================================================

    /// Oyun ID'si
    property string gameId: ""

    /// Oyun adi
    property string gameName: ""

    /// Oyun resmi URL
    property string imageUrl: ""

    /// Onaylanmis mi - Flutter: game.isVerified
    property bool isVerified: false

    // =========================================================================
    // SIZE - Flutter: width 140, height 200
    // =========================================================================

    width: 140
    height: 200

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // HOVER SCALE - Flutter: Matrix4 scale 1.05
    // =========================================================================

    scale: isHovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // =========================================================================
    // SHADOW LAYERS - Flutter: BoxShadow style
    // Hover: pink (0.2 alpha, blur 6, offset 0,4) + gold (0.1 alpha, blur 8, offset 0,2)
    // Default: black (0.2 alpha, blur 6, offset 0,3)
    // =========================================================================

    // Pink glow source (invisible) - uses Theme.pink
    Rectangle {
        id: pinkGlowSource
        anchors.fill: cardContent
        anchors.margins: -6  // blur radius
        anchors.topMargin: -2  // offset.y = 4, so -6+4 = -2
        anchors.bottomMargin: -10  // -6-4 = -10
        radius: 8
        color: Theme.pink
        visible: false
        layer.enabled: true
    }

    // Pink glow with blur - HOVER ONLY
    MultiEffect {
        anchors.fill: pinkGlowSource
        source: pinkGlowSource
        blurEnabled: true
        blur: 0.3  // Softer blur
        blurMax: 24
        opacity: root.isHovered ? 0.25 : 0  // Flutter: 0.2 alpha
        z: -1

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // Gold glow source (invisible) - uses Theme.splashGold
    Rectangle {
        id: goldGlowSource
        anchors.fill: cardContent
        anchors.margins: -8  // blur radius
        anchors.topMargin: -6  // offset.y = 2, so -8+2 = -6
        anchors.bottomMargin: -10  // -8-2 = -10
        radius: 10
        color: Theme.splashGold
        visible: false
        layer.enabled: true
    }

    // Gold glow with blur - HOVER ONLY
    MultiEffect {
        anchors.fill: goldGlowSource
        source: goldGlowSource
        blurEnabled: true
        blur: 0.35  // Softer blur
        blurMax: 32
        opacity: root.isHovered ? 0.15 : 0  // Flutter: 0.1 alpha
        z: -2

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // Default black shadow (always visible)
    Rectangle {
        id: blackShadowSource
        anchors.fill: cardContent
        anchors.margins: -6  // blur radius
        anchors.topMargin: -3  // offset.y = 3, so -6+3 = -3
        anchors.bottomMargin: -9  // -6-3 = -9
        radius: 8
        color: "black"
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: blackShadowSource
        source: blackShadowSource
        blurEnabled: true
        blur: 0.3
        blurMax: 24
        opacity: root.isHovered ? 0.1 : 0.2  // Fade when hovered (colored shadows take over)
        z: -3

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // =========================================================================
    // CARD CONTENT
    // =========================================================================

    Rectangle {
        id: cardContent
        anchors.fill: parent
        radius: 4  // Flutter: borderRadius 4
        clip: true
        color: Theme.surfaceLight

        // =====================================================================
        // IMAGE LAYER - Flutter: CachedNetworkImage
        // =====================================================================

        Image {
            id: gameImage
            anchors.fill: parent
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true

            // Loading placeholder
            Rectangle {
                anchors.fill: parent
                visible: gameImage.status === Image.Loading
                color: Theme.surfaceLight

                // Loading indicator
                BusyIndicator {
                    anchors.centerIn: parent
                    running: gameImage.status === Image.Loading
                    width: 24
                    height: 24
                }
            }

            // Error fallback - Flutter: errorWidget
            Rectangle {
                anchors.fill: parent
                visible: gameImage.status === Image.Error || root.imageUrl === ""

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.surface }
                    GradientStop { position: 1.0; color: Theme.surfaceLight }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    // Game icon - Flutter: Icons.games, size 32
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\uD83C\uDFAE"  // Game emoji
                        font.pixelSize: 32
                        color: Theme.textMuted
                    }

                    // Game name - Flutter: fontSize 11, w500, maxLines 2
                    Text {
                        width: cardContent.width - 16
                        text: root.gameName
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // =====================================================================
        // GRADIENT OVERLAY - Flutter: LinearGradient stops [0.5, 1.0]
        // =====================================================================

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.5; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.9) }
            }
        }

        // =====================================================================
        // VERIFIED BADGE - Flutter: top 8, right 8
        // =====================================================================

        Rectangle {
            visible: root.isVerified
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.rightMargin: 8
            width: 22
            height: 22
            radius: 4
            color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.9)

            // Checkmark icon - Flutter: Icons.verified, size 14
            Text {
                anchors.centerIn: parent
                text: "\u2713"  // Checkmark
                font.pixelSize: 14
                font.weight: Font.Bold
                color: "white"
            }
        }

        // =====================================================================
        // CONTENT - Flutter: left 10, right 10, bottom 10
        // =====================================================================

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            height: nameText.height + 4

            // Game name - Flutter: white, fontSize 13, w600
            Text {
                id: nameText
                anchors.bottom: parent.bottom
                width: parent.width
                text: root.gameName
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: "white"
                elide: Text.ElideRight
            }
        }

        // =====================================================================
        // HOVER OVERLAY - Flutter: gradient gold 8% → pink 12%
        // =====================================================================

        Rectangle {
            anchors.fill: parent
            visible: root.isHovered
            opacity: root.isHovered ? 1.0 : 0.0

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 0.84, 0.0, 0.08) }  // gold 8%
                GradientStop { position: 1.0; color: Qt.rgba(1.0, 0.41, 0.71, 0.12) } // pink 12%
            }

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            // "Detay" button - Flutter: padding h16 v8, gradient gold → pink
            Rectangle {
                anchors.centerIn: parent
                width: detayText.width + 32
                height: detayText.height + 16
                radius: 4

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
    }

    // =========================================================================
    // MOUSE AREA
    // =========================================================================

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
