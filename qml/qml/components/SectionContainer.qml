import QtQuick
import QtQuick.Layouts
import "../theme"

/**
 * SectionContainer.qml - Reusable glassmorphic section wrapper
 *
 * Provides a dark rounded container with subtle border and ambient glow.
 * All section headers should use this for visual consistency.
 */
Rectangle {
    id: container

    // Content goes here via default property
    default property alias content: contentLayout.data

    // Glow origin: "top-right" or "bottom-left"
    property string glowPosition: "top-right"

    // Expose inner layout for external anchoring/sizing
    property alias contentLayout: contentLayout
    property alias contentSpacing: contentLayout.spacing

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding

    readonly property int _padding: 20
    readonly property int _radius: Dimensions.radiusSection

    radius: _radius
    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1
    clip: false

    // Ambient accent glow
    Canvas {
        anchors.fill: parent
        property color glowColor: Theme.accentDark
        onGlowColorChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (width <= 0 || height <= 0) return

            var cr = container._radius
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

            var cx, cy
            if (container.glowPosition === "bottom-center") {
                cx = width / 2; cy = height - 20
            } else if (container.glowPosition === "bottom-left") {
                cx = 40; cy = height - 30
            } else {
                cx = width - 40; cy = 30
            }
            var r = Math.max(width, height) * 0.55
            var gc = glowColor
            var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
            var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
            grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + ",0.12)")
            grad.addColorStop(0.3, "rgba(" + R + "," + G + "," + B + ",0.06)")
            grad.addColorStop(0.6, "rgba(" + R + "," + G + "," + B + ",0.02)")
            grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
            ctx.fillStyle = grad
            ctx.fillRect(0, 0, width, height)
        }
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: container._padding
        spacing: 8
    }
}
