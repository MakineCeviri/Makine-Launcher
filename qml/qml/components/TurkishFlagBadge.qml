import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

// Scalable Turkish flag badge (crescent + rotating star, Canvas-rendered for crisp small sizes)
Rectangle {
    id: flagRoot
    property real flagWidth: 22
    property real flagHeight: 14
    // External controllers (e.g. window state) can pause the spin to save GPU/CPU
    property bool starSpinning: true
    property int starSpinDurationMs: 6000

    width: flagWidth; height: flagHeight
    radius: Dimensions.badgeRadius
    color: Theme.turkishRed

    // Crescent layer (static)
    Canvas {
        id: crescentLayer
        anchors.fill: parent
        onWidthChanged: Qt.callLater(requestPaint)
        onHeightChanged: Qt.callLater(requestPaint)

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

    // Star layer — wrapped in an Item that rotates in-place via RotationAnimator (GPU thread)
    Item {
        id: starWrap

        // Geometry shared with crescent so the star sits next to the inner circle
        readonly property real outerR: flagRoot.height * 0.36
        readonly property real outerCx: flagRoot.width * 0.42
        readonly property real gap: Math.max(1.5, outerR * 0.30)
        readonly property real innerR: outerR - Math.max(1.2, outerR * 0.24)
        readonly property real innerCx: outerCx + gap
        readonly property real starR: Math.max(1.5, flagRoot.height * 0.15)
        readonly property real starCx: innerCx + innerR + starR + Math.max(0.5, gap * 0.3)
        readonly property real starCy: flagRoot.height / 2
        // 1px padding on each side so anti-aliased edges are not clipped during rotation
        readonly property real boxSize: starR * 2 + 2

        x: starCx - boxSize / 2
        y: starCy - boxSize / 2
        width: boxSize
        height: boxSize
        transformOrigin: Item.Center

        Canvas {
            id: starCanvas
            anchors.fill: parent
            onWidthChanged: Qt.callLater(requestPaint)
            onHeightChanged: Qt.callLater(requestPaint)

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var localCx = width / 2
                var localCy = height / 2
                var r1 = starWrap.starR

                ctx.fillStyle = "white"
                ctx.beginPath()
                for (var i = 0; i < 10; i++) {
                    var angle = (i * Math.PI / 5) - Math.PI / 2
                    var r = i % 2 === 0 ? r1 : r1 * 0.38
                    var x = localCx + Math.cos(angle) * r
                    var y = localCy + Math.sin(angle) * r
                    if (i === 0) ctx.moveTo(x, y)
                    else ctx.lineTo(x, y)
                }
                ctx.closePath()
                ctx.fill()
            }
        }

        // GPU-thread rotation — does not block the UI thread, no Canvas redraw required
        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: flagRoot.starSpinDurationMs
            loops: Animation.Infinite
            running: flagRoot.visible && flagRoot.starSpinning
        }
    }
}
