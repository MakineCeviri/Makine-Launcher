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

    property real underlineWidth: selected ? 24 : (navItemMouse.containsMouse || activeFocus ? 16 : 0)
    Behavior on underlineWidth {
        NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic }
    }

    // Animated phase for gradient underline
    property real animPhase: 0
    NumberAnimation on animPhase {
        from: 0; to: 1
        duration: 3000
        loops: Animation.Infinite
        running: navItemRoot.underlineWidth > 0
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

    // Animated gradient underline (soft pastel colors)
    Item {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: navItemRoot.underlineWidth
        height: 2
        clip: true
        visible: width > 0

        Rectangle {
            width: parent.width * 2
            height: parent.height
            radius: 1
            x: -parent.width * navItemRoot.animPhase

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.000; color: "#E8D5A0" }
                GradientStop { position: 0.056; color: "#DDBFA0" }
                GradientStop { position: 0.111; color: "#D4ABA8" }
                GradientStop { position: 0.167; color: "#BBADC0" }
                GradientStop { position: 0.222; color: "#A0B8CC" }
                GradientStop { position: 0.278; color: "#94C8BE" }
                GradientStop { position: 0.333; color: "#9CCAAA" }
                GradientStop { position: 0.389; color: "#B5CC9A" }
                GradientStop { position: 0.444; color: "#C0B598" }
                GradientStop { position: 0.500; color: "#E8D5A0" }
                GradientStop { position: 0.556; color: "#DDBFA0" }
                GradientStop { position: 0.611; color: "#D4ABA8" }
                GradientStop { position: 0.667; color: "#BBADC0" }
                GradientStop { position: 0.722; color: "#A0B8CC" }
                GradientStop { position: 0.778; color: "#94C8BE" }
                GradientStop { position: 0.833; color: "#9CCAAA" }
                GradientStop { position: 0.889; color: "#B5CC9A" }
                GradientStop { position: 0.944; color: "#C0B598" }
                GradientStop { position: 1.000; color: "#E8D5A0" }
            }
        }
    }

    MouseArea {
        id: navItemMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navItemRoot.clicked()
    }
}
