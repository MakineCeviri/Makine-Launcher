import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

/**
 * EmptyState.qml - Reusable empty/zero-data state indicator
 *
 * Usage:
 *   EmptyState {
 *       icon: "\uD83C\uDFAE"
 *       title: qsTr("No games found")
 *       subtitle: qsTr("Scan your library to find games")
 *       actionText: qsTr("Scan Now")
 *       onActionClicked: doScan()
 *   }
 */
Rectangle {
    id: root

    property string icon: ""
    property string title: ""
    property string subtitle: ""
    property string actionText: ""

    signal actionClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: contentCol.implicitHeight + 2 * Dimensions.spacingXL
    radius: Dimensions.radiusStandard
    color: Theme.withAlpha(Theme.textPrimary, 0.03)
    border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
    border.width: 1

    ColumnLayout {
        id: contentCol
        anchors.centerIn: parent
        spacing: Dimensions.spacingSM

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.icon
            font.pixelSize: Dimensions.fontBanner
            visible: root.icon.length > 0
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.title
            font.pixelSize: Dimensions.fontMD
            font.weight: Font.Medium
            color: Theme.textSecondary
            visible: root.title.length > 0
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.subtitle
            font.pixelSize: Dimensions.fontSM
            color: Theme.textMuted
            visible: root.subtitle.length > 0
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Dimensions.spacingSM
            text: root.actionText
            visible: root.actionText.length > 0
            flat: true
            font.pixelSize: Dimensions.fontSM
            onClicked: root.actionClicked()

            contentItem: Label {
                text: parent.text
                font: parent.font
                color: Theme.accent
                horizontalAlignment: Text.AlignHCenter
            }
            background: Rectangle {
                radius: Dimensions.radiusSmall
                color: parent.hovered ? Theme.withAlpha(Theme.accent, 0.1) : "transparent"
            }
        }
    }
}
