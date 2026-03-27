import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

// Scalable Turkish flag badge (crescent only, Canvas-rendered for crisp small sizes)
Rectangle {
    id: flagRoot
    property real flagWidth: 22
    property real flagHeight: 14

    width: flagWidth; height: flagHeight
    radius: Dimensions.badgeRadius
    color: Theme.turkishRed

    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w = width, h = height, cy = h / 2
            var bgColor = flagRoot.color

            // Crescent: white outer circle, red inner cutout
            var outerR = h * 0.36
            var outerCx = w * 0.42
            var gap = Math.max(1.5, outerR * 0.30)
            var innerR = outerR - Math.max(1.2, outerR * 0.24)
            var innerCx = outerCx + gap

            ctx.fillStyle = "white"
            ctx.beginPath()
            ctx.arc(outerCx, cy, outerR, 0, Math.PI * 2)
            ctx.fill()

            ctx.fillStyle = bgColor
            ctx.beginPath()
            ctx.arc(innerCx, cy, innerR, 0, Math.PI * 2)
            ctx.fill()
        }
    }
}
