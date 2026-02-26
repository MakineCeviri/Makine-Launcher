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

        Rectangle {
            Layout.preferredWidth: 6; Layout.preferredHeight: 6
            radius: 3; color: Theme.accentBase
            Layout.alignment: Qt.AlignVCenter
        }

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
