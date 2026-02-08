import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * InfoRow.qml - Bilgi satırı (label: value)
 */
RowLayout {
    id: root

    property string label: ""
    property string value: ""
    property color labelColor: Theme.textMuted
    property color valueColor: Theme.textPrimary

    spacing: Dimensions.spacingMD

    Text {
        text: root.label
        font.pixelSize: Dimensions.fontBody
        color: root.labelColor
    }

    Item { Layout.fillWidth: true }

    Text {
        text: root.value
        font.pixelSize: Dimensions.fontBody
        font.weight: Font.Medium
        color: root.valueColor
        horizontalAlignment: Text.AlignRight
    }
}
