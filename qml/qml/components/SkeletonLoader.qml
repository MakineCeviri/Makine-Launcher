import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

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
    Accessible.ignored: true

    // Shape configuration
    property int skeletonRadius: 4
    property bool circular: false

    // Animation configuration
    property bool animationsEnabled: true
    property int animationDuration: 1500
    property int animationDelay: 0  // For staggered animations in lists

    // Colors
    property color baseColor: Theme.textPrimary06
    property color shimmerColor: Theme.textPrimary12

    // Size
    implicitWidth: 100
    implicitHeight: 20

    radius: circular ? Math.min(width, height) / 2 : skeletonRadius

    // Shimmer animation phase
    property real shimmerPhase: 0.0

    SequentialAnimation on shimmerPhase {
        running: root.visible && root.animationsEnabled
        loops: Animation.Infinite

        PauseAnimation { duration: root.animationDelay }

        NumberAnimation {
            from: 0.0; to: 1.0
            duration: root.animationDuration
            easing.type: Easing.InOutSine
        }

        PauseAnimation { duration: 600 }

        onRunningChanged: {
            if (typeof SceneProfiler !== "undefined")
                SceneProfiler.registerAnimation("skeletonShimmer", running)
        }
    }

    // Shimmer gradient overlay
    Rectangle {
        width: parent.width * 1.5
        height: parent.height
        x: -width + (shimmerPhase * (parent.width + width))
        radius: parent.radius

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.4; color: root.shimmerColor }
            GradientStop { position: 0.6; color: root.shimmerColor }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    color: baseColor
}
