import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * PlatformIcons.qml - Platform ikonları (Windows, Mac, Linux)
 */
RowLayout {
    id: root
    property bool hasWindows: false
    property bool hasMac: false
    property bool hasLinux: false
    property color iconColor: Theme.textPrimary
    property int iconSize: 18
    spacing: 4

    // Windows icon - text based for simplicity
    Text {
        text: "⊞"  // Windows-like symbol
        font.pixelSize: root.iconSize
        color: root.iconColor
        visible: root.hasWindows
    }

    // Mac icon
    Text {
        text: ""  // Apple symbol (if font supports)
        font.pixelSize: root.iconSize
        color: root.iconColor
        visible: root.hasMac
    }

    // Linux icon
    Text {
        text: "🐧"  // Penguin emoji for Linux
        font.pixelSize: root.iconSize - 2
        color: root.iconColor
        visible: root.hasLinux
    }
}
