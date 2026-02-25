import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SettingsCard.qml - Settings card container
 *
 * Simple bordered card that wraps settings content.
 * Children are placed in a zero-spacing ColumnLayout.
 */
Rectangle {
    default property alias content: _cc.data
    implicitHeight: _cc.implicitHeight
    radius: Dimensions.radiusStandard
    color: Theme.surface
    border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
    border.width: 1
    ColumnLayout { id: _cc; anchors.fill: parent; spacing: 0 }
}
