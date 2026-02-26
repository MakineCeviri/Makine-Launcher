import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * GameCardSkeleton.qml - Loading placeholder for GameCard
 *
 * Matches GameCard dimensions (140x200) with shimmer animation.
 * Use this while game data is loading.
 *
 * Usage:
 *   GameCardSkeleton {
 *       animationDelay: index * 100  // Staggered loading effect
 *   }
 */
Item {
    id: root

    // Animation configuration
    property bool animationsEnabled: true
    property int animationDelay: 0  // For staggered effect

    // Match GameCard dimensions
    width: 140
    height: 200

    // Shadow placeholder
    Rectangle {
        anchors.fill: parent
        anchors.margins: -4
        anchors.bottomMargin: -8
        radius: Dimensions.cardBorderRadius
        color: Theme.withAlpha(Theme.bgPrimary, 0.15)
        z: -1
    }

    // Main card container
    Rectangle {
        id: cardContainer
        anchors.fill: parent
        radius: Dimensions.cardBorderRadius
        color: Theme.surfaceLight
        clip: true

        // Image area skeleton
        SkeletonLoader {
            id: imageSkeleton
            anchors.fill: parent
            skeletonRadius: 4
            animationsEnabled: root.animationsEnabled
            animationDelay: root.animationDelay
            baseColor: Theme.withAlpha(Theme.textPrimary, 0.04)
        }

        // Gradient overlay (matches GameCard)
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.5; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.7) }
            }
        }

        // Badge placeholder (top-right)
        SkeletonLoader {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: Dimensions.marginSM
            anchors.rightMargin: Dimensions.marginSM
            width: 22
            height: 22
            skeletonRadius: 4
            animationsEnabled: root.animationsEnabled
            animationDelay: root.animationDelay + 50
            baseColor: Theme.withAlpha(Theme.textPrimary, 0.08)
        }

        // Title area at bottom
        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Dimensions.marginBase
            spacing: Dimensions.spacingSM

            // Title line 1
            SkeletonLoader {
                width: parent.width * 0.85
                height: 12
                skeletonRadius: 2
                animationsEnabled: root.animationsEnabled
                animationDelay: root.animationDelay + 100
                baseColor: Theme.withAlpha(Theme.textPrimary, 0.15)
            }

            // Title line 2 (shorter)
            SkeletonLoader {
                width: parent.width * 0.6
                height: 10
                skeletonRadius: 2
                animationsEnabled: root.animationsEnabled
                animationDelay: root.animationDelay + 150
                baseColor: Theme.withAlpha(Theme.textPrimary, 0.1)
            }
        }
    }
}
