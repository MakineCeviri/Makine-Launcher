import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsCard.qml - Settings card container
 *
 * Simple bordered card that wraps settings content.
 * Children are placed in a zero-spacing ColumnLayout.
 */
Rectangle {
    default property alias content: _cc.data
    implicitHeight: _cc.implicitHeight
    radius: Dimensions.radiusMD
    color: Theme.surface
    border.color: Theme.primary06
    border.width: 1
    ColumnLayout { id: _cc; anchors.fill: parent; spacing: 0 }
}
