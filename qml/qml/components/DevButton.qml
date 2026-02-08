import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property string icon: ""
    property bool isLoading: false
    property bool isDestructive: false
    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 72
    color: (mouseArea.containsMouse || activeFocus) ? Theme.withAlpha(Theme.textPrimary, 0.04) : "transparent"
    radius: Dimensions.radiusStandard
    enabled: !root.isLoading

    activeFocusOnTab: enabled
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: subtitle
    Accessible.onPressAction: clicked()
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL

        // Left item (icon)
        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Dimensions.radiusStandard
            color: root.isDestructive
                 ? Theme.withAlpha(Theme.error, 0.1)
                 : Theme.withAlpha(Theme.textPrimary, 0.06)

            scale: mouseArea.pressed ? 0.95 : 1.0
            Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

            Image {
                anchors.centerIn: parent
                source: root.icon
                width: 18
                height: 18
                sourceSize: Qt.size(18, 18)
                antialiasing: true
            }
        }

        // Content (title and subtitle)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            Label {
                text: root.title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: root.isDestructive ? Theme.error : Theme.textPrimary
            }

            Label {
                text: root.subtitle
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }
        }

        // Right item (loading indicator or chevron)
        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24

            BusyIndicator {
                anchors.fill: parent
                visible: root.isLoading
                running: root.isLoading
            }

            Image {
                anchors.centerIn: parent
                visible: !root.isLoading
                source: "qrc:/qt/qml/MakineAI/resources/icons/chevron-right.svg"
                width: 20
                height: 20
                sourceSize: Qt.size(20, 20)
                antialiasing: true
            }
        }
    }
}
