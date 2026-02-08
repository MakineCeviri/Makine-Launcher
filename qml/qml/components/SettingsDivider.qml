import QtQuick
import MakineAI 1.0

/**
 * SettingsDivider.qml - Ayarlar bölüm ayırıcı
 */
Rectangle {
    id: root

    property int topMargin: Dimensions.marginMD
    property int bottomMargin: Dimensions.marginMD

    implicitHeight: 1 + topMargin + bottomMargin
    color: "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 1
        color: Theme.withAlpha(Theme.textMuted, 0.15)
    }
}
