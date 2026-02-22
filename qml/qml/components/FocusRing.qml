import QtQuick
import MakineAI 1.0

/**
 * FocusRing.qml - Reusable keyboard focus indicator
 *
 * Replaces 25+ inline focus rectangles. Place as child of focusable item:
 *   Rectangle {
 *       activeFocusOnTab: true
 *       FocusRing {}
 *   }
 */
Rectangle {
    id: ring

    property Item target: parent
    property bool showRing: target ? target.activeFocus : false
    property color ringColor: Theme.withAlpha(Theme.primary, 0.6)
    property int offset: -2

    anchors.fill: target
    anchors.margins: offset
    radius: (target && target.radius !== undefined) ? target.radius - offset : -offset
    color: "transparent"
    border.color: ringColor
    border.width: 2
    visible: showRing
}
