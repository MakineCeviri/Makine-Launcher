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
                GradientStop { position: 0.00; color: "#FCCD66" }
                GradientStop { position: 0.12; color: "#F7AE76" }
                GradientStop { position: 0.25; color: "#EE968F" }
                GradientStop { position: 0.37; color: "#CC9FD8" }
                GradientStop { position: 0.50; color: "#90C2E6" }
                GradientStop { position: 0.62; color: "#77DBC8" }
                GradientStop { position: 0.75; color: "#80E59D" }
                GradientStop { position: 0.87; color: "#C8EB7C" }
                GradientStop { position: 1.00; color: "#FCCD66" }
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
