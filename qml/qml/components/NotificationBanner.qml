import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * NotificationBanner.qml - Dismissable notification bar for status messages
 */
Rectangle {
    id: root

    property string notificationMessage: ""
    property string notificationType: "info" // "info", "warning", "error"

    signal settingsRequested()
    signal dismissRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: visible ? contentRow.implicitHeight + 16 : 0
    visible: notificationMessage !== ""

    radius: Dimensions.radiusSM
    color: {
        if (notificationType === "error") return Theme.withAlpha(Theme.destructive, 0.10)
        if (notificationType === "warning") return Theme.withAlpha(Theme.warning, 0.10)
        return Theme.withAlpha(Theme.primary, 0.08)
    }
    border.color: {
        if (notificationType === "error") return Theme.withAlpha(Theme.destructive, 0.25)
        if (notificationType === "warning") return Theme.withAlpha(Theme.warning, 0.25)
        return Theme.withAlpha(Theme.primary, 0.15)
    }
    border.width: 1

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.margins: 8
        spacing: Dimensions.spacingSM

        // Type icon
        Label {
            text: {
                if (root.notificationType === "error") return "\u26A0"
                if (root.notificationType === "warning") return "\u26A0"
                return "\u2139"
            }
            font.pixelSize: Dimensions.fontSM
            color: {
                if (root.notificationType === "error") return Theme.destructive
                if (root.notificationType === "warning") return Theme.warning
                return Theme.primary
            }
        }

        Label {
            text: root.notificationMessage
            font.pixelSize: Dimensions.fontSM
            color: Theme.textPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Settings link
        Label {
            text: qsTr("Ayarlar")
            font.pixelSize: Dimensions.fontXS
            color: Theme.primary
            visible: root.notificationType === "warning"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.settingsRequested()
            }
        }

        // Dismiss button
        Label {
            text: "\u2715"
            font.pixelSize: Dimensions.fontXS
            color: Theme.textMuted

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.dismissRequested()
            }
        }
    }
}
