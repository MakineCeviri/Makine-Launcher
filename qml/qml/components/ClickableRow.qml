import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ClickableRow.qml - Clickable row component for settings pages
 *
 * Supports two modes:
 * 1. Built-in layout: set title/subtitle + icon or iconSource
 * 2. Custom content: set contentItem with a Component
 */
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string icon: ""           // Emoji or text icon
    property string iconSource: ""     // SVG/PNG icon path
    property bool showArrow: true
    property bool isDestructive: false
    property alias contentItem: contentLoader.sourceComponent

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.title
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Layout.fillWidth: true
    implicitHeight: 72
    implicitWidth: 200

    // Focus indicator
    FocusRing { target: root }

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusMD
        color: mouseArea.containsMouse ? Theme.primary05 : "transparent"

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
            spacing: Dimensions.spacingXL

            // Icon container (visible when icon or iconSource is set)
            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: root.isDestructive
                    ? Theme.error10
                    : Theme.primary08
                visible: root.icon !== "" || root.iconSource !== ""

                scale: mouseArea.pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                // Emoji/text icon
                Label {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: root.icon
                    font.pixelSize: Dimensions.fontXL
                    color: root.isDestructive ? Theme.error : Theme.textMuted
                    visible: root.icon !== "" && root.iconSource === ""
                }

                // SVG/PNG icon
                Image {
                    anchors.centerIn: parent
                    source: root.iconSource
                    sourceSize: Qt.size(20, 20)
                    visible: root.iconSource !== ""
                    asynchronous: true
                    mipmap: true
                    opacity: root.isDestructive ? 1.0 : 0.6
                }
            }

            // Built-in title/subtitle layout
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXXS
                visible: root.title !== ""

                Label {
                    textFormat: Text.PlainText
                    text: root.title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: root.isDestructive ? Theme.error : Theme.textPrimary
                }

                Label {
                    textFormat: Text.PlainText
                    text: root.subtitle
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                    visible: root.subtitle !== ""
                }
            }

            // Custom content loader (fallback when title is empty)
            Loader {
                id: contentLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.title === "" && sourceComponent !== null
            }

            // Arrow icon
            Text {
                textFormat: Text.PlainText
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                visible: root.showArrow
                text: "\uE76C"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 14
                color: Theme.primary
                opacity: mouseArea.containsMouse ? 0.8 : 0.4

                Behavior on opacity {
                    NumberAnimation { duration: Dimensions.animFast }
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }
    }
}
