import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HorizontalGameStrip - Horizontal scrolling game card strip.
 * Built-in GameCard delegate, weighted drag, batched wheel scroll.
 * Usage: model + onGameClicked — that's it.
 * Set wrapAround: true for infinite circular scrolling (HomePage only).
 *
 * When model is a CatalogProxyModel with wrapAround, the proxy handles 2x
 * doubling. When model is a JS array, this strip duplicates it internally.
 */
Item {
    id: strip

    property var model: []
    property alias count: view.count
    property real dragWeight: 0.35
    property bool wrapAround: false
    property bool largeCards: false
    property bool wheelEnabled: true
    property real driftSpeed: 0  // px/sec, negative=left, positive=right (wrapAround only)
    property bool _initialCentered: false

    // Is the model a C++ QAbstractItemModel (proxy)?
    readonly property bool _isProxyModel: model && typeof model.rowCount === "function"

    // For JS array models: build 2x view model internally
    property var _viewModel: []
    property bool _wrapReady: false

    function _updateViewModel() {
        if (_isProxyModel) return // proxy handles doubling
        var src = model
        if (!src || src.length === 0) { _viewModel = src || []; return }
        if (!wrapAround || src.length < 2 || !_wrapReady) { _viewModel = src; return }
        _viewModel = src.concat(src)
    }

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

    // Re-center when model changes
    onModelChanged: {
        _initialCentered = false
        if (!_isProxyModel) {
            _wrapReady = false
            _updateViewModel()
            if (wrapAround && model && model.length >= 2)
                _wrapActivation.restart()
        }
    }

    // Deferred wrap for JS array models
    Timer {
        id: _wrapActivation
        interval: 600
        onTriggered: {
            if (strip._isProxyModel || !strip.wrapAround || !strip.model || strip.model.length < 2)
                return
            strip._wrapReady = true
            Qt.callLater(function() {
                if (strip._jumpWidth > 0)
                    view.contentX = strip._jumpWidth - view.width / 2
            })
        }
    }

    onWrapAroundChanged: if (!_isProxyModel) _updateViewModel()
    on_WrapReadyChanged: if (!_isProxyModel) _updateViewModel()
    Component.onCompleted: if (!_isProxyModel) _updateViewModel()

    // Pixel width of one model copy (jump distance for wrap teleport)
    readonly property real _jumpWidth: {
        if (!wrapAround || view.contentWidth <= 0 || view.count <= 0) return 0
        if (_isProxyModel) {
            // Proxy model: count is already 2x, half is one copy
            var halfCount = Math.floor(view.count / 2)
            if (halfCount <= 0) return 0
            return halfCount * (Dimensions.cardWidth + Dimensions.cardGap)
        }
        // JS array: _viewModel is 2x
        if (!_wrapReady || !model || model.length < 2) return 0
        return (view.contentWidth + view.spacing) / 2
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
        clip: true
        readonly property int _minCardH: 100
        readonly property real _hoverScale: 1.03
        readonly property int _baseH: strip.largeCards
                    ? Math.min(parent.height, Math.round(Dimensions.cardHeight * 1.3))
                    : Math.max(_minCardH, Math.min(Dimensions.cardHeight, parent.height))
        height: Math.ceil(_baseH * _hoverScale)
        orientation: ListView.Horizontal
        spacing: Dimensions.cardGap
        model: strip._isProxyModel ? strip.model : strip._viewModel
        interactive: false
        cacheBuffer: strip.wrapAround ? 0 : 520
        displayMarginBeginning: 0
        displayMarginEnd: 0
        pixelAligned: true
        reuseItems: true

        // Center the strip initially
        onContentWidthChanged: {
            if (!strip._initialCentered && contentWidth > width && count > 0) {
                if (strip.wrapAround && strip._jumpWidth > 0) {
                    contentX = strip._jumpWidth - width / 2
                } else {
                    contentX = (contentWidth - width) / 2
                }
                strip._initialCentered = true
            }
        }

        delegate: Item {
            required property var model
            required property int index
            width: _gc.width
            height: ListView.view.height

            GameCard {
                id: _gc
                anchors.verticalCenter: parent.verticalCenter
                height: view._baseH
                gameId: model.gameId ?? model.id ?? ""
                gameName: model.name ?? model.gameName ?? ""
                steamAppId: model.steamAppId ?? ""
                installPath: model.installPath ?? ""
                onClicked: strip.gameClicked(
                    model.gameId ?? model.id ?? "",
                    model.name ?? model.gameName ?? "",
                    model.installPath ?? "", model.engine ?? ""
                )
            }
        }
    }

    // Edge fade — only when content overflows
    readonly property bool _needsEdge: view.contentWidth > view.width
    readonly property int _edgeW: Math.ceil(view._baseH * (view._hoverScale - 1.0)) + 16

    // Strip-level hover
    HoverHandler { id: stripHover }

    // --- Left edge + nav chevron ---
    Rectangle {
        visible: strip._needsEdge
        anchors.left: parent.left; anchors.top: view.top; anchors.bottom: view.bottom
        width: strip._edgeW; z: 10
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.bgPrimary }
            GradientStop { position: 0.5; color: Theme.withAlpha(Theme.bgPrimary, 0.5) }
            GradientStop { position: 1.0; color: "transparent" }
        }

        Canvas {
            anchors.centerIn: parent; width: 16; height: 16
            opacity: stripHover.hovered && view.contentX > view.originX + 10 ? 0.9 : 0
            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            renderStrategy: Canvas.Cooperative
            onPaint: {
                var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2.2
                ctx.lineCap = "round"; ctx.lineJoin = "round"
                ctx.shadowColor = "#ffffff"; ctx.shadowBlur = 8
                ctx.beginPath(); ctx.moveTo(11,2); ctx.lineTo(5,8); ctx.lineTo(11,14); ctx.stroke()
            }
        }

        MouseArea {
            anchors.fill: parent; z: 20
            cursorShape: Qt.PointingHandCursor
            visible: stripHover.hovered && view.contentX > view.originX + 10
            onClicked: { scrollAnim.stop(); scrollAnim.to = Math.max(view.originX, view.contentX - view.width * 0.6); scrollAnim.start() }
        }
    }

    // --- Right edge + nav chevron ---
    Rectangle {
        visible: strip._needsEdge
        anchors.right: parent.right; anchors.top: view.top; anchors.bottom: view.bottom
        width: strip._edgeW; z: 10
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.withAlpha(Theme.bgPrimary, 0.5) }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }

        Canvas {
            anchors.centerIn: parent; width: 16; height: 16
            opacity: stripHover.hovered && view.contentX < (view.contentWidth - view.width + view.originX - 10) ? 0.9 : 0
            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            renderStrategy: Canvas.Cooperative
            onPaint: {
                var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2.2
                ctx.lineCap = "round"; ctx.lineJoin = "round"
                ctx.shadowColor = "#ffffff"; ctx.shadowBlur = 8
                ctx.beginPath(); ctx.moveTo(5,2); ctx.lineTo(11,8); ctx.lineTo(5,14); ctx.stroke()
            }
        }

        MouseArea {
            anchors.fill: parent; z: 20
            cursorShape: Qt.PointingHandCursor
            visible: stripHover.hovered && view.contentX < (view.contentWidth - view.width + view.originX - 10)
            onClicked: { scrollAnim.stop(); scrollAnim.to = Math.min(view.contentWidth - view.width + view.originX, view.contentX + view.width * 0.6); scrollAnim.start() }
        }
    }

    // Weighted drag — click only fires if movement < 8px
    MouseArea {
        anchors.fill: view
        cursorShape: _dragged ? Qt.ClosedHandCursor : Qt.ArrowCursor

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
            } else {
                var jw = strip._jumpWidth
                if (jw > 0) {
                    if (newX < jw * 0.15) newX += jw
                    else if (newX > jw * 1.15) newX -= jw
                }
            }

            view.contentX = newX

            var now = Date.now(), dt = now - _lastTime
            if (dt > 0) {
                _releaseV = (mouse.x - _lastX) / dt * 16
                _lastX = mouse.x
                _lastTime = now
            }
        }

        property bool _wasDragged: false

        onReleased: {
            _wasDragged = _dragged
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
            _dragged = false
        }

        onClicked: function(mouse) {
            if (_wasDragged) return
            var item = view.itemAt(view.contentX + mouse.x, mouse.y)
            if (!item) return
            // Delegate is Item wrapper — find the GameCard child
            if (typeof item.clicked === "function") item.clicked()
            else if (item.children.length > 0 && typeof item.children[0].clicked === "function") item.children[0].clicked()
        }
    }

    // Momentum after drag release + wheel scroll
    property real _velocity: 0

    // Frame-synced momentum — fires exactly once per vsync, no timer drift
    FrameAnimation {
        id: momentumAnim
        running: false
        onTriggered: {
            var dt = Math.min(frameTime, 0.05)
            strip._velocity *= Math.pow(0.82, dt / 0.016)

            if (Math.abs(strip._velocity) < 0.3) {
                strip._velocity = 0
                if (typeof FrameTimer !== "undefined") FrameTimer.endInteraction()
                running = false
                return
            }

            var displacement = strip._velocity * (dt / 0.016)
            var newX = view.contentX - displacement

            if (!strip.wrapAround) {
                var lo = view.originX
                var hi = Math.max(lo, view.contentWidth - view.width + lo)
                newX = Math.max(lo, Math.min(hi, newX))
            } else {
                var jw = strip._jumpWidth
                if (jw > 0) {
                    if (newX < jw * 0.15) newX += jw
                    else if (newX > jw * 1.15) newX -= jw
                }
            }

            view.contentX = newX
        }
    }

    WheelHandler {
        enabled: strip.wheelEnabled
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

    // Subtle continuous drift (only when wrapAround is active)
    FrameAnimation {
        id: driftAnim
        running: strip.driftSpeed !== 0 && strip.wrapAround
                 && strip._jumpWidth > 0
                 && !momentumAnim.running
        onTriggered: {
            var dt = Math.min(frameTime, 0.05)
            var displacement = strip.driftSpeed * dt
            var newX = view.contentX + displacement

            var jw = strip._jumpWidth
            if (jw > 0) {
                if (newX < jw * 0.15) newX += jw
                else if (newX > jw * 1.15) newX -= jw
            }

            view.contentX = newX
        }
    }

}
