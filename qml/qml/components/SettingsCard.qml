import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SettingsCard.qml - Ayarlar kartı bileşeni
 */
Rectangle {
    id: root

    property string title: ""
    property string description: ""
    default property alias content: contentColumn.children

    implicitHeight: mainColumn.implicitHeight + 32
    radius: Dimensions.radiusStandard
    color: Theme.withAlpha(Theme.bgSecondary, 0.5)
    border.color: Theme.withAlpha(Theme.textMuted, 0.1)
    border.width: 1

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: root.title !== ""

            Text {
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                Layout.fillWidth: true
                text: root.description
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                visible: root.description !== ""
            }
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: 8
        }
    }
}
