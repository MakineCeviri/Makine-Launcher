import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * BackgroundBlur.qml - Native Qt BackgroundBlur birebir port
 * Kaynak: ui/src/widgets/backgroundblur.cpp
 *
 * Features:
 * - Blurred background image
 * - Fade animation
 * - Gradient overlay
 */
Item {
    id: root

    property string imageSource: ""
    property real blurRadius: 30
    property real blurOpacity: 0.0

    // Fade animations
    function fadeIn() {
        fadeAnim.to = 1.0
        fadeAnim.start()
    }

    function fadeOut() {
        fadeAnim.to = 0.0
        fadeAnim.start()
    }

    NumberAnimation {
        id: fadeAnim
        target: root
        property: "blurOpacity"
        duration: 500
        easing.type: Easing.OutCubic
    }

    // Background image
    Image {
        id: bgImage
        anchors.fill: parent
        source: root.imageSource
        fillMode: Image.PreserveAspectCrop
        visible: false
    }

    // Blurred layer
    MultiEffect {
        id: blurEffect
        anchors.fill: bgImage
        source: bgImage
        blurEnabled: true
        blur: root.blurRadius / 100.0
        blurMax: 64
        opacity: root.blurOpacity
        visible: root.imageSource !== ""

        Behavior on opacity { NumberAnimation { duration: 500 } }
    }

    // Gradient overlay - Native Qt: bg 70%, bg 95%, bg 100%
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bgPrimary, 0.7) }
            GradientStop { position: 0.5; color: Theme.withAlpha(Theme.bgPrimary, 0.95) }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }
    }
}
