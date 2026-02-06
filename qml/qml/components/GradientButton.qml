import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * GradientButton.qml - Native Qt AnimatedGradientButton birebir port
 * Kaynak: ui/src/widgets/animatedgradientbutton.cpp
 *
 * Features:
 * - 2000ms infinite gradient animation (gold <-> olive)
 * - smoothValue calculation for smooth transitions
 * - Dynamic glow effect based on animation
 * - Hover enhancement
 */
Rectangle {
    id: root

    property string text: "Button"
    property string icon: ""
    property bool enabled: true
    property bool animationsEnabled: true

    signal clicked()

    implicitWidth: buttonContent.width + 48
    implicitHeight: 48  // Native Qt: sizeHint 200x48
    radius: Dimensions.radiusXS  // 4

    // Native Qt: 2000ms infinite animation loop
    property real animValue: 0.0
    NumberAnimation on animValue {
        running: root.animationsEnabled && root.enabled
        from: 0.0
        to: 1.0
        duration: 2000
        loops: Animation.Infinite
    }

    // Native Qt: smoothValue = (1 - ((animValue * 2 - 1).abs())).clamp(0.0, 1.0)
    property real smoothValue: animationsEnabled ? (1.0 - Math.abs(animValue * 2.0 - 1.0)) : 0.0

    // Native Qt: color1/color2 gold <-> olive lerp
    property color color1: Theme.lerpColor(Theme.gold, Theme.olive, smoothValue)
    property color color2: Theme.lerpColor(Theme.olive, Theme.gold, smoothValue)

    // Native Qt: glowIntensity = 0.3 + (smoothValue * 0.2), hover -> 0.6
    property real glowIntensity: mouseArea.containsMouse ? 0.6 : (0.3 + smoothValue * 0.2)

    // Native Qt: blurRadius = hover ? 24 : (16 + smoothValue * 8)
    property real blurRadius: mouseArea.containsMouse ? 24.0 : (16.0 + smoothValue * 8.0)

    // Native Qt: Animated gradient
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: root.color1 }
        GradientStop { position: 1.0; color: root.color2 }
    }

    scale: mouseArea.pressed ? 0.97 : (mouseArea.containsMouse && enabled ? 1.03 : 1.0)
    opacity: enabled ? 1.0 : 0.5

    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    // Native Qt: Shadow/glow effect
    Rectangle {
        anchors.fill: parent
        anchors.margins: -(root.blurRadius / 4)
        anchors.topMargin: anchors.margins + 4  // offset (0, 4)
        radius: parent.radius + root.blurRadius / 6
        color: Theme.withAlpha(root.color1, root.glowIntensity)
        z: -1
    }

    Row {
        id: buttonContent
        anchors.centerIn: parent
        spacing: 8

        // Icon (optional) - Native Qt: size 20
        Item {
            visible: root.icon !== ""
            width: 20
            height: 20
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: root.icon
                font.pixelSize: 16
                color: "white"
            }
        }

        // Text - Native Qt: fontSize 14, w600
        Text {
            text: root.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "white"
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.enabled) root.clicked()
    }
}
