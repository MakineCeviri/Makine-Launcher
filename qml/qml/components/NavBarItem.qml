import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * NavBarItem.qml - Navigation bar item with icon, label, and animated underline
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    /// Icon karakter veya emoji
    property string iconText: ""

    /// Label metni
    property string label: ""

    /// Whether this item is selected
    property bool isSelected: false

    /// Dark/light theme mode
    property bool isDark: true

    /// Whether to show the label text
    property bool showLabel: true

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.label
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    // =========================================================================
    // SIZE
    // =========================================================================

    implicitWidth: contentRow.width + 24
    implicitHeight: contentColumn.height

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // COLOR HELPERS
    // =========================================================================

    function getIconColor() {
        if (root.isSelected) return Theme.primary
        if (root.isHovered) return root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
        return root.isDark ? Theme.textMuted : Theme.lightTextMuted
    }

    function getTextColor() {
        if (root.isSelected) return Theme.primary
        if (root.isHovered) return root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
        return root.isDark ? Theme.textSecondary : Theme.lightTextSecondary
    }

    // =========================================================================
    // CONTENT
    // =========================================================================

    Column {
        id: contentColumn
        anchors.centerIn: parent
        spacing: 0

        Item {
            width: contentRow.width + 24
            height: contentRow.height + 16

            // Background highlight for active/hover state
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 6
                color: root.isSelected
                    ? Theme.withAlpha(Theme.primary, 0.12)
                    : (root.isHovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
            }

            Row {
                id: contentRow
                anchors.centerIn: parent
                spacing: root.showLabel ? 8 : 0
                Text {
                    id: iconItem
                    text: root.iconText
                    font.pixelSize: 18
                    color: getIconColor()
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                Text {
                    id: labelItem
                    visible: root.showLabel
                    text: root.label
                    font.pixelSize: 13
                    font.weight: root.isSelected ? Font.DemiBold : Font.Medium
                    color: getTextColor()
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
            }
        }

        // Animated underline indicator
        Rectangle {
            id: underline
            anchors.horizontalCenter: parent.horizontalCenter

            // Enhanced: wider underline when selected
            width: root.isSelected ? 32 : (root.isHovered ? 20 : 0)
            height: root.isSelected ? 3 : 2  // Thicker when selected

            // Enhanced: glow effect for selected state
            color: root.isSelected
                ? Theme.primary
                : (root.isDark
                    ? Qt.rgba(1, 1, 1, 0.4)
                    : Qt.rgba(0, 0, 0, 0.3))

            radius: height / 2

            Behavior on width {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }

            Behavior on height {
                NumberAnimation { duration: 150 }
            }

            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            // Subtle glow behind underline when selected
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 8
                height: parent.height + 4
                radius: height / 2
                color: Theme.primary
                opacity: root.isSelected ? 0.3 : 0
                z: -1

                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }
            }
        }
    }

    // =========================================================================
    // MOUSE AREA
    // =========================================================================

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
