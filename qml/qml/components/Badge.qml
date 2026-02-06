import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * Badge.qml - Küçük etiket/rozet bileşeni
 */
Rectangle {
    id: root

    property string text: ""
    property color backgroundColor: Theme.withAlpha(Theme.primary, 0.15)
    property color textColor: Theme.primary
    property int fontSize: 11

    // Legacy alias for compatibility
    property alias badgeColor: root.textColor

    implicitWidth: badgeText.implicitWidth + Dimensions.paddingLG
    implicitHeight: 22
    radius: Dimensions.radiusXS
    color: root.backgroundColor
    border.color: Theme.withAlpha(root.textColor, 0.3)
    border.width: 1

    Text {
        id: badgeText
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.fontSize
        font.weight: Font.DemiBold
        color: root.textColor
    }
}
