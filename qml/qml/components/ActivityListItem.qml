import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * ActivityListItem.qml - Activity list item (Native Qt)
 */
Rectangle {
    id: root

    property var model: ({}) // message, type, time

    Layout.fillWidth: true
    implicitHeight: 32
    color: "transparent"
    radius: Dimensions.radiusStandard

    // Hover effect
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

        Behavior on color { ColorAnimation { duration: 150 } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        spacing: 12

        // Status dot
        Rectangle {
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            Layout.alignment: Qt.AlignVCenter
            radius: 3
            color: root.model.type === "success" ? Theme.success :
                   root.model.type === "error" ? Theme.error :
                   root.model.type === "warning" ? Theme.warning : Theme.primary
        }

        // Message
        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.model.message || ""
            font.pixelSize: 12
            color: Theme.textSecondary
            elide: Text.ElideRight
        }

        // Timestamp
        Text {
            Layout.alignment: Qt.AlignVCenter
            text: root.model.time || ""
            font.pixelSize: 10
            color: Theme.textMuted
        }
    }
}
