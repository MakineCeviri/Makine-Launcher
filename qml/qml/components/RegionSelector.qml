import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * Full-screen overlay for selecting a screen region via rubber-band drag.
 * Emits regionSelected(x, y, width, height) when the user completes selection.
 */
Item {
    id: regionSelector
    anchors.fill: parent
    visible: false
    z: 9999

    signal regionSelected(int rx, int ry, int rw, int rh)
    signal cancelled()

    property bool selecting: false
    property point startPos: Qt.point(0, 0)
    property rect selectionRect: Qt.rect(0, 0, 0, 0)

    function show() {
        visible = true
        selecting = false
        selectionRect = Qt.rect(0, 0, 0, 0)
    }

    function hide() {
        visible = false
        selecting = false
    }

    // Semi-transparent dark overlay
    Rectangle {
        anchors.fill: parent
        color: "#88000000"
    }

    // Selection rectangle
    Rectangle {
        id: selBox
        x: selectionRect.x
        y: selectionRect.y
        width: selectionRect.width
        height: selectionRect.height
        color: "transparent"
        visible: regionSelector.selecting || selectionRect.width > 0

        border.color: Theme.primary
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
                width: 8; height: 8
                radius: 4
                color: Theme.primary
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
            color: Theme.primary
            visible: selBox.width > 10 && selBox.height > 10

            Text {
                id: dimText
                anchors.centerIn: parent
                text: Math.round(selBox.width) + " \u00d7 " + Math.round(selBox.height)
                font.pixelSize: 12
                font.weight: Font.Medium
                color: Theme.textOnColor
                textFormat: Text.PlainText
            }
        }
    }

    // Instruction text
    Text {
        anchors.centerIn: parent
        visible: !regionSelector.selecting && regionSelector.selectionRect.width === 0
        text: qsTr("Fare ile s\u00fcr\u00fckleyerek OCR b\u00f6lgesi se\u00e7in\nESC ile iptal")
        font.pixelSize: 16
        font.weight: Font.Medium
        color: "#ffffff"
        horizontalAlignment: Text.AlignHCenter
        textFormat: Text.PlainText
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.CrossCursor

        onPressed: function(mouse) {
            regionSelector.selecting = true
            regionSelector.startPos = Qt.point(mouse.x, mouse.y)
            regionSelector.selectionRect = Qt.rect(mouse.x, mouse.y, 0, 0)
        }

        onPositionChanged: function(mouse) {
            if (!regionSelector.selecting) return
            var x = Math.min(regionSelector.startPos.x, mouse.x)
            var y = Math.min(regionSelector.startPos.y, mouse.y)
            var w = Math.abs(mouse.x - regionSelector.startPos.x)
            var h = Math.abs(mouse.y - regionSelector.startPos.y)
            regionSelector.selectionRect = Qt.rect(x, y, w, h)
        }

        onReleased: function(mouse) {
            regionSelector.selecting = false
            if (regionSelector.selectionRect.width > 10 && regionSelector.selectionRect.height > 10) {
                regionSelector.regionSelected(
                    Math.round(regionSelector.selectionRect.x),
                    Math.round(regionSelector.selectionRect.y),
                    Math.round(regionSelector.selectionRect.width),
                    Math.round(regionSelector.selectionRect.height))
                regionSelector.hide()
            }
        }
    }

    Keys.onEscapePressed: {
        regionSelector.cancelled()
        regionSelector.hide()
    }

    onVisibleChanged: {
        if (visible) forceActiveFocus()
    }
}
