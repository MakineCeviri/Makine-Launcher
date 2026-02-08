import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string description: ""
    property bool checked: false
    signal toggled()

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.title
    Accessible.checked: root.checked
    activeFocusOnTab: true
    Keys.onReturnPressed: { root.checked = !root.checked; root.toggled() }
    Keys.onSpacePressed: { root.checked = !root.checked; root.toggled() }

    Layout.fillWidth: true
    Layout.preferredHeight: 72
    color: mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
    radius: Dimensions.radiusStandard

    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    // Focus indicator
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: root.radius + 1
        color: "transparent"
        border.color: Theme.withAlpha(Theme.primary, 0.6)
        border.width: 2
        visible: root.activeFocus
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL

        // Content (title and subtitle)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            Label {
                text: root.title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Label {
                text: root.description
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }
        }

        // Custom Square Toggle - Kare görünüm, hafif oval köşe
        Rectangle {
            id: customToggle
            width: Dimensions.toggleWidth
            height: Dimensions.toggleHeight
            radius: Dimensions.toggleRadius
            color: root.checked ? Theme.primary : Theme.withAlpha(Theme.textMuted, 0.3)

            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            // Knob (kare görünümlü)
            Rectangle {
                id: knob
                width: Dimensions.toggleKnobSize
                height: Dimensions.toggleKnobSize
                radius: Dimensions.toggleKnobRadius
                x: root.checked ? parent.width - width - 3 : 3
                y: (parent.height - height) / 2
                color: "white"

                Behavior on x {
                    NumberAnimation {
                        duration: Dimensions.animFast
                        easing.type: Easing.OutCubic
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.checked = !root.checked
                    root.toggled()
                }
            }
        }
    }
}
