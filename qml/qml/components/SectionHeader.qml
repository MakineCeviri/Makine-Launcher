import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SectionHeader.qml - Section header with icon (Native Qt)
 */
RowLayout {
    id: root

    property string title: ""
    property string iconSource: ""
    property color iconColor: Theme.primary
    property color backgroundColor: Theme.withAlpha(Theme.primary, 0.15)
    property alias extraContent: extraContentItem.data

    spacing: 12

    Rectangle {
        Layout.preferredWidth: 40
        Layout.preferredHeight: 40
        radius: Dimensions.radiusStandard
        color: root.backgroundColor

        Image {
            anchors.centerIn: parent
            source: root.iconSource
            sourceSize.width: 20
            sourceSize.height: 20
            fillMode: Image.PreserveAspectFit
        }
    }

    Text {
        text: root.title
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    Item {
        Layout.fillWidth: true
        id: extraContentItem
    }
}
