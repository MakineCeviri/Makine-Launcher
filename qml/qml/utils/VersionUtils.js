/**
 * VersionUtils.js - Shared version comparison utility
 *
 * Compares semantic version strings (e.g., "1.2.3" vs "1.3.0").
 * Returns: 1 if v1 > v2, -1 if v1 < v2, 0 if equal.
 */

function compareVersions(v1, v2) {
    if (!v1 || !v2) return 0

    var parts1 = v1.split(".").map(function(x) { return parseInt(x) || 0 })
    var parts2 = v2.split(".").map(function(x) { return parseInt(x) || 0 })

    for (var i = 0; i < Math.max(parts1.length, parts2.length); i++) {
        var p1 = parts1[i] || 0
        var p2 = parts2[i] || 0
        if (p1 > p2) return 1
        if (p1 < p2) return -1
    }
    return 0
}
