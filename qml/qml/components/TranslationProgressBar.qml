import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * TranslationProgressBar.qml - Animated progress bar with glow effects
 * Flutter equivalent: LinearProgressIndicator with custom gradient
 *
 * Features:
 * - Animated gradient (gold → olive → pastel blue)
 * - Glow tip effect that follows progress
 * - Shimmer animation overlay
 * - Pulse animation on value change
 * - GPU-optimized with animationsEnabled flag
 */
Item {
    id: root

    property real value: 0.0  // 0.0 to 1.0
    property bool indeterminate: false
    property bool animationsEnabled: true
    property int animationDuration: 3000
    property real barHeight: 12

    implicitHeight: barHeight + 20  // Extra space for glow

    // Animation phase for gradient cycling
    property real animPhase: 0.0

    NumberAnimation on animPhase {
        from: 0.0
        to: 1.0
        duration: root.animationDuration
        loops: Animation.Infinite
        running: root.animationsEnabled && root.value < 1.0
    }

    // Smooth sinusoidal transition
    property real smoothValue: 0.5 + 0.5 * Math.sin(animPhase * Math.PI * 2)

    // Animated gradient colors
    property color color1: Qt.rgba(
        Theme.gold.r * (1 - smoothValue) + Theme.olive.r * smoothValue,
        Theme.gold.g * (1 - smoothValue) + Theme.olive.g * smoothValue,
        Theme.gold.b * (1 - smoothValue) + Theme.olive.b * smoothValue,
        1
    )

    property color color2: Qt.rgba(
        Theme.olive.r * (1 - smoothValue) + Theme.pastelBlue.r * smoothValue,
        Theme.olive.g * (1 - smoothValue) + Theme.pastelBlue.g * smoothValue,
        Theme.olive.b * (1 - smoothValue) + Theme.pastelBlue.b * smoothValue,
        1
    )

    property color color3: Qt.rgba(
        Theme.pastelBlue.r * (1 - smoothValue) + Theme.gold.r * smoothValue,
        Theme.pastelBlue.g * (1 - smoothValue) + Theme.gold.g * smoothValue,
        Theme.pastelBlue.b * (1 - smoothValue) + Theme.gold.b * smoothValue,
        1
    )

    // Background track
    Rectangle {
        id: track
        anchors.centerIn: parent
        width: parent.width
        height: root.barHeight
        radius: height / 2
        color: Qt.rgba(1, 1, 1, 0.05)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        // Subtle inner shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.1) }
                GradientStop { position: 0.3; color: "transparent" }
            }
        }
    }

    // Progress fill with gradient
    Rectangle {
        id: progressFill
        anchors.verticalCenter: parent.verticalCenter
        x: 0
        width: root.indeterminate ?
            parent.width * 0.3 :
            Math.max(root.barHeight, parent.width * root.value)
        height: root.barHeight
        radius: height / 2
        clip: true

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.color1 }
            GradientStop { position: 0.5; color: root.color2 }
            GradientStop { position: 1.0; color: root.color3 }
        }

        Behavior on width {
            enabled: !root.indeterminate
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        // Indeterminate animation
        SequentialAnimation on x {
            running: root.indeterminate && root.animationsEnabled
            loops: Animation.Infinite
            NumberAnimation {
                from: -progressFill.width
                to: root.width
                duration: 1500
                easing.type: Easing.InOutQuad
            }
        }

        // Shimmer overlay
        Rectangle {
            id: shimmer
            width: parent.width * 0.4
            height: parent.height
            radius: parent.radius
            visible: root.animationsEnabled && root.value < 1.0

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.3) }
                GradientStop { position: 1.0; color: "transparent" }
            }

            SequentialAnimation on x {
                running: root.animationsEnabled && root.value < 1.0
                loops: Animation.Infinite
                NumberAnimation {
                    from: -shimmer.width
                    to: progressFill.width
                    duration: 1500
                    easing.type: Easing.InOutQuad
                }
                PauseAnimation { duration: 500 }
            }
        }

        // Top highlight
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height * 0.4
            radius: parent.radius

            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.25) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // Glow effect at progress tip
    Rectangle {
        id: glowTip
        visible: root.animationsEnabled && root.value > 0 && root.value < 1.0
        anchors.verticalCenter: parent.verticalCenter
        x: progressFill.width - width / 2
        width: 24
        height: root.barHeight + 8
        radius: height / 2
        color: "transparent"

        // Glow layers
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 1.5
            height: parent.height * 1.5
            radius: height / 2
            color: Qt.rgba(root.color2.r, root.color2.g, root.color2.b, 0.3)
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            radius: height / 2
            color: Qt.rgba(root.color2.r, root.color2.g, root.color2.b, 0.5)
        }

        // Pulse animation
        SequentialAnimation on scale {
            running: root.animationsEnabled && root.value > 0 && root.value < 1.0
            loops: Animation.Infinite
            NumberAnimation { to: 1.2; duration: 600; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
        }

        Behavior on x {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
    }

    // Completion effect
    Rectangle {
        anchors.fill: progressFill
        radius: progressFill.radius
        visible: root.value >= 1.0
        color: "transparent"
        border.color: Theme.success
        border.width: 2
        opacity: root.value >= 1.0 ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }

        // Success glow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.3)
            border.width: 3
            z: -1
        }
    }
}
