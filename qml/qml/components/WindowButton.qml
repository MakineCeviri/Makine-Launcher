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
    property color hoverColor: Theme.withAlpha(Theme.textPrimary, 0.1)
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
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

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
    // FOCUS INDICATOR
    // =========================================================================

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: Dimensions.radiusStandard
        color: "transparent"
        border.color: Theme.withAlpha(Theme.primary, 0.6)
        border.width: 2
        visible: root.activeFocus
    }

    // =========================================================================
    // TOOLTIP
    // =========================================================================

    ToolTip.visible: root.tooltip.length > 0 && isHovered
    ToolTip.delay: 500
    ToolTip.text: root.tooltip
}
