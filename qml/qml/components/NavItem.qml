import QtQuick
import QtQuick.Layouts

Item {
    id: navItemRoot
    property string text: ""
    property bool selected: false
    signal clicked()

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.onPressAction: clicked()

    activeFocusOnTab: true
    Layout.preferredWidth: navItemLabel.width + 24
    Layout.fillHeight: true

    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    property real underlineWidth: selected ? 24 : (navItemMouse.containsMouse || activeFocus ? 16 : 0)
    Behavior on underlineWidth {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    Label {
        id: navItemLabel
        anchors.centerIn: parent
        text: navItemRoot.text
        font.pixelSize: Dimensions.fontBody
        font.weight: navItemRoot.selected ? Font.DemiBold : Font.Medium
        color: navItemRoot.selected ? Theme.primary
             : (navItemMouse.containsMouse || navItemRoot.activeFocus) ? Theme.textPrimary
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
