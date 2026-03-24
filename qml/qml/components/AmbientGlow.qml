import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AmbientGlow.qml - Reusable radial glow effect with rounded-rect clipping.
 * Supports hover mode (intensity changes on hover) and custom glow origin.
 *
 * Performance: Canvas paints once at full intensity; hover transitions use
 * opacity animation instead of repeated Canvas repaints (~9 draws → 0).
 */
Canvas {
    id: glow

    property string position: "top-right"  // "top-right", "bottom-left", "bottom-center"
    property color glowColor: Theme.accentDark
    property real cornerRadius: Dimensions.radiusSection
    property real intensity: 0.12

    // Hover support — set hoveredIntensity >= 0 to enable hover mode
    property bool hovered: false
    property real hoveredIntensity: -1  // -1 = no hover mode

    // Custom origin — set >= 0 to override position-based origin
    property real originX: -1
    property real originY: -1

    // Gradient spread multiplier
    property real spread: 0.55

    renderTarget: Canvas.FramebufferObject
    renderStrategy: Canvas.Cooperative

    // Paint once at max intensity; hover transitions via opacity (GPU-composited, no repaint)
    readonly property real _paintIntensity: hoveredIntensity >= 0 ? Math.max(intensity, hoveredIntensity) : intensity
    opacity: (hoveredIntensity >= 0)
             ? (hovered ? hoveredIntensity / _paintIntensity : intensity / _paintIntensity)
             : 1.0
    Behavior on opacity {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    // Repaint only when geometry or visual parameters actually change (not on hover)
    onGlowColorChanged: Qt.callLater(requestPaint)
    onWidthChanged: Qt.callLater(requestPaint)
    onHeightChanged: Qt.callLater(requestPaint)
    onPositionChanged: Qt.callLater(requestPaint)
    on_PaintIntensityChanged: Qt.callLater(requestPaint)

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

        // Glow origin
        var cx, cy
        if (originX >= 0) {
            cx = originX
        } else if (position === "bottom-center") {
            cx = width / 2
        } else if (position === "bottom-left" || position === "top-left") {
            cx = 40
        } else {
            cx = width - 40
        }
        if (originY >= 0) {
            cy = originY
        } else if (position === "bottom-center" || position === "bottom-left") {
            cy = height - 20
        } else {
            cy = 30
        }

        // Radial gradient at max intensity (opacity handles hover transitions)
        var _i = _paintIntensity
        var r = Math.max(width, height) * spread
        var gc = glowColor
        var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
        var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
        grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + "," + _i + ")")
        grad.addColorStop(0.25, "rgba(" + R + "," + G + "," + B + "," + (_i * 0.5) + ")")
        grad.addColorStop(0.5, "rgba(" + R + "," + G + "," + B + "," + (_i * 0.2) + ")")
        grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
        ctx.fillStyle = grad
        ctx.fillRect(0, 0, width, height)
    }
}
