import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * CustomScrollBar.qml - Özel scrollbar bileşeni
 */
ScrollBar {
    id: root

    property color handleColor: Theme.withAlpha(Theme.textMuted, 0.3)
    property color handleHoverColor: Theme.withAlpha(Theme.textMuted, 0.5)

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 100
        radius: 3
        color: root.hovered || root.pressed ? root.handleHoverColor : root.handleColor
        opacity: root.active ? 1.0 : 0.0

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    background: Rectangle {
        implicitWidth: 6
        color: "transparent"
    }
}
