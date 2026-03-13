import QtQuick
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * AppBackground.qml - macOS-inspired ambient background.
 * Two soft color orbs floating in dark space — minimal, elegant, professional.
 * Repaints only on accent change or window resize.
 */
Canvas {
    id: bg

    readonly property color _c1: Theme.accentBase
    readonly property color _c2: Theme.accentLight
    readonly property color _c4: Theme.accentDark

    on_C1Changed: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    renderStrategy: Canvas.Cooperative

    function _rgba(r, g, b, a) {
        return "rgba(" + r + "," + g + "," + b + "," + a + ")"
    }

    onPaint: {
        var ctx = getContext("2d")
        var w = width, h = height
        ctx.clearRect(0, 0, w, h)
        if (w <= 0 || h <= 0) return

        var dim = Math.max(w, h)

        var r1 = Math.round(_c1.r * 255), g1 = Math.round(_c1.g * 255), b1 = Math.round(_c1.b * 255)
        var r2 = Math.round(_c2.r * 255), g2 = Math.round(_c2.g * 255), b2 = Math.round(_c2.b * 255)
        var r4 = Math.round(_c4.r * 255), g4 = Math.round(_c4.g * 255), b4 = Math.round(_c4.b * 255)

        // Blend tones for seamless transitions
        var mr = Math.round((r1 + r4) / 2), mg = Math.round((g1 + g4) / 2), mb = Math.round((b1 + b4) / 2)

        // ── Orb 1: Upper-right — primary accent, wide and ethereal ──
        var orb1 = ctx.createRadialGradient(w * 0.72, h * 0.18, 0, w * 0.72, h * 0.18, dim * 0.85)
        orb1.addColorStop(0.00, _rgba(r2, g2, b2, 0.14))
        orb1.addColorStop(0.10, _rgba(r1, g1, b1, 0.10))
        orb1.addColorStop(0.25, _rgba(mr, mg, mb, 0.06))
        orb1.addColorStop(0.45, _rgba(r4, g4, b4, 0.025))
        orb1.addColorStop(0.70, _rgba(r4, g4, b4, 0.008))
        orb1.addColorStop(1.00, "transparent")
        ctx.fillStyle = orb1
        ctx.fillRect(0, 0, w, h)

        // ── Orb 2: Lower-left — deeper accent, complementary position ──
        var orb2 = ctx.createRadialGradient(w * 0.22, h * 0.78, 0, w * 0.22, h * 0.78, dim * 0.75)
        orb2.addColorStop(0.00, _rgba(r4, g4, b4, 0.12))
        orb2.addColorStop(0.12, _rgba(mr, mg, mb, 0.08))
        orb2.addColorStop(0.28, _rgba(r4, g4, b4, 0.04))
        orb2.addColorStop(0.50, _rgba(r4, g4, b4, 0.015))
        orb2.addColorStop(0.75, _rgba(r4, g4, b4, 0.005))
        orb2.addColorStop(1.00, "transparent")
        ctx.fillStyle = orb2
        ctx.fillRect(0, 0, w, h)

        // ── Subtle vignette — barely there, just enough depth ──
        var vig = ctx.createRadialGradient(w * 0.50, h * 0.46, dim * 0.30, w * 0.50, h * 0.46, dim * 1.0)
        vig.addColorStop(0.00, "transparent")
        vig.addColorStop(0.60, "transparent")
        vig.addColorStop(0.80, _rgba(0, 0, 0, 0.06))
        vig.addColorStop(1.00, _rgba(0, 0, 0, 0.14))
        ctx.fillStyle = vig
        ctx.fillRect(0, 0, w, h)
    }
}
