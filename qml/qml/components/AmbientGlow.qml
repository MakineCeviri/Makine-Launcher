import QtQuick
import MakineAI 1.0

/**
 * AmbientGlow.qml - Reusable radial glow effect with rounded-rect clipping.
 * Replaces duplicated Canvas glow in SectionContainer and HomePage.
 */
Canvas {
    id: glow

    property string position: "top-right"  // "top-right", "bottom-left", "bottom-center"
    property color glowColor: Theme.accentDark
    property real cornerRadius: Dimensions.radiusSection
    property real intensity: 0.12

    onGlowColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPositionChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (width <= 0 || height <= 0) return

        // Rounded-rect clip path
        var cr = cornerRadius
        ctx.beginPath()
        ctx.moveTo(cr, 0); ctx.lineTo(width - cr, 0)
        ctx.quadraticCurveTo(width, 0, width, cr)
        ctx.lineTo(width, height - cr)
        ctx.quadraticCurveTo(width, height, width - cr, height)
        ctx.lineTo(cr, height)
        ctx.quadraticCurveTo(0, height, 0, height - cr)
        ctx.lineTo(0, cr)
        ctx.quadraticCurveTo(0, 0, cr, 0)
        ctx.closePath(); ctx.clip()

        // Glow origin based on position
        var cx, cy
        if (position === "bottom-center") {
            cx = width / 2; cy = height - 20
        } else if (position === "bottom-left") {
            cx = 40; cy = height - 30
        } else {
            cx = width - 40; cy = 30
        }

        // Radial gradient
        var r = Math.max(width, height) * 0.55
        var gc = glowColor
        var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
        var i = intensity
        var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
        grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + "," + i + ")")
        grad.addColorStop(0.3, "rgba(" + R + "," + G + "," + B + "," + (i * 0.5) + ")")
        grad.addColorStop(0.6, "rgba(" + R + "," + G + "," + B + "," + (i * 0.17) + ")")
        grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
        ctx.fillStyle = grad
        ctx.fillRect(0, 0, width, height)
    }
}
