import QtQuick
import MakineAI 1.0

/**
 * GradientBorderCanvas - Animated rainbow gradient border for cards
 *
 * Shared by GameCard and PatchCard. Renders a rounded-rect stroke
 * with rotating gradient colors from Theme.brandGradient.
 */
Canvas {
    id: root

    property real phase: 0
    property bool hovered: false
    property real borderRadius: Dimensions.cardBorderRadius
    property real borderWidth: 1.5

    onPhaseChanged: if (hovered) requestPaint()
    onHoveredChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        var angle = phase * Math.PI * 2
        var cx = width / 2, cy = height / 2
        var len = Math.max(width, height) * 0.7
        var x1 = cx + Math.cos(angle) * len
        var y1 = cy + Math.sin(angle) * len
        var x2 = cx - Math.cos(angle) * len
        var y2 = cy - Math.sin(angle) * len

        var grad = ctx.createLinearGradient(x1, y1, x2, y2)
        var colors = Theme.brandGradient
        for (var i = 0; i < colors.length; i++)
            grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

        var bw = root.borderWidth
        var r = root.borderRadius - bw / 2
        var px = bw / 2, py = bw / 2
        var w = width - bw, h = height - bw

        ctx.beginPath()
        ctx.moveTo(px + r, py)
        ctx.lineTo(px + w - r, py)
        ctx.arcTo(px + w, py, px + w, py + r, r)
        ctx.lineTo(px + w, py + h - r)
        ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
        ctx.lineTo(px + r, py + h)
        ctx.arcTo(px, py + h, px, py + h - r, r)
        ctx.lineTo(px, py + r)
        ctx.arcTo(px, py, px + r, py, r)
        ctx.closePath()

        ctx.strokeStyle = grad
        ctx.lineWidth = bw
        ctx.globalAlpha = hovered ? 0.8 : 0.0
        ctx.stroke()
    }
}
