import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

Rectangle {
    id: notificationBanner

    property string notificationMessage: ""
    property string notificationType: "info"  // info, warning, error, update

    signal settingsRequested()
    signal dismissRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: notificationMessage ? 44 : 0
    radius: Dimensions.radiusStandard
    visible: notificationMessage !== ""
    clip: true

    color: {
        switch(notificationType) {
            case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.15)
            case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.15)
            case "error": return Theme.withAlpha(Theme.notificationError, 0.15)
            default: return Theme.withAlpha(Theme.textMuted, 0.1)
        }
    }

    border.color: {
        switch(notificationType) {
            case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.4)
            case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.4)
            case "error": return Theme.withAlpha(Theme.notificationError, 0.4)
            default: return Theme.withAlpha(Theme.textPrimary, 0.1)
        }
    }
    border.width: 1

    Behavior on Layout.preferredHeight {
        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
    }

    // Content fade-in when banner appears
    property bool _shown: notificationMessage !== ""
    on_ShownChanged: {
        if (_shown) {
            bannerContentRow.opacity = 0
            bannerContentFadeIn.restart()
        }
    }
    NumberAnimation {
        id: bannerContentFadeIn
        target: bannerContentRow
        property: "opacity"
        from: 0; to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    RowLayout {
        id: bannerContentRow
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginMD
        anchors.rightMargin: Dimensions.marginMS
        spacing: Dimensions.spacingLG

        Text {
            text: {
                switch(notificationType) {
                    case "update": return "\u2B06"
                    case "warning": return "\u26A0"
                    case "error": return "\u2715"
                    default: return "\u2139"
                }
            }
            font.pixelSize: Dimensions.fontLG
            color: {
                switch(notificationType) {
                    case "update": return Theme.notificationUpdate
                    case "warning": return Theme.notificationWarning
                    case "error": return Theme.notificationError
                    default: return Theme.textSecondary
                }
            }

            // Subtle scale pulse on appear
            scale: notificationBanner._shown ? 1.0 : 0.5
            Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }
        }

        Text {
            Layout.fillWidth: true
            text: notificationBanner.notificationMessage
            font.pixelSize: Dimensions.fontBody
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Rectangle {
            visible: notificationType === "update"
            width: updateBtnText.width + 16
            height: 28
            radius: Dimensions.radiusStandard
            color: updateBtnMouse.containsMouse ? Theme.withAlpha(Theme.notificationUpdate, 0.3) : Theme.withAlpha(Theme.notificationUpdate, 0.2)
            scale: updateBtnMouse.pressed ? 0.94 : 1.0
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Go to settings")
            activeFocusOnTab: true
            Keys.onReturnPressed: notificationBanner.settingsRequested()
            Keys.onSpacePressed: notificationBanner.settingsRequested()

            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

            Text {
                id: updateBtnText
                anchors.centerIn: parent
                text: qsTr("Ayarlara Git")
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.notificationUpdate
            }

            // Focus indicator
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: parent.radius + 1
                color: "transparent"
                border.color: Theme.withAlpha(Theme.primary, 0.6)
                border.width: 2
                visible: parent.activeFocus
            }

            MouseArea {
                id: updateBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: notificationBanner.settingsRequested()
            }
        }

        Rectangle {
            width: 24
            height: 24
            radius: 12
            color: closeBtnMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Close notification")
            activeFocusOnTab: true
            Keys.onReturnPressed: notificationBanner.dismissRequested()
            Keys.onSpacePressed: notificationBanner.dismissRequested()

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
            }

            // Focus indicator
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: parent.radius + 1
                color: "transparent"
                border.color: Theme.withAlpha(Theme.primary, 0.6)
                border.width: 2
                visible: parent.activeFocus
            }

            MouseArea {
                id: closeBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: notificationBanner.dismissRequested()
            }
        }
    }
}
