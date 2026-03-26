import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * StyledScrollBar.qml - Themed scrollbar for ScrollView/Flickable
 *
 * Replaces 6+ inline scrollbar styling blocks:
 *   ScrollBar.vertical: StyledScrollBar {}
 */
ScrollBar {
    id: root

    policy: ScrollBar.AsNeeded

    background: Rectangle { color: "transparent" }

    contentItem: Rectangle {
        implicitWidth: root.pressed ? 5 : 3
        radius: implicitWidth / 2
        color: root.pressed ? Theme.scrollbarThumbHover
             : root.hovered ? Theme.scrollbarThumbHover
             : Theme.scrollbarThumb
        opacity: root.active ? 1.0 : 0.0

        Behavior on implicitWidth { NumberAnimation { duration: Dimensions.animVeryFast } }
        Behavior on opacity { NumberAnimation { duration: Dimensions.animMedium } }
    }
}
