import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MakineAI 1.0

/**
 * Frameless transparent overlay window showing OCR translation results.
 * Stays on top of all windows. Auto-positions below the capture region.
 */
Window {
    id: overlayWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip | Qt.WindowTransparentForInput
    color: "transparent"
    visible: false
    width: 400
    height: contentCol.implicitHeight + 24

    property string translatedText: ""
    property string ocrText: ""
    property rect sourceRegion: Qt.rect(0, 0, 0, 0)

    // Position below the source region
    x: sourceRegion.x + (sourceRegion.width - width) / 2
    y: sourceRegion.y + sourceRegion.height + 12

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: 12
        color: "#E0181818"
        border.color: "#40ffffff"
        border.width: 1

        // Drop shadow layer
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            z: -1
            radius: 14
            color: "#40000000"
        }

        Column {
            id: contentCol
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Translation header
            Row {
                spacing: 6
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: "#4ade80"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: qsTr("\u00c7eviri")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: "#9ca3af"
                    textFormat: Text.PlainText
                }
            }

            // Translated text
            Text {
                width: parent.width
                text: overlayWindow.translatedText
                font.pixelSize: 15
                font.weight: Font.Medium
                color: "#f3f4f6"
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#30ffffff"
                visible: overlayWindow.ocrText.length > 0
            }

            // Original OCR text (smaller, muted)
            Text {
                width: parent.width
                text: overlayWindow.ocrText
                font.pixelSize: 11
                color: "#6b7280"
                wrapMode: Text.Wrap
                visible: text.length > 0
                textFormat: Text.PlainText
            }
        }
    }

    // Auto-hide after 10s of no updates
    Timer {
        id: hideTimer
        interval: 10000
        onTriggered: overlayWindow.visible = false
    }

    onTranslatedTextChanged: {
        if (translatedText.length > 0) {
            visible = true
            hideTimer.restart()
        }
    }
}
