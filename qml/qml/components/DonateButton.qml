import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * DonateButton.qml - Animated gradient donate button with glow effect
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    /// Theme mode
    property bool isDark: true

    /// Disable animations for GPU optimization
    property bool disableAnimations: false

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // ACCESSIBILITY
    // =========================================================================

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Donate")
    Accessible.onPressAction: clicked()
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    // =========================================================================
    // SIZE
    // =========================================================================

    implicitWidth: contentRow.width + 24
    implicitHeight: contentRow.height + 16

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

    // Smooth sinusoidal transition (0→1→0)
    readonly property real smoothValue: {
        if (root.disableAnimations) return 0.0
        var v = _animValue * 2 - 1  // -1 to 1
        return 1 - Math.abs(v)  // 0 to 1 to 0 (smooth)
    }

    // =========================================================================
    // COLOR INTERPOLATION
    // =========================================================================

    readonly property color color1: Theme.lerpColor(Theme.gold, Theme.olive, smoothValue)
    readonly property color color2: Theme.lerpColor(Theme.olive, Theme.gold, smoothValue)

    readonly property real glowIntensity: 0.25 + (smoothValue * 0.25)

    // =========================================================================
    // GLOW
    // =========================================================================

    Rectangle {
        anchors.centerIn: buttonBg
        width: buttonBg.width + (isHovered ? 40 : (20 + smoothValue * 12))
        height: buttonBg.height + (isHovered ? 40 : (20 + smoothValue * 12))
        anchors.verticalCenterOffset: 4
        radius: Dimensions.radiusStandard + 10
        color: Theme.withAlpha(color1, isHovered ? 0.6 : glowIntensity)
        z: -1

        Behavior on width { NumberAnimation { duration: 200 } }
        Behavior on height { NumberAnimation { duration: 200 } }
        Behavior on color { ColorAnimation { duration: 200 } }
    }

    // =========================================================================
    // BUTTON BACKGROUND
    // =========================================================================

    Rectangle {
        id: buttonBg
        anchors.fill: parent
        radius: Dimensions.radiusStandard

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.color1 }
            GradientStop { position: 1.0; color: root.color2 }
        }
    }

    // =========================================================================
    // CONTENT
    // =========================================================================

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8
        Text {
            text: "\u2764"
            font.pixelSize: 18
            color: "white"
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: "Destekçi Ol"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "white"
            anchors.verticalCenter: parent.verticalCenter
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
