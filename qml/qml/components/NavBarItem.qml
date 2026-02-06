import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * NavBarItem.qml - Flutter nav_bar_item.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/nav_bar_item.dart
 *
 * Horizontal navbar icin navigation item
 *
 * Ozellikler:
 * - Icon + label
 * - Selected/hover state
 * - Animasyonlu underline
 * - Label gizlenebilir
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter NavBarItem birebir
    // =========================================================================

    /// Icon karakter veya emoji
    property string iconText: ""

    /// Label metni
    property string label: ""

    /// Secili mi - Flutter: isSelected
    property bool isSelected: false

    /// Tema modu - Flutter: isDark
    property bool isDark: true

    /// Label gosterilsin mi - Flutter: showLabel
    property bool showLabel: true

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // SIZE
    // =========================================================================

    implicitWidth: contentRow.width + 24  // Flutter: horizontal padding 12 * 2
    implicitHeight: contentColumn.height

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // COLOR HELPERS - Flutter _getIconColor ve _getTextColor
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

        // Flutter: Padding(horizontal: 12, vertical: 8)
        Item {
            width: contentRow.width + 24
            height: contentRow.height + 16

            Row {
                id: contentRow
                anchors.centerIn: parent
                spacing: root.showLabel ? 8 : 0  // Flutter: SizedBox(width: 8)

                // Icon - Flutter: size 18
                Text {
                    id: iconItem
                    text: root.iconText
                    font.pixelSize: 18  // Flutter: size 18
                    color: getIconColor()
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                // Label - Flutter: fontSize 13, w500/w600
                Text {
                    id: labelItem
                    visible: root.showLabel
                    text: root.label
                    font.pixelSize: 13  // Flutter: fontSize 13
                    font.weight: root.isSelected ? Font.DemiBold : Font.Medium  // Flutter: w600/w500
                    color: getTextColor()
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
            }
        }

        // Underline - Flutter: AnimatedContainer
        Rectangle {
            id: underline
            anchors.horizontalCenter: parent.horizontalCenter

            // Flutter: selected ? 24 : (hovered ? 16 : 0)
            width: root.isSelected ? 24 : (root.isHovered ? 16 : 0)
            height: 2  // Flutter: height 2

            // Flutter: selected ? primary : white 40%
            color: root.isSelected
                ? Theme.primary
                : (root.isDark
                    ? Qt.rgba(1, 1, 1, 0.4)
                    : Qt.rgba(0, 0, 0, 0.3))

            radius: 1  // Flutter: borderRadius 1

            Behavior on width {
                NumberAnimation { duration: 150 }  // Flutter: 150ms
            }

            Behavior on color {
                ColorAnimation { duration: 150 }
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
