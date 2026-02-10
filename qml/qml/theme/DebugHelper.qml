pragma Singleton
import QtQuick

/**
 * DebugHelper - Bug detection and logging system
 *
 * Usage:
 *   DebugHelper.log("component", "message")
 *   DebugHelper.logState("component", {"key": value})
 *   DebugHelper.logError("MAK-001", "description")
 *   DebugHelper.logNavigation(fromIndex, toIndex)
 */
QtObject {
    id: root

    // Enable/disable debug mode - set to false for release builds
    readonly property bool enabled: false

    // Log levels (camelCase for QML)
    readonly property int levelDebug: 0
    readonly property int levelInfo: 1
    readonly property int levelWarn: 2
    readonly property int levelError: 3

    property int minLevel: levelDebug

    // Error code registry (MAK-XXX format)
    readonly property var errorCodes: ({
        // Navigation errors (001-099)
        "MAK-001": "Page transition failed - invalid index",
        "MAK-002": "Page transition failed - already transitioning",
        "MAK-003": "Navigation target not found",
        "MAK-004": "Back navigation failed - no previous page",

        // UI State errors (100-199)
        "MAK-100": "Component visibility state mismatch",
        "MAK-101": "Animation state corrupted",
        "MAK-102": "Opacity value out of range",
        "MAK-103": "Layout binding broken",

        // Data errors (200-299)
        "MAK-200": "Game data load failed",
        "MAK-201": "Settings save failed",
        "MAK-202": "Translation data missing",

        // Service errors (300-399)
        "MAK-300": "GameService not responding",
        "MAK-302": "Network request failed"
    })

    // Navigation history for debugging
    property var navigationHistory: []
    property int maxHistorySize: 20

    // State snapshots
    property var stateSnapshots: []

    // Timestamp helper
    function timestamp() {
        var now = new Date()
        return now.toTimeString().split(' ')[0] + "." + now.getMilliseconds()
    }

    // Basic log
    function log(component, message) {
        if (!enabled || minLevel > levelDebug) return
        console.log("[" + timestamp() + "] [DEBUG] [" + component + "] " + message)
    }

    // Info log
    function info(component, message) {
        if (!enabled || minLevel > levelInfo) return
        console.log("[" + timestamp() + "] [INFO] [" + component + "] " + message)
    }

    // Warning log
    function warn(component, message) {
        if (!enabled || minLevel > levelWarn) return
        console.warn("[" + timestamp() + "] [WARN] [" + component + "] " + message)
    }

    // Error log with code
    function logError(code, details) {
        if (!enabled) return
        var desc = errorCodes[code] || "Unknown error"
        console.error("[" + timestamp() + "] [ERROR] " + code + ": " + desc)
        if (details) {
            console.error("  Details: " + JSON.stringify(details))
        }

        // Store for crash report
        stateSnapshots.push({
            type: "error",
            timestamp: new Date().toISOString(),
            code: code,
            description: desc,
            details: details
        })
    }

    // Log state object
    function logState(component, state) {
        if (!enabled || minLevel > levelDebug) return
        console.log("[" + timestamp() + "] [STATE] [" + component + "] " + JSON.stringify(state))

        // Store snapshot
        if (stateSnapshots.length >= 50) {
            stateSnapshots.shift()
        }
        stateSnapshots.push({
            type: "state",
            timestamp: new Date().toISOString(),
            component: component,
            state: state
        })
    }

    // Log navigation
    function logNavigation(fromIndex, toIndex, success) {
        if (!enabled) return

        var pageNames = ["Home", "Settings", "GameDetail"]
        var fromName = pageNames[fromIndex] || "Unknown(" + fromIndex + ")"
        var toName = pageNames[toIndex] || "Unknown(" + toIndex + ")"

        var entry = {
            timestamp: new Date().toISOString(),
            from: fromIndex,
            to: toIndex,
            fromName: fromName,
            toName: toName,
            success: success
        }

        navigationHistory.push(entry)
        if (navigationHistory.length > maxHistorySize) {
            navigationHistory.shift()
        }

        var status = success ? "OK" : "FAILED"
        console.log("[" + timestamp() + "] [NAV] " + fromName + " -> " + toName + " [" + status + "]")
    }

    // Validate page state
    function validatePageState(pageIndex, page) {
        if (!enabled) return true

        var issues = []

        // Check opacity
        if (page.opacity < 0 || page.opacity > 1) {
            issues.push("opacity=" + page.opacity + " (invalid)")
            logError("MAK-102", {pageIndex: pageIndex, opacity: page.opacity})
        }

        // Check visibility consistency
        // A visible page should have opacity > 0
        if (page.visible && page.opacity === 0) {
            issues.push("visible=true but opacity=0")
            logError("MAK-100", {pageIndex: pageIndex, visible: page.visible, opacity: page.opacity})
        }

        if (issues.length > 0) {
            warn("PageValidator", "Page " + pageIndex + " issues: " + issues.join(", "))
            return false
        }

        return true
    }

    // Get debug report
    function getDebugReport() {
        return {
            timestamp: new Date().toISOString(),
            navigationHistory: navigationHistory,
            recentStates: stateSnapshots.slice(-10),
            errors: stateSnapshots.filter(function(s) { return s.type === "error" })
        }
    }

    // Print summary
    function printSummary() {
        console.log("=== DEBUG SUMMARY ===")
        console.log("Navigation count: " + navigationHistory.length)
        console.log("State snapshots: " + stateSnapshots.length)
        var errors = stateSnapshots.filter(function(s) { return s.type === "error" })
        console.log("Errors: " + errors.length)
        if (errors.length > 0) {
            console.log("Recent errors:")
            errors.slice(-5).forEach(function(e) {
                console.log("  " + e.code + ": " + e.description)
            })
        }
        console.log("====================")
    }
}
