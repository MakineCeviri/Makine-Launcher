import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Item {
    id: navItemRoot
    property string text: ""
    property bool selected: false
    signal clicked()

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.onPressAction: clicked()

    activeFocusOnTab: true
    Layout.preferredWidth: navItemLabel.width + 28
    Layout.fillHeight: true

    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    Text {
        textFormat: Text.PlainText
        id: navItemLabel
        anchors.centerIn: parent
        text: navItemRoot.text
        font.pixelSize: Dimensions.fontBody
        font.weight: navItemRoot.selected ? Font.DemiBold : Font.Medium
        color: navItemRoot.selected ? Theme.textPrimary
             : (navItemMouse.containsMouse || navItemRoot.activeFocus) ? Theme.textPrimary
             : Theme.textSecondary

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    }

    // Underline indicator — expands on select, shrinks on deselect
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: navItemRoot.selected ? navItemLabel.width + 8 : 0
        height: 2
        radius: 1
        color: Theme.textPrimary
        opacity: navItemRoot.selected ? 1.0 : 0.0

        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: navItemMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navItemRoot.clicked()
    }
}
