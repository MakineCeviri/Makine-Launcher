import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * SkeletonLoader.qml - Loading skeleton with shimmer effect
 *
 * Displays a placeholder shape with animated shimmer and pulse effects
 * while content is loading. Industry-standard skeleton loading pattern.
 *
 * Features:
 * - Configurable shape (rectangle, circle, rounded)
 * - Animated shimmer overlay
 * - Subtle pulse animation on base color
 * - GPU-optimized with animationsEnabled flag
 * - Supports staggered animation delay for lists
 *
 * Usage:
 *   SkeletonLoader {
 *       width: 140
 *       height: 200
 *       skeletonRadius: 8
 *       animationDelay: index * 100  // For staggered effect
 *   }
 */
Rectangle {
    id: root

    // Shape configuration
    property int skeletonRadius: 4
    property bool circular: false

    // Animation configuration
    property bool animationsEnabled: true
    property int animationDuration: 1500
    property int animationDelay: 0  // For staggered animations in lists

    // Colors
    property color baseColor: Qt.rgba(1, 1, 1, 0.06)
    property color shimmerColor: Qt.rgba(1, 1, 1, 0.12)

    // Size
    implicitWidth: 100
    implicitHeight: 20

    radius: circular ? Math.min(width, height) / 2 : skeletonRadius
    clip: true

    // Shimmer animation phase
    property real shimmerPhase: 0.0

    SequentialAnimation on shimmerPhase {
        running: root.visible && root.animationsEnabled
        loops: Animation.Infinite

        // Initial delay for staggered effect
        PauseAnimation { duration: root.animationDelay }

        // Main shimmer cycle
        NumberAnimation {
            from: 0.0
            to: 1.0
            duration: root.animationDuration
            easing.type: Easing.InOutQuad
        }

        // Pause between cycles
        PauseAnimation { duration: 300 }
    }

    // Shimmer gradient overlay
    Rectangle {
        id: shimmerOverlay
        width: parent.width * 2
        height: parent.height
        x: -width + (shimmerPhase * (parent.width + width))
        radius: parent.radius

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: root.shimmerColor }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.18) }
            GradientStop { position: 0.7; color: root.shimmerColor }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Subtle pulse animation on base color
    property real pulseValue: 1.0

    SequentialAnimation on pulseValue {
        running: root.visible && root.animationsEnabled
        loops: Animation.Infinite

        NumberAnimation {
            to: 1.3
            duration: 800
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            to: 1.0
            duration: 800
            easing.type: Easing.InOutSine
        }
    }

    // Apply pulse to base color
    color: Qt.rgba(
        baseColor.r * pulseValue,
        baseColor.g * pulseValue,
        baseColor.b * pulseValue,
        baseColor.a
    )
}
