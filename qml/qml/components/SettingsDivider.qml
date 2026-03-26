import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsDivider.qml - Thin horizontal separator line.
 * variant "settings" (default): 0.04 opacity — for settings panels
 * variant "section": 0.06 opacity — for section headers and screen separators
 */
Rectangle {
    property string variant: "settings"

    Layout.fillWidth: true
    Layout.preferredHeight: 1
    color: variant === "section" ? Theme.textPrimary06 : Theme.textPrimary04
}
