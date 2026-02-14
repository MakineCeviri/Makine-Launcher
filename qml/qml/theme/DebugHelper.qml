pragma Singleton
import QtQuick

/**
 * DebugHelper - Lightweight debug logging
 *
 * Usage:
 *   DebugHelper.log("component", "message")
 *   DebugHelper.warn("component", "message")
 */
QtObject {
    // Enable/disable debug mode - set to false for release builds
    readonly property bool enabled: false

    function timestamp() {
        var now = new Date()
        return now.toTimeString().split(' ')[0] + "." + now.getMilliseconds()
    }

    function log(component, message) {
        if (!enabled) return
        console.log("[" + timestamp() + "] [DEBUG] [" + component + "] " + message)
    }

    function warn(component, message) {
        if (!enabled) return
        console.warn("[" + timestamp() + "] [WARN] [" + component + "] " + message)
    }
}
