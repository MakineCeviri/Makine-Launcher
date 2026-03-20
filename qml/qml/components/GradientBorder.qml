import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GradientBorder — Apple-style premium gradient border overlay.
 *
 * Drop inside any rounded Rectangle to replace flat border with
 * a top-lit gradient stroke (bright top → dim bottom).
 * Parent must have border.width: 0.
 */
Canvas {
    id: root

    property real cornerRadius: Dimensions.radiusSection

    // Gradient stops — top is bright (overhead light), bottom is barely visible
    property color topColor: Qt.rgba(1, 1, 1, 0.12)
    property color midColor: Qt.rgba(1, 1, 1, 0.05)
    property color bottomColor: Qt.rgba(1, 1, 1, 0.02)

    anchors.fill: parent
    z: 100
    renderTarget: Canvas.FramebufferObject
    renderStrategy: Canvas.Cooperative

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        var r = root.cornerRadius
        var w = width, h = height

        // Inset by 0.5px for crisp 1px stroke
        var x0 = 0.5, y0 = 0.5
        var x1 = w - 0.5, y1 = h - 0.5
        r = Math.max(0, r - 0.5)

        ctx.beginPath()
        ctx.moveTo(x0 + r, y0)
        ctx.lineTo(x1 - r, y0)
        ctx.quadraticCurveTo(x1, y0, x1, y0 + r)
        ctx.lineTo(x1, y1 - r)
        ctx.quadraticCurveTo(x1, y1, x1 - r, y1)
        ctx.lineTo(x0 + r, y1)
        ctx.quadraticCurveTo(x0, y1, x0, y1 - r)
        ctx.lineTo(x0, y0 + r)
        ctx.quadraticCurveTo(x0, y0, x0 + r, y0)
        ctx.closePath()

        var grad = ctx.createLinearGradient(0, 0, 0, h)
        grad.addColorStop(0.0, root.topColor)
        grad.addColorStop(0.45, root.midColor)
        grad.addColorStop(1.0, root.bottomColor)

        ctx.strokeStyle = grad
        ctx.lineWidth = 1
        ctx.stroke()
    }

    onWidthChanged: Qt.callLater(requestPaint)
    onHeightChanged: Qt.callLater(requestPaint)
    onTopColorChanged: Qt.callLater(requestPaint)
    onMidColorChanged: Qt.callLater(requestPaint)
    onBottomColorChanged: Qt.callLater(requestPaint)
}
