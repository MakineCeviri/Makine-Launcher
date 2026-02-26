import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * GameSection.qml - Reusable section: title + badge + horizontal game strip + empty state.
 * Wraps SectionContainer with a standard header pattern.
 */
SectionContainer {
    id: section

    property string title: ""
    property var model: []
    property color badgeColor: Theme.primary
    property string emptyText: ""
    property bool loading: false

    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    Layout.fillWidth: true
    Layout.fillHeight: true

    // Header row
    RowLayout {
        Layout.fillWidth: true
        spacing: Dimensions.spacingSM

        Label {
            textFormat: Text.PlainText
            text: section.title
            font.pixelSize: Dimensions.fontLG
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        BusyIndicator {
            visible: section.loading
            running: section.loading
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            Layout.preferredHeight: 20
            Layout.preferredWidth: countLabel.width + 14
            radius: 10
            color: Theme.withAlpha(section.badgeColor, 0.12)
            border.color: Theme.withAlpha(section.badgeColor, 0.20)
            border.width: 1
            Label {
                textFormat: Text.PlainText
                id: countLabel
                anchors.centerIn: parent
                text: (section.model || []).length
                font.pixelSize: Dimensions.fontXS
                font.weight: Font.Medium
                color: section.badgeColor
            }
        }
    }

    // Separator
    SettingsDivider { variant: "section" }

    // Game strip with edge controls
    Item {
        id: stripContainer
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (section.model || []).length > 0

        HorizontalGameStrip {
            id: gameStrip
            anchors.fill: parent
            model: section.model
            onGameClicked: (gameId, gameName, installPath, engine) =>
                section.gameClicked(gameId, gameName, installPath, engine)
        }

        // Left edge fade — anchored to section container (same as CatalogSection)
        Rectangle {
            anchors.left: section.left
            anchors.top: section.top; anchors.bottom: section.bottom
            anchors.topMargin: 40
            width: 28; z: 10
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.90) }
                GradientStop { position: 0.4; color: Qt.rgba(0.055, 0.055, 0.055, 0.25) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Right edge fade — anchored to section container (same as CatalogSection)
        Rectangle {
            anchors.right: section.right
            anchors.top: section.top; anchors.bottom: section.bottom
            anchors.topMargin: 40
            width: 28; z: 10; rotation: 180
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.90) }
                GradientStop { position: 0.4; color: Qt.rgba(0.055, 0.055, 0.055, 0.25) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Left arrow button
        Rectangle {
            id: leftArrow
            anchors.left: parent.left; anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 32; height: 32; radius: 16; z: 20
            visible: gameStrip.canScrollLeft
            color: leftMouse.containsMouse
                ? Theme.withAlpha(Theme.bgPrimary, 0.92)
                : Theme.withAlpha(Theme.bgPrimary, 0.70)
            border.color: Theme.glassBorder; border.width: 1
            scale: leftMouse.pressed ? 0.90 : 1.0
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2039"
                font.pixelSize: Dimensions.fontTitle
                color: Theme.textPrimary
            }

            MouseArea {
                id: leftMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: gameStrip.scrollLeft()
            }
        }

        // Right arrow button
        Rectangle {
            id: rightArrow
            anchors.right: parent.right; anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 32; height: 32; radius: 16; z: 20
            visible: gameStrip.canScrollRight
            color: rightMouse.containsMouse
                ? Theme.withAlpha(Theme.bgPrimary, 0.92)
                : Theme.withAlpha(Theme.bgPrimary, 0.70)
            border.color: Theme.glassBorder; border.width: 1
            scale: rightMouse.pressed ? 0.90 : 1.0
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u203A"
                font.pixelSize: Dimensions.fontTitle
                color: Theme.textPrimary
            }

            MouseArea {
                id: rightMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: gameStrip.scrollRight()
            }
        }
    }

    // Empty state
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (section.model || []).length === 0 && !section.loading
        text: section.emptyText
        font.pixelSize: Dimensions.fontSM
        color: Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
