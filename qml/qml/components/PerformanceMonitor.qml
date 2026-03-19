import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PerformanceMonitor.qml - Real-time performance overlay (dev builds only)
 *
 * Shows FPS, frame time, render time, jank detection, plus expandable sections:
 * - Scene: active screen, last transition, open dialogs
 * - Scroll: interaction FPS, max frame, jank during interaction
 * - Images: download count, cache hit rate, queue peak
 * - Memory: working set MB, peak, private bytes, image cache
 * - Animations: running/total count, names
 *
 * Data comes from C++ singletons (FrameTimer, SceneProfiler, MemoryProfiler).
 * Press F3 to toggle visibility.
 *
 * Gated by devToolsEnabled context property — absent in release builds.
 */
Rectangle {
    id: root

    // Configuration
    property bool showDetails: true

    // Expandable section toggles
    property bool showScene: false
    property bool showScroll: false
    property bool showImages: false
    property bool showMemory: false
    property bool showAnimations: false

    // C++ FrameTimer feeds all metrics (or fallback to zero)
    readonly property bool hasFrameTimer: typeof FrameTimer !== "undefined" && FrameTimer !== null
    readonly property real currentFps: hasFrameTimer ? FrameTimer.fps : 0
    readonly property real avgFrameTime: hasFrameTimer ? FrameTimer.frameTime : 0
    readonly property real renderTimeMs: hasFrameTimer ? FrameTimer.renderTime : 0
    readonly property real minFrameTime: hasFrameTimer ? FrameTimer.minFrameTime : 0
    readonly property real maxFrameTime: hasFrameTimer ? FrameTimer.maxFrameTime : 0
    readonly property int jankCount: hasFrameTimer ? FrameTimer.jankCount : 0
    readonly property int frameCount: hasFrameTimer ? FrameTimer.frameCount : 0

    // SceneProfiler
    readonly property bool hasSceneProfiler: typeof SceneProfiler !== "undefined" && SceneProfiler !== null
    readonly property string activeScreen: hasSceneProfiler ? SceneProfiler.activeScreen : ""
    readonly property real lastTransitionMs: hasSceneProfiler ? SceneProfiler.lastTransitionMs : 0
    readonly property string activeInteraction: hasSceneProfiler ? SceneProfiler.activeInteraction : ""
    readonly property int dialogCount: hasSceneProfiler ? SceneProfiler.dialogCount : 0
    readonly property int runningAnimations: hasSceneProfiler ? SceneProfiler.runningAnimationCount : 0

    // MemoryProfiler
    readonly property bool hasMemoryProfiler: typeof MemoryProfiler !== "undefined" && MemoryProfiler !== null
    readonly property real workingSetMB: hasMemoryProfiler ? MemoryProfiler.workingSetMB : 0
    readonly property real peakWorkingSetMB: hasMemoryProfiler ? MemoryProfiler.peakWorkingSetMB : 0
    readonly property real privateBytesMB: hasMemoryProfiler ? MemoryProfiler.privateBytesMB : 0
    readonly property int imageCacheCount: hasMemoryProfiler ? MemoryProfiler.imageCacheCount : 0
    readonly property real imageCacheSizeMB: hasMemoryProfiler ? MemoryProfiler.imageCacheSizeMB : 0

    // Interaction FPS from FrameTimer
    readonly property real interactionFps: hasFrameTimer ? FrameTimer.interactionFps : 0
    readonly property real interactionMaxFrame: hasFrameTimer ? FrameTimer.interactionMaxFrame : 0
    readonly property int interactionJankCount: hasFrameTimer ? FrameTimer.interactionJankCount : 0
    readonly property string interactionName: hasFrameTimer ? (FrameTimer.interactionName || "") : ""

    // Thresholds
    readonly property real targetFps: 60
    readonly property real targetFrameTime: 16.67  // 1000/60

    width: showDetails ? 280 : 80
    height: showDetails ? contentColumn.height + 16 : 32
    radius: Dimensions.radiusStandard
    color: Theme.bgPrimary85
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

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Performance Monitor")

    // Click to toggle details
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.showDetails = !root.showDetails
        onDoubleClicked: {
            if (root.hasFrameTimer) FrameTimer.reset()
        }
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
                    running: root.visible && root.currentFps > 0 && root.currentFps < 30
                             && root.Window.window !== null
                             && root.Window.window.visibility !== Window.Minimized
                    NumberAnimation { to: 0.3; duration: Dimensions.fadeTransitionDuration }
                    NumberAnimation { to: 1.0; duration: Dimensions.fadeTransitionDuration }
                    onRunningChanged: {
                        if (typeof SceneProfiler !== "undefined")
                            SceneProfiler.registerAnimation("perfMonitorPulse", running)
                    }
                }
            }

            Text {
                textFormat: Text.PlainText
                text: Math.round(root.currentFps) + " FPS"
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Bold
                font.family: "Consolas"
                color: root.fpsColor
            }

            Item { Layout.fillWidth: true }

            // Collapse indicator
            Text {
                textFormat: Text.PlainText
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
                color: Theme.textPrimary10
            }

            // Frame time
            MetricRow {
                label: qsTr("Frame")
                value: root.avgFrameTime.toFixed(2) + " ms"
                valueColor: root.avgFrameTime <= root.targetFrameTime ? Theme.statusOnline : Theme.warning
            }

            // Render time (GPU/scene graph)
            MetricRow {
                label: qsTr("Render")
                value: root.renderTimeMs.toFixed(2) + " ms"
                valueColor: root.renderTimeMs <= 8 ? Theme.statusOnline :
                            root.renderTimeMs <= 14 ? Theme.warning : Theme.error
            }

            // Min/Max frame time
            MetricRow {
                label: qsTr("Min/Max")
                value: root.minFrameTime.toFixed(1) + "/" + root.maxFrameTime.toFixed(1) + " ms"
                valueColor: Theme.textMuted
            }

            // Jank frames (>33ms)
            MetricRow {
                label: qsTr("Jank")
                value: root.jankCount.toString()
                valueColor: root.jankCount > 0 ? Theme.error : Theme.statusOnline
            }

            // Frame count
            MetricRow {
                label: qsTr("Frames")
                value: root.frameCount.toString()
                valueColor: Theme.textMuted
            }

            // ===== SCENE SECTION =====
            SectionHeader {
                text: "Scene"
                expanded: root.showScene
                onToggle: root.showScene = !root.showScene
            }

            ColumnLayout {
                visible: root.showScene
                spacing: Dimensions.spacingXXS
                Layout.fillWidth: true
                Layout.leftMargin: 4

                MetricRow {
                    label: "Screen"
                    value: root.activeScreen || "-"
                    valueColor: Theme.textSecondary
                }
                MetricRow {
                    label: "Transition"
                    value: root.lastTransitionMs > 0 ? root.lastTransitionMs.toFixed(0) + " ms" : "-"
                    valueColor: root.lastTransitionMs > 300 ? Theme.warning : Theme.statusOnline
                }
                MetricRow {
                    label: "Dialogs"
                    value: root.dialogCount.toString()
                    valueColor: root.dialogCount > 0 ? Theme.warning : Theme.textMuted
                }
            }

            // ===== SCROLL SECTION =====
            SectionHeader {
                text: "Scroll"
                expanded: root.showScroll
                onToggle: root.showScroll = !root.showScroll
            }

            ColumnLayout {
                visible: root.showScroll
                spacing: Dimensions.spacingXXS
                Layout.fillWidth: true
                Layout.leftMargin: 4

                MetricRow {
                    label: "Active"
                    value: root.interactionName || "idle"
                    valueColor: root.interactionName ? Theme.statusOnline : Theme.textMuted
                }
                MetricRow {
                    label: "FPS"
                    value: root.interactionFps > 0 ? Math.round(root.interactionFps).toString() : "-"
                    valueColor: root.interactionFps >= 55 ? Theme.statusOnline :
                                root.interactionFps >= 30 ? Theme.warning : Theme.textMuted
                }
                MetricRow {
                    label: "Max frame"
                    value: root.interactionMaxFrame > 0 ? root.interactionMaxFrame.toFixed(1) + " ms" : "-"
                    valueColor: root.interactionMaxFrame > 33 ? Theme.error : Theme.textMuted
                }
                MetricRow {
                    label: "Jank"
                    value: root.interactionJankCount.toString()
                    valueColor: root.interactionJankCount > 0 ? Theme.error : Theme.textMuted
                }
            }

            // ===== IMAGES SECTION =====
            SectionHeader {
                text: "Images"
                expanded: root.showImages
                onToggle: root.showImages = !root.showImages
            }

            ColumnLayout {
                visible: root.showImages
                spacing: Dimensions.spacingXXS
                Layout.fillWidth: true
                Layout.leftMargin: 4

                MetricRow {
                    label: "Cached"
                    value: root.imageCacheCount + " files"
                    valueColor: Theme.textSecondary
                }
                MetricRow {
                    label: "Size"
                    value: root.imageCacheSizeMB.toFixed(1) + " MB"
                    valueColor: Theme.textSecondary
                }
            }

            // ===== MEMORY SECTION =====
            SectionHeader {
                text: "Memory"
                expanded: root.showMemory
                onToggle: root.showMemory = !root.showMemory
            }

            ColumnLayout {
                visible: root.showMemory
                spacing: Dimensions.spacingXXS
                Layout.fillWidth: true
                Layout.leftMargin: 4

                MetricRow {
                    label: "Working"
                    value: root.workingSetMB.toFixed(1) + " MB"
                    valueColor: root.workingSetMB > 300 ? Theme.warning : Theme.textSecondary
                }
                MetricRow {
                    label: "Peak"
                    value: root.peakWorkingSetMB.toFixed(1) + " MB"
                    valueColor: Theme.textMuted
                }
                MetricRow {
                    label: "Private"
                    value: root.privateBytesMB.toFixed(1) + " MB"
                    valueColor: Theme.textMuted
                }
            }

            // ===== ANIMATIONS SECTION =====
            SectionHeader {
                text: "Animations"
                expanded: root.showAnimations
                onToggle: root.showAnimations = !root.showAnimations
            }

            ColumnLayout {
                visible: root.showAnimations
                spacing: Dimensions.spacingXXS
                Layout.fillWidth: true
                Layout.leftMargin: 4

                MetricRow {
                    label: "Running"
                    value: root.runningAnimations.toString()
                    valueColor: root.runningAnimations > 3 ? Theme.warning : Theme.statusOnline
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.textPrimary10
                Layout.topMargin: Dimensions.marginXXS
            }

            // Help text
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: qsTr("Click: toggle • Dbl: reset")
                font.pixelSize: Dimensions.fontMini
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Collapsible section header
    component SectionHeader: Rectangle {
        property string text: ""
        property bool expanded: false
        signal toggle()

        Layout.fillWidth: true
        Layout.topMargin: 2
        height: 18
        color: sectionMa.containsMouse ? Theme.textPrimary05 : "transparent"
        radius: 2

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            spacing: 4

            Text {
                textFormat: Text.PlainText
                text: parent.parent.expanded ? "▾" : "▸"
                font.pixelSize: Dimensions.fontMini
                color: Theme.textMuted
            }
            Text {
                textFormat: Text.PlainText
                text: parent.parent.text
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.DemiBold
                font.family: "Consolas"
                color: Theme.textSecondary
            }
            Item { Layout.fillWidth: true }
        }

        MouseArea {
            id: sectionMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.toggle()
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
            textFormat: Text.PlainText
            text: label
            font.pixelSize: Dimensions.fontCaption
            font.family: "Consolas"
            color: Theme.textMuted
        }

        Item { Layout.fillWidth: true }

        Text {
            textFormat: Text.PlainText
            text: value
            font.pixelSize: Dimensions.fontCaption
            font.family: "Consolas"
            font.weight: Font.Medium
            color: valueColor
        }
    }
}
