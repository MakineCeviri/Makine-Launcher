import QtQuick
import QtQuick.Layouts

Item {
    id: navItemRoot
    property string text: ""
    property bool selected: false
    signal clicked()

    Layout.preferredWidth: navItemLabel.width + 24
    Layout.fillHeight: true

    property real underlineWidth: selected ? 24 : (navItemMouse.containsMouse ? 16 : 0)
    Behavior on underlineWidth {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    Label {
        id: navItemLabel
        anchors.centerIn: parent
        text: navItemRoot.text
        font.pixelSize: 13
        font.weight: navItemRoot.selected ? Font.DemiBold : Font.Medium
        color: navItemRoot.selected ? Theme.primary
             : navItemMouse.containsMouse ? Theme.textPrimary
             : Theme.textSecondary

        Behavior on color { ColorAnimation { duration: 150 } }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: navItemRoot.underlineWidth
        height: 2
        radius: 1
        color: navItemRoot.selected
            ? Theme.primary
            : Theme.withAlpha(Theme.textPrimary, 0.4)
        visible: width > 0
    }

    MouseArea {
        id: navItemMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navItemRoot.clicked()
    }
}
