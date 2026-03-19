import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MakineLauncher 1.0

/**
 * Full-screen transparent Window for selecting a screen region.
 * Covers the ENTIRE screen (not just the app window).
 */
Window {
    id: regionWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "#66000000"
    visible: false

    // Cover entire primary screen
    x: 0; y: 0
    width: Screen.width
    height: Screen.height

    signal regionSelected(int rx, int ry, int rw, int rh)
    signal cancelled()

    property bool selecting: false
    property point startPos: Qt.point(0, 0)
    property rect selectionRect: Qt.rect(0, 0, 0, 0)

    function show() {
        selecting = false
        selectionRect = Qt.rect(0, 0, 0, 0)
        visible = true
        raise()
        requestActivate()
    }

    function hide() {
        visible = false
        selecting = false
    }

    // Cut-out effect: darken everything except selected region
    Item {
        anchors.fill: parent

        // Selection rectangle (bright area)
        Rectangle {
            id: selBox
            x: selectionRect.x
            y: selectionRect.y
            width: selectionRect.width
            height: selectionRect.height
            color: "transparent"
            visible: regionWindow.selecting || selectionRect.width > 0

            border.color: "#60a5fa"
            border.width: 2

            // Corner handles
            Repeater {
                model: [
                    {"ax": 0, "ay": 0}, {"ax": 1, "ay": 0},
                    {"ax": 0, "ay": 1}, {"ax": 1, "ay": 1}
                ]
                Rectangle {
                    required property var modelData
                    x: modelData.ax * (selBox.width - 8)
                    y: modelData.ay * (selBox.height - 8)
                    width: 8; height: 8; radius: 4
                    color: "#60a5fa"
                }
            }

            // Dimension label
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: 8
                implicitWidth: dimText.implicitWidth + 16
                implicitHeight: 24
                radius: 4
                color: "#60a5fa"
                visible: selBox.width > 10 && selBox.height > 10

                Text {
                    id: dimText
                    anchors.centerIn: parent
                    text: Math.round(selBox.width) + " \u00d7 " + Math.round(selBox.height)
                    font.pixelSize: 12; font.weight: Font.Medium
                    color: "#ffffff"
                    textFormat: Text.PlainText
                }
            }
        }

        // Instruction
        Text {
            anchors.centerIn: parent
            visible: !regionWindow.selecting && regionWindow.selectionRect.width === 0
            text: "Fare ile s\u00fcr\u00fckleyerek OCR b\u00f6lgesi se\u00e7in\nESC ile iptal"
            font.pixelSize: 18; font.weight: Font.Medium
            color: "#ffffff"
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor

            onPressed: function(mouse) {
                regionWindow.selecting = true
                regionWindow.startPos = Qt.point(mouse.x, mouse.y)
                regionWindow.selectionRect = Qt.rect(mouse.x, mouse.y, 0, 0)
            }

            onPositionChanged: function(mouse) {
                if (!regionWindow.selecting) return
                var sx = Math.min(regionWindow.startPos.x, mouse.x)
                var sy = Math.min(regionWindow.startPos.y, mouse.y)
                var sw = Math.abs(mouse.x - regionWindow.startPos.x)
                var sh = Math.abs(mouse.y - regionWindow.startPos.y)
                regionWindow.selectionRect = Qt.rect(sx, sy, sw, sh)
            }

            onReleased: {
                regionWindow.selecting = false
                if (regionWindow.selectionRect.width > 10 && regionWindow.selectionRect.height > 10) {
                    regionWindow.regionSelected(
                        Math.round(regionWindow.selectionRect.x),
                        Math.round(regionWindow.selectionRect.y),
                        Math.round(regionWindow.selectionRect.width),
                        Math.round(regionWindow.selectionRect.height))
                    regionWindow.hide()
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: { regionWindow.cancelled(); regionWindow.hide() }
    }
}
