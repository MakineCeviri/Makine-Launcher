import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * NotificationToast.qml - Animated toast notification popup
 *
 * Slides in from the top-right corner, auto-dismisses after a timeout.
 * Supports different notification types with distinct styling.
 */
Item {
    id: root

    property string title: ""
    property string message: ""
    property string type: "info" // info, update, success, warning, error, translation
    property int duration: 5000
    property bool showing: false

    signal clicked()
    signal dismissed()

    Accessible.role: Accessible.Button
    Accessible.name: root.title
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    anchors.right: parent ? parent.right : undefined
    anchors.top: parent ? parent.top : undefined
    anchors.rightMargin: 16
    anchors.topMargin: 16
    width: 320
    height: toastContent.implicitHeight + 24
    z: 200
    visible: opacity > 0
    opacity: showing ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
    }

    transform: Translate {
        y: root.showing ? 0 : -20
        Behavior on y {
            NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
        }
    }

    function show(notifTitle, notifMessage, notifType, notifDuration) {
        root.title = notifTitle || ""
        root.message = notifMessage || ""
        root.type = notifType || "info"
        root.duration = notifDuration || 5000
        root.showing = true
        dismissTimer.restart()
    }

    function dismiss() {
        root.showing = false
        dismissTimer.stop()
        root.dismissed()
    }

    Timer {
        id: dismissTimer
        interval: root.duration
        onTriggered: root.dismiss()
    }

    // Background
    Rectangle {
        id: toastBg
        anchors.fill: parent
        radius: Dimensions.radiusLG
        color: Theme.surface
        border.color: {
            switch(root.type) {
                case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.4)
                case "success": return Theme.withAlpha(Theme.success, 0.4)
                case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.4)
                case "error": return Theme.withAlpha(Theme.notificationError, 0.4)
                default: return Qt.rgba(1, 1, 1, 0.1)
            }
        }
        border.width: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowBlur: 0.8
            shadowVerticalOffset: 4
            shadowHorizontalOffset: 0
        }
    }

    // Left accent line
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 1
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        width: 3
        radius: 2
        color: {
            switch(root.type) {
                case "update": return Theme.notificationUpdate
                case "success": return Theme.success
                case "warning": return Theme.notificationWarning
                case "error": return Theme.notificationError
                case "translation": return Theme.primary
                default: return Theme.primary
            }
        }
    }

    // Content
    RowLayout {
        id: toastContent
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 10

        // Type icon
        Rectangle {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignTop
            radius: 6
            color: {
                switch(root.type) {
                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.15)
                    case "success": return Theme.withAlpha(Theme.success, 0.15)
                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.15)
                    case "error": return Theme.withAlpha(Theme.notificationError, 0.15)
                    default: return Theme.withAlpha(Theme.primary, 0.15)
                }
            }

            Text {
                anchors.centerIn: parent
                font.pixelSize: Dimensions.fontBody
                text: {
                    switch(root.type) {
                        case "update": return "\u2B06"
                        case "success": return "\u2714"
                        case "warning": return "\u26A0"
                        case "error": return "\u2716"
                        case "translation": return "\uD83C\uDF0D"
                        default: return "\u2139"
                    }
                }
            }
        }

        // Text content
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                text: root.message
                font.pixelSize: Dimensions.fontXS
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                visible: text !== ""
            }
        }

        // Close button
        Item {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignTop

            Rectangle {
                anchors.fill: parent
                radius: Dimensions.radiusXS
                color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                font.pixelSize: Dimensions.fontCaption
                color: Theme.textMuted
                opacity: closeMouse.containsMouse ? 1.0 : 0.5
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.dismiss()
            }
        }
    }

    // Progress bar for auto-dismiss
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 2
        anchors.leftMargin: 8
        height: 2
        radius: 1
        width: (parent.width - 16) * progressAnim.progress
        color: Theme.withAlpha(Theme.textMuted, 0.3)
        visible: root.showing

        Item {
            id: progressAnim
            property real progress: root.showing ? 0.0 : 1.0

            NumberAnimation on progress {
                running: root.showing
                from: 1.0
                to: 0.0
                duration: root.duration
            }
        }
    }

    // Focus indicator
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: Dimensions.radiusLG + 1
        color: "transparent"
        border.color: Theme.withAlpha(Theme.primary, 0.6)
        border.width: 2
        visible: root.activeFocus
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: root.clicked()
        cursorShape: Qt.PointingHandCursor
    }
}
