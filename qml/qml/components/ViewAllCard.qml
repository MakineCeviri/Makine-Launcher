import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * ViewAllCard.qml - "View All" card with animated gradient accent
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    property int remainingCount: 0

    /// Disable animations for GPU optimization
    property bool disableAnimations: false

    // =========================================================================
    // SIZE
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
    // ANIMATION
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
    // ACCENT COLOR
    // =========================================================================

    readonly property color accentColor: {
        var t = disableAnimations ? 0.0 : _animValue
        return Theme.lerpColor(Theme.gold, Theme.olive, t)
    }

    // =========================================================================
    // HOVER SCALE
    // =========================================================================

    scale: isHovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // =========================================================================
    // GLOW EFFECT
    // =========================================================================

    Rectangle {
        id: glowSource
        anchors.fill: cardContent
        anchors.margins: -8
        anchors.topMargin: 0
        anchors.bottomMargin: -16
        radius: Dimensions.radiusStandard
        color: accentColor
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: glowSource
        source: glowSource
        blurEnabled: true
        blur: 8 / 32
        blurMax: 24
        opacity: root.isHovered ? 0.3 : 0
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

        color: root.isHovered
            ? Theme.withAlpha(Theme.surface, 0.85)
            : Qt.rgba(0.08, 0.08, 0.08, 0.6)

        border.color: root.isHovered
            ? Theme.withAlpha(accentColor, 0.6)
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

            Rectangle {
                id: plusCircle
                anchors.horizontalCenter: parent.horizontalCenter
                width: 50
                height: 50
                radius: 25
                color: root.isHovered
                    ? Theme.withAlpha(accentColor, 0.15)
                    : "transparent"
                border.color: root.isHovered
                    ? accentColor
                    : Qt.rgba(1, 1, 1, 0.3)
                border.width: 2

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

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

            Item { width: 1; height: 16 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "+" + root.remainingCount
                font.pixelSize: 24
                font.weight: Font.Bold
                color: root.isHovered ? accentColor : Theme.textPrimary

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            Item { width: 1; height: 4 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("daha fazla")
                font.pixelSize: 13
                font.weight: Font.Medium
                color: root.isHovered
                    ? Qt.rgba(1, 1, 1, 0.8)
                    : Theme.textSecondary

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            Item { width: 1; height: 12 }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: viewAllText.width + 24
                height: viewAllText.height + 12
                radius: Dimensions.radiusStandard

                color: root.isHovered
                    ? Theme.withAlpha(accentColor, 0.2)
                    : Qt.rgba(1, 1, 1, 0.08)
                border.color: root.isHovered
                    ? Theme.withAlpha(accentColor, 0.4)
                    : Qt.rgba(1, 1, 1, 0.1)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                Text {
                    id: viewAllText
                    anchors.centerIn: parent
                    text: qsTr("Tümünü Gör")
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

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("View all games")
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    // Focus indicator
    Rectangle {
        anchors.fill: cardContent
        anchors.margins: -2
        radius: cardContent.radius + 2
        color: "transparent"
        border.color: Theme.withAlpha(Theme.primary, 0.6)
        border.width: 2
        visible: root.activeFocus
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
