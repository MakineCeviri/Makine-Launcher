import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * PerformanceMonitor.qml - Real-time performance overlay
 *
 * Shows FPS, frame time, and memory usage in development mode.
 * Press F3 to toggle visibility.
 *
 * Usage in Main.qml:
 *   PerformanceMonitor {
 *       visible: developmentMode
 *   }
 */
Rectangle {
    id: root

    // Configuration
    property bool showDetails: true
    property int updateInterval: 500  // ms
    property int sampleCount: 60      // frames to average

    // Metrics
    property real currentFps: 0
    property real avgFrameTime: 0
    property real minFrameTime: 999
    property real maxFrameTime: 0
    property int frameCount: 0
    property int droppedFrames: 0

    // Thresholds
    readonly property real targetFps: 60
    readonly property real targetFrameTime: 16.67  // 1000/60

    // Internal
    property var frameTimes: []
    property real lastFrameTime: 0

    width: showDetails ? 180 : 80
    height: showDetails ? contentColumn.height + 16 : 32
    radius: Dimensions.radiusStandard
    color: Qt.rgba(0, 0, 0, 0.85)
    border.color: fpsColor
    border.width: 1

    // Position: top-right corner
    anchors.right: parent ? parent.right : undefined
    anchors.top: parent ? parent.top : undefined
    anchors.margins: Dimensions.marginMS

    // FPS color based on performance
    property color fpsColor: {
        if (currentFps >= 55) return Theme.statusOnline
        if (currentFps >= 30) return Theme.warning
        return Theme.error
    }

    // Frame timer - measures time between frames
    Item {
        id: frameTimer

        property real startTime: 0

        // Called every frame via NumberAnimation
        NumberAnimation on rotation {
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
            running: root.visible
        }

        onRotationChanged: {
            var now = Date.now()
            if (lastFrameTime > 0) {
                var delta = now - lastFrameTime
                frameTimes.push(delta)

                // Keep only last N samples
                if (frameTimes.length > sampleCount) {
                    frameTimes.shift()
                }

                // Detect dropped frames (> 2x target)
                if (delta > targetFrameTime * 2) {
                    droppedFrames++
                }

                frameCount++
            }
            lastFrameTime = now
        }
    }

    // Update metrics periodically
    Timer {
        interval: root.updateInterval
        repeat: true
        running: root.visible
        onTriggered: updateMetrics()
    }

    function updateMetrics() {
        if (frameTimes.length === 0) return

        // Calculate average frame time
        var sum = 0
        var min = 999
        var max = 0

        for (var i = 0; i < frameTimes.length; i++) {
            sum += frameTimes[i]
            if (frameTimes[i] < min) min = frameTimes[i]
            if (frameTimes[i] > max) max = frameTimes[i]
        }

        avgFrameTime = sum / frameTimes.length
        minFrameTime = min
        maxFrameTime = max
        currentFps = avgFrameTime > 0 ? 1000 / avgFrameTime : 0
    }

    function reset() {
        frameTimes = []
        frameCount = 0
        droppedFrames = 0
        currentFps = 0
        avgFrameTime = 0
        minFrameTime = 999
        maxFrameTime = 0
    }

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Performance Monitor")

    // Click to toggle details
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.showDetails = !root.showDetails
        onDoubleClicked: root.reset()
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: Dimensions.marginSM
        spacing: Dimensions.spacingXS

        // FPS Header (always visible)
        RowLayout {
            spacing: Dimensions.spacingMD

            // FPS indicator dot
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.fpsColor

                // Pulse animation when low FPS
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: root.visible && root.currentFps < 30
                    NumberAnimation { to: 0.3; duration: Dimensions.fadeTransitionDuration }
                    NumberAnimation { to: 1.0; duration: Dimensions.fadeTransitionDuration }
                }
            }

            Text {
                text: Math.round(root.currentFps) + " FPS"
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Bold
                font.family: "Consolas"
                color: root.fpsColor
            }

            Item { Layout.fillWidth: true }

            // Collapse indicator
            Text {
                text: root.showDetails ? "▼" : "▶"
                font.pixelSize: Dimensions.fontCaption
                color: Theme.textMuted
            }
        }

        // Details section
        ColumnLayout {
            visible: root.showDetails
            spacing: Dimensions.spacingXXS
            Layout.fillWidth: true

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Qt.rgba(1, 1, 1, 0.1)
            }

            // Frame time
            MetricRow {
                label: "Frame"
                value: root.avgFrameTime.toFixed(2) + " ms"
                valueColor: root.avgFrameTime <= root.targetFrameTime ? Theme.statusOnline : Theme.warning
            }

            // Min/Max frame time
            MetricRow {
                label: "Min/Max"
                value: root.minFrameTime.toFixed(1) + "/" + root.maxFrameTime.toFixed(1) + " ms"
                valueColor: Theme.textMuted
            }

            // Dropped frames
            MetricRow {
                label: "Dropped"
                value: root.droppedFrames.toString()
                valueColor: root.droppedFrames > 0 ? Theme.error : Theme.statusOnline
            }

            // Frame count
            MetricRow {
                label: "Frames"
                value: root.frameCount.toString()
                valueColor: Theme.textMuted
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Qt.rgba(1, 1, 1, 0.1)
                Layout.topMargin: Dimensions.marginXXS
            }

            // Help text
            Text {
                Layout.fillWidth: true
                text: qsTr("Click: toggle • Dbl: reset")
                font.pixelSize: Dimensions.fontMini
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Metric row component
    component MetricRow: RowLayout {
        property string label: ""
        property string value: ""
        property color valueColor: Theme.textSecondary

        Layout.fillWidth: true
        spacing: Dimensions.spacingXS

        Text {
            text: label
            font.pixelSize: Dimensions.fontCaption
            font.family: "Consolas"
            color: Theme.textMuted
        }

        Item { Layout.fillWidth: true }

        Text {
            text: value
            font.pixelSize: Dimensions.fontCaption
            font.family: "Consolas"
            font.weight: Font.Medium
            color: valueColor
        }
    }
}
