import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ToggleSetting.qml - Toggle switch setting row with accessibility
 */
Item {
    id: _toggleRoot
    property string title: ""
    property string description: ""
    property bool checked: false
    property bool disableAnimations: false
    signal toggled()
    activeFocusOnTab: true
    Keys.onReturnPressed: toggled()
    Keys.onSpacePressed: toggled()
    Accessible.role: Accessible.CheckBox
    Accessible.name: title
    Accessible.description: description
    Accessible.checked: checked
    Accessible.onToggleAction: toggled()
    Layout.fillWidth: true
    Layout.preferredHeight: 72
    Rectangle {
        anchors.fill: parent
        color: _toggleMouse.containsMouse ? Theme.primary03 : "transparent"
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true; text: title
                font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                color: Theme.textPrimary; elide: Text.ElideRight
            }
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true; text: description
                font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                elide: Text.ElideRight
            }
        }
        Rectangle {
            id: _toggleTrack
            Layout.preferredWidth: Dimensions.toggleWidth
            Layout.preferredHeight: Dimensions.toggleHeight
            radius: Dimensions.toggleRadius
            color: checked ? Theme.primary : Theme.primary12
            property bool showGlow: _toggleMouse.containsMouse || _toggleRoot.activeFocus
            border.color: showGlow
                ? (checked ? Theme.primary60 : Theme.primary25)
                : "transparent"
            border.width: 1.5
            scale: _toggleMouse.containsMouse ? 1.05 : 1.0
            Behavior on color {
                ColorAnimation {
                    duration: _toggleRoot.disableAnimations ? 0 : 200
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
            Rectangle {
                width: Dimensions.toggleKnobSize
                height: Dimensions.toggleKnobSize
                radius: Dimensions.toggleKnobRadius
                color: Theme.textOnColor
                x: checked ? parent.width - width - 3 : 3
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    anchors.fill: parent; anchors.margins: -1
                    radius: Dimensions.radiusStandard
                    color: "transparent"
                    border.color: Theme.bgPrimary15
                    border.width: 1; z: -1
                }
                Behavior on x {
                    NumberAnimation {
                        duration: _toggleRoot.disableAnimations ? 0 : 200
                        easing.type: Easing.OutCubic
                    }
                }
                scale: _toggleMouse.pressed ? 0.85 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }
            }
            MouseArea {
                id: _toggleMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: _toggleRoot.toggled()
            }
        }
    }
}
