import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

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

    // Indicator width: full on selected, partial on hover, zero otherwise
    property real underlineWidth: selected ? 20 : (navItemMouse.containsMouse || activeFocus ? 12 : 0)
    Behavior on underlineWidth {
        NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic }
    }

    // Indicator opacity
    property real underlineOpacity: selected ? 1.0 : (navItemMouse.containsMouse || activeFocus ? 0.5 : 0)
    Behavior on underlineOpacity {
        NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic }
    }

    Text {
        id: navItemLabel
        anchors.centerIn: parent
        text: navItemRoot.text
        font.pixelSize: Dimensions.fontBody
        font.weight: navItemRoot.selected ? Font.DemiBold : Font.Medium
        color: navItemRoot.selected ? Theme.primary
             : (navItemMouse.containsMouse || navItemRoot.activeFocus) ? Theme.textPrimary
             : Theme.textSecondary

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    }

    // Minimal accent underline
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: navItemRoot.underlineWidth
        height: 1.5
        radius: 0.75
        color: Theme.primary
        opacity: navItemRoot.underlineOpacity
    }

    MouseArea {
        id: navItemMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navItemRoot.clicked()
    }
}
