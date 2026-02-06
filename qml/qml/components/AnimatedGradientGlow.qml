import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * AnimatedGradientGlow.qml - Soft rotating rainbow glow
 *
 * A square gradient that rotates in place, heavily blurred
 * to create a dreamy, soft spinning light effect.
 */
Item {
    id: root

    property bool active: false
    property bool animationsEnabled: true

    opacity: root.active ? 0.35 : 0
    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

    // Rotation phase (0 → 1 = 0° → 360°)
    property real animPhase: 0
    NumberAnimation on animPhase {
        from: 0; to: 1
        duration: 8000
        loops: Animation.Infinite
        running: root.opacity > 0 && root.animationsEnabled
    }

    // Gradient source - rotating square, rendered to texture
    Item {
        id: glowSource
        anchors.fill: parent
        layer.enabled: root.opacity > 0
        visible: false

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(parent.width, parent.height) * 0.55
            height: width
            rotation: root.animPhase * 360
            transformOrigin: Item.Center

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.00; color: Theme.brandGold }
                GradientStop { position: 0.12; color: Theme.brandOrange }
                GradientStop { position: 0.25; color: Theme.brandCoral }
                GradientStop { position: 0.37; color: Theme.brandPurple }
                GradientStop { position: 0.50; color: Theme.brandBlue }
                GradientStop { position: 0.62; color: Theme.brandTeal }
                GradientStop { position: 0.75; color: Theme.brandGreen }
                GradientStop { position: 0.87; color: Theme.brandLime }
                GradientStop { position: 1.00; color: Theme.brandGold }
            }
        }
    }

    // Heavy blur for soft diffused glow
    MultiEffect {
        anchors.fill: parent
        source: glowSource
        blurEnabled: true
        blurMax: 64
        blur: 1.0
    }
}
