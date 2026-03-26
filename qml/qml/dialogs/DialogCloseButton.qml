import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * DialogCloseButton.qml - Reusable 28x28 close (X) button for dialog headers
 *
 * Usage:
 *   DialogCloseButton { onClicked: { root.cancelled(); root.close() } }
 */
Rectangle {
    id: btn

    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: 14
    color: _mouse.containsMouse ? Theme.textPrimary08 : "transparent"

    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    Canvas {
        anchors.centerIn: parent
        width: 10; height: 10
        property bool hov: _mouse.containsMouse
        onHovChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = hov ? Theme.textPrimary : Theme.textMuted
            ctx.lineWidth = 1.5; ctx.lineCap = "round"
            ctx.beginPath(); ctx.moveTo(1, 1); ctx.lineTo(9, 9); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(9, 1); ctx.lineTo(1, 9); ctx.stroke()
        }
    }

    MouseArea {
        id: _mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}
