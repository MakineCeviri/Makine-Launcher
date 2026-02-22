import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

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
        radius: Dimensions.radiusStandard
        color: mouseArea.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.03) : "transparent"

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
                    ? Theme.withAlpha(Theme.error, 0.1)
                    : Theme.withAlpha(Theme.textPrimary, 0.06)
                visible: root.icon !== "" || root.iconSource !== ""

                scale: mouseArea.pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                // Emoji/text icon
                Label {
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
                    opacity: root.isDestructive ? 1.0 : 0.6
                }
            }

            // Built-in title/subtitle layout
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXXS
                visible: root.title !== ""

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
            Image {
                visible: root.showArrow
                source: "qrc:/qt/qml/MakineAI/resources/icons/arrow_right.svg"
                sourceSize: Qt.size(16, 16)
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
