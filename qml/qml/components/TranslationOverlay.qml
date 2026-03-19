import QtQuick
import QtQuick.Window
import MakineLauncher 1.0

/**
 * Minimal frameless overlay — just translated text, nothing else.
 * Configurable text color, size, background opacity.
 */
Window {
    id: overlayWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip | Qt.WindowTransparentForInput
    color: "transparent"
    visible: false
    width: Math.max(200, textItem.implicitWidth + 28)
    height: textItem.implicitHeight + 20

    property string translatedText: ""
    property string ocrText: ""
    property rect sourceRegion: Qt.rect(0, 0, 0, 0)

    // Configurable from plugin settings
    property color textColor: "#f3f4f6"
    property int textSize: 15
    property color bgColor: "#CC1a1a1a"

    x: sourceRegion.x + (sourceRegion.width - width) / 2
    y: sourceRegion.y + sourceRegion.height + 6

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: overlayWindow.bgColor

        Text {
            id: textItem
            anchors.centerIn: parent
            width: Math.min(implicitWidth, 480)
            text: overlayWindow.translatedText
            font.pixelSize: overlayWindow.textSize
            font.weight: Font.Medium
            color: overlayWindow.textColor
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    Timer {
        id: hideTimer
        interval: 15000
        onTriggered: overlayWindow.visible = false
    }

    onTranslatedTextChanged: {
        if (translatedText.length > 0) {
            visible = true
            hideTimer.restart()
        }
    }
}
