import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * NotificationBell.qml - Bell icon with animated unread count badge
 */
Item {
    id: root

    property int unreadCount: 0
    property bool panelOpen: false

    signal clicked()

    implicitWidth: 36
    implicitHeight: 36

    // Bell icon
    Image {
        id: bellIcon
        anchors.centerIn: parent
        width: 20; height: 20
        source: "qrc:/qt/qml/MakineAI/resources/icons/bell.svg"
        sourceSize: Qt.size(20, 20)
        visible: false
    }

    MultiEffect {
        anchors.fill: bellIcon
        source: bellIcon
        colorization: 1.0
        colorizationColor: mouseArea.containsMouse || root.panelOpen
            ? Theme.textPrimary
            : Theme.textSecondary

        Behavior on colorizationColor {
            ColorAnimation { duration: 150 }
        }
    }

    // Unread count badge
    Rectangle {
        id: badge
        visible: root.unreadCount > 0
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 2
        anchors.rightMargin: 2
        width: root.unreadCount > 9 ? 18 : 16
        height: 16
        radius: 8
        color: Theme.error

        scale: visible ? 1.0 : 0.0
        Behavior on scale {
            NumberAnimation { duration: 200; easing.type: Easing.OutBack }
        }

        Text {
            anchors.centerIn: parent
            text: root.unreadCount > 99 ? "99+" : root.unreadCount.toString()
            font.pixelSize: 9
            font.weight: Font.Bold
            color: "#FFFFFF"
        }
    }

    // Hover background
    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusStandard
        color: mouseArea.containsMouse
            ? Theme.withAlpha(Theme.primary, 0.1)
            : "transparent"

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip {
        visible: mouseArea.containsMouse
        text: root.unreadCount > 0
            ? qsTr("%1 yeni bildirim").arg(root.unreadCount)
            : qsTr("Bildirimler")
        delay: 400
    }

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Notifications")
}
