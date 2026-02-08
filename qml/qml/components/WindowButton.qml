import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * WindowButton.qml - Title bar window control button (minimize, maximize, close)
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    property string iconText: ""
    property color hoverColor: Qt.rgba(1, 1, 1, 0.1)
    property bool isClose: false
    property string tooltip: ""
    property bool isDark: true

    // =========================================================================
    // SIZE
    // =========================================================================

    width: 46
    height: 32

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.tooltip

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // MAIN BUTTON
    // =========================================================================

    Rectangle {
        id: buttonBackground
        anchors.fill: parent
        color: root.isHovered ? root.hoverColor : "transparent"

        Behavior on color {
            ColorAnimation { duration: Dimensions.animFast }
        }

        Text {
            anchors.centerIn: parent
            text: root.iconText
            font.pixelSize: Dimensions.fontLG
            font.family: "Segoe MDL2 Assets"  // Windows icon font

            color: {
                if (root.isHovered && root.isClose) {
                    return "white"
                }
                return root.isDark ? Theme.textSecondary : Theme.lightTextSecondary
            }

            Behavior on color {
                ColorAnimation { duration: Dimensions.animFast }
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
        cursorShape: Qt.ArrowCursor  // Windows butonları için normal cursor

        onClicked: root.clicked()
    }

    // =========================================================================
    // TOOLTIP
    // =========================================================================

    ToolTip.visible: root.tooltip.length > 0 && isHovered
    ToolTip.delay: 500
    ToolTip.text: root.tooltip
}
