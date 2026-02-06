import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * ViewAllCard.qml - Flutter view_all_card.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/view_all_card.dart
 *
 * "Tumunu Gor" karti - animasyonlu gradient accent color
 *
 * Ozellikler:
 * - GameCard ile ayni boyut (140x200)
 * - Animasyonlu gold-olive accent color
 * - Plus icon circle
 * - Count display
 * - Hover scale ve glow
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter ViewAllCard birebir
    // =========================================================================

    /// Kalan oyun sayisi
    property int remainingCount: 0

    /// Animasyonlari devre disi birak
    property bool disableAnimations: false

    // =========================================================================
    // SIZE - Dimensions'dan al (GameCard ile ayni)
    // =========================================================================

    width: Dimensions.cardWidth
    height: Dimensions.cardHeight

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // ANIMATION - Flutter: 2000ms repeat
    // =========================================================================

    property real _animValue: 0.0

    NumberAnimation on _animValue {
        id: colorAnimation
        from: 0.0
        to: 1.0
        duration: 2000
        loops: Animation.Infinite
        running: !root.disableAnimations
    }

    // =========================================================================
    // ACCENT COLOR - Flutter: gold (#DDC66A) → olive (#759764) lerp
    // =========================================================================

    readonly property color accentColor: {
        var t = disableAnimations ? 0.0 : _animValue
        return Theme.lerpColor(Theme.gold, Theme.olive, t)
    }

    // =========================================================================
    // HOVER SCALE - Flutter: 1.05
    // =========================================================================

    scale: isHovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // =========================================================================
    // GLOW EFFECT - Flutter: hover ? accentColor alpha 0.3, blur 8, offset (0,8)
    // =========================================================================

    // Glow source (invisible)
    Rectangle {
        id: glowSource
        anchors.fill: cardContent
        anchors.margins: -8  // blur radius
        anchors.topMargin: 0  // offset.y = 8, so -8+8 = 0
        anchors.bottomMargin: -16  // -8-8 = -16
        radius: Dimensions.radiusStandard
        color: accentColor
        visible: false
        layer.enabled: true
    }

    // Real blur glow effect - Flutter exact values
    MultiEffect {
        anchors.fill: glowSource
        source: glowSource
        blurEnabled: true
        blur: 8 / 32  // blur 8 normalized
        blurMax: 24
        opacity: root.isHovered ? 0.3 : 0  // Flutter: alpha 0.3
        z: -1

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
        radius: Dimensions.radiusStandard

        // Background - subtle dark bg for consistency with GameCards
        color: root.isHovered
            ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.85)
            : Qt.rgba(0.08, 0.08, 0.08, 0.6)

        // Border - enhanced visibility
        border.color: root.isHovered
            ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.6)
            : Qt.rgba(1, 1, 1, 0.15)
        border.width: root.isHovered ? 2 : 1

        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on border.width { NumberAnimation { duration: 200 } }

        // =====================================================================
        // CONTENT COLUMN
        // =====================================================================

        Column {
            anchors.centerIn: parent
            spacing: 0

            // Plus Icon Circle - Flutter: 50x50
            Rectangle {
                id: plusCircle
                anchors.horizontalCenter: parent.horizontalCenter
                width: 50
                height: 50
                radius: 25
                color: root.isHovered
                    ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
                    : "transparent"
                border.color: root.isHovered
                    ? accentColor
                    : Qt.rgba(1, 1, 1, 0.3)
                border.width: 2

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                // Plus icon - Flutter: Icons.add_rounded, size 28
                Text {
                    anchors.centerIn: parent
                    text: "+"
                    font.pixelSize: 28
                    font.weight: Font.Normal
                    color: root.isHovered
                        ? accentColor
                        : Qt.rgba(1, 1, 1, 0.5)

                    Behavior on color { ColorAnimation { duration: 200 } }
                }
            }

            // Spacer - Flutter: SizedBox(height: 16)
            Item { width: 1; height: 16 }

            // Count - Flutter: '+${count}', fontSize 24, bold
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "+" + root.remainingCount
                font.pixelSize: 24
                font.weight: Font.Bold
                color: root.isHovered ? accentColor : Theme.textPrimary

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            // Spacer - Flutter: SizedBox(height: 4)
            Item { width: 1; height: 4 }

            // "daha fazla" - Flutter: fontSize 13, w500
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "daha fazla"
                font.pixelSize: 13
                font.weight: Font.Medium
                color: root.isHovered
                    ? Qt.rgba(1, 1, 1, 0.8)
                    : Theme.textSecondary

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            // Spacer - Flutter: SizedBox(height: 12)
            Item { width: 1; height: 12 }

            // "Tümünü Gör" button - Flutter: padding h12 v6, fontSize 11, w600
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: viewAllText.width + 24
                height: viewAllText.height + 12
                radius: Dimensions.radiusStandard

                color: root.isHovered
                    ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                    : Qt.rgba(1, 1, 1, 0.08)
                border.color: root.isHovered
                    ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.4)
                    : Qt.rgba(1, 1, 1, 0.1)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                Text {
                    id: viewAllText
                    anchors.centerIn: parent
                    text: "T\u00FCm\u00FCn\u00FC G\u00F6r"  // Tümünü Gör
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: root.isHovered ? accentColor : Theme.textSecondary

                    Behavior on color { ColorAnimation { duration: 200 } }
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
