import QtQuick
import MakineAI 1.0

/**
 * HorizontalGameStrip - Horizontal scrolling game card strip.
 * Built-in GameCard delegate, weighted drag, batched wheel scroll.
 * Usage: model + onGameClicked — that's it.
 * Set wrapAround: true for infinite circular scrolling (HomePage only).
 */
Item {
    id: strip

    property var model: []
    property alias count: view.count
    property real dragWeight: 0.35
    property bool wrapAround: false
    property bool _initialCentered: false

    // Scroll state for edge navigation
    readonly property bool canScrollLeft: !wrapAround && view.contentWidth > view.width
                                          && view.contentX > view.originX + 2
    readonly property bool canScrollRight: !wrapAround && view.contentWidth > view.width
                                           && view.contentX < view.contentWidth - view.width + view.originX - 2

    function scrollLeft() {
        var step = Dimensions.cardWidth + Dimensions.cardGap
        var target = Math.max(view.originX, view.contentX - step * 2)
        scrollAnim.to = target
        scrollAnim.restart()
    }

    function scrollRight() {
        var step = Dimensions.cardWidth + Dimensions.cardGap
        var maxX = view.contentWidth - view.width + view.originX
        var target = Math.min(maxX, view.contentX + step * 2)
        scrollAnim.to = target
        scrollAnim.restart()
    }

    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    // Re-center when model changes (search filtering, data reload)
    onModelChanged: _initialCentered = false

    // 3x repeated model for seamless infinite scroll
    readonly property var _viewModel: {
        var src = model
        if (!src || src.length === 0) return src || []
        if (!wrapAround || src.length < 2) return src
        return src.concat(src, src)
    }

    // Pixel width of one model copy (jump distance for wrap teleport)
    readonly property real _jumpWidth: {
        if (!wrapAround || view.contentWidth <= 0 || (model || []).length < 2) return 0
        return (view.contentWidth + view.spacing) / 3
    }

    // Silently teleport contentX to stay in the middle copy
    function _normalizeWrap() {
        if (_jumpWidth <= 0) return
        var jw = _jumpWidth
        if (view.contentX < jw * 0.3)
            view.contentX += jw
        else if (view.contentX > jw * 1.7)
            view.contentX -= jw
    }

    // Smooth scroll for arrow navigation
    NumberAnimation {
        id: scrollAnim
        target: view; property: "contentX"
        duration: 300; easing.type: Easing.OutCubic
    }

    ListView {
        id: view
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: Math.min(Dimensions.cardHeight, parent.height)
        orientation: ListView.Horizontal
        spacing: Dimensions.cardGap
        model: strip._viewModel
        clip: !strip.wrapAround
        interactive: false
        cacheBuffer: strip.wrapAround ? 200 : 150
        displayMarginBeginning: 0
        displayMarginEnd: 0
        pixelAligned: true
        reuseItems: true

        // Center the strip initially
        onContentWidthChanged: {
            if (!strip._initialCentered && contentWidth > width && count > 0) {
                if (strip.wrapAround && strip._jumpWidth > 0) {
                    // Start at center of the middle copy
                    var seg = strip._jumpWidth
                    contentX = seg + Math.max(0, (seg - width) / 2)
                } else {
                    contentX = (contentWidth - width) / 2
                }
                strip._initialCentered = true
            }
        }

        delegate: GameCard {
            required property var modelData
            required property int index
            height: ListView.view.height
            gameId: modelData.gameId || modelData.id || ""
            gameName: modelData.name || modelData.gameName || ""
            steamAppId: modelData.steamAppId || ""
            imageUrl: ImageCache.resolve(
                modelData.steamAppId || modelData.gameId || modelData.id || ""
            )
            installPath: modelData.installPath || ""
            verified: modelData.isVerified || false
            translated: modelData.hasTranslation || modelData.packageInstalled || false
            hasUpdate: modelData.hasUpdate || false
            onClicked: strip.gameClicked(
                modelData.gameId || modelData.id || "",
                modelData.name || modelData.gameName || "",
                modelData.installPath || "", modelData.engine || ""
            )
        }
    }

    // Weighted drag — click only fires if movement < 8px
    MouseArea {
        anchors.fill: view
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        property real _startX: 0
        property real _startContentX: 0
        property real _lastX: 0
        property real _releaseV: 0
        property real _lastTime: 0
        property bool _dragged: false

        onPressed: function(mouse) {
            momentumAnim.stop()
            _dragged = false
            _startX = mouse.x
            _startContentX = view.contentX
            _lastX = mouse.x
            _lastTime = Date.now()
            _releaseV = 0

            if (typeof FrameTimer !== "undefined")
                FrameTimer.beginInteraction("scrollStrip")
        }

        onPositionChanged: function(mouse) {
            if (!_dragged && Math.abs(mouse.x - _startX) > 8)
                _dragged = true
            if (!_dragged) return

            var newX = _startContentX - (mouse.x - _startX) * strip.dragWeight

            if (!strip.wrapAround) {
                var lo = view.originX
                var hi = Math.max(lo, view.contentWidth - view.width + lo)
                newX = Math.max(lo, Math.min(hi, newX))
            }

            view.contentX = newX
            if (strip.wrapAround) strip._normalizeWrap()

            var now = Date.now(), dt = now - _lastTime
            if (dt > 0) {
                _releaseV = (mouse.x - _lastX) / dt * 16
                _lastX = mouse.x
                _lastTime = now
            }
        }

        onReleased: {
            if (_dragged) {
                strip._velocity = _releaseV * strip.dragWeight
                if (Math.abs(strip._velocity) > 0.5) {
                    momentumAnim.start()
                } else {
                    if (typeof FrameTimer !== "undefined") FrameTimer.endInteraction()
                }
            } else {
                if (typeof FrameTimer !== "undefined") FrameTimer.endInteraction()
            }
        }

        onClicked: function(mouse) {
            if (_dragged) return
            var item = view.itemAt(view.contentX + mouse.x, mouse.y)
            if (item) item.clicked()
        }
    }

    // Momentum after drag release + wheel scroll
    property real _velocity: 0

    // Frame-synced momentum — fires exactly once per vsync, no timer drift
    FrameAnimation {
        id: momentumAnim
        running: false
        onTriggered: {
            // Frame-rate independent decay: 0.88 per 16ms baseline
            var dt = Math.min(frameTime, 0.05) // cap at 50ms to avoid spiral
            strip._velocity *= Math.pow(0.82, dt / 0.016)

            if (Math.abs(strip._velocity) < 0.3) {
                strip._velocity = 0
                if (typeof FrameTimer !== "undefined") FrameTimer.endInteraction()
                running = false
                return
            }

            // Scale displacement by actual frame time for consistent speed
            var displacement = strip._velocity * (dt / 0.016)
            var newX = view.contentX - displacement

            if (!strip.wrapAround) {
                var lo = view.originX
                var hi = Math.max(lo, view.contentWidth - view.width + lo)
                newX = Math.max(lo, Math.min(hi, newX))
            }

            view.contentX = newX
            if (strip.wrapAround) strip._normalizeWrap()
        }
    }

    WheelHandler {
        orientation: Qt.Vertical
        property real _prev: 0
        onRotationChanged: {
            if (typeof FrameTimer !== "undefined" && !momentumAnim.running)
                FrameTimer.beginInteraction("wheelScroll")
            strip._velocity += Math.max(-20, Math.min(20, rotation - _prev)) * 0.6
            _prev = rotation
            momentumAnim.restart()
        }
    }
}
