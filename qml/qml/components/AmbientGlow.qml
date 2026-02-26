import QtQuick
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * AmbientGlow.qml - Reusable radial glow effect with rounded-rect clipping.
 * Supports hover mode (intensity changes on hover) and custom glow origin.
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

    // Repaint only when geometry or visual parameters actually change.
    // Hover state changes are handled via effectiveIntensity below.
    onGlowColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPositionChanged: requestPaint()

    // Smooth intensity transition on hover instead of instant repaint
    property real effectiveIntensity: intensity
    Behavior on effectiveIntensity {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }
    onHoveredChanged: effectiveIntensity = (hoveredIntensity >= 0 && hovered) ? hoveredIntensity : intensity
    onIntensityChanged: effectiveIntensity = (hoveredIntensity >= 0 && hovered) ? hoveredIntensity : intensity
    onEffectiveIntensityChanged: requestPaint()

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

        // Effective intensity (already interpolated by Behavior)
        var _i = effectiveIntensity

        // Radial gradient
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
