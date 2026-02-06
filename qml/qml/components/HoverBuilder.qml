import QtQuick

/**
 * HoverBuilder.qml - Reusable hover state manager with animated transitions
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    /// Mouse hover state
    readonly property bool isHovered: mouseArea.containsMouse

    /// Mouse press state
    readonly property bool isPressed: mouseArea.pressed

    /// Cursor shape
    property int cursorShape: Qt.PointingHandCursor

    /// Hover animation duration in milliseconds
    property int hoverDuration: 150

    /// Whether the component is enabled (clickable)
    property bool enabled: true

    // =========================================================================
    // SIGNALS
    // =========================================================================

    /// Emitted when clicked
    signal clicked()

    /// Emitted when right-clicked
    signal rightClicked()

    /// Emitted when double-clicked
    signal doubleClicked()

    /// Emitted when hover state changes
    signal hoverChanged(bool hovered)

    /// Emitted when press state changes
    signal pressChanged(bool pressed)

    // =========================================================================
    // INTERNAL STATE
    // =========================================================================

    /// Interpolation value for hover animation (0.0 - 1.0)
    property real hoverProgress: isHovered ? 1.0 : 0.0

    Behavior on hoverProgress {
        NumberAnimation {
            duration: root.hoverDuration
            easing.type: Easing.OutQuad
        }
    }

    // =========================================================================
    // MOUSE AREA
    // =========================================================================

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? root.cursorShape : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            if (!root.enabled) return

            if (mouse.button === Qt.RightButton) {
                root.rightClicked()
            } else {
                root.clicked()
            }
        }

        onDoubleClicked: {
            if (!root.enabled) return
            root.doubleClicked()
        }

        onContainsMouseChanged: {
            root.hoverChanged(containsMouse)
        }

        onPressedChanged: {
            root.pressChanged(pressed)
        }
    }

    // =========================================================================
    // HELPER FUNCTIONS
    // =========================================================================

    /**
     * Interpolates between two colors based on hover state
     */
    function lerpColor(normalColor, hoverColor) {
        return Qt.rgba(
            normalColor.r + (hoverColor.r - normalColor.r) * hoverProgress,
            normalColor.g + (hoverColor.g - normalColor.g) * hoverProgress,
            normalColor.b + (hoverColor.b - normalColor.b) * hoverProgress,
            normalColor.a + (hoverColor.a - normalColor.a) * hoverProgress
        )
    }

    /**
     * Interpolates between two values based on hover state
     */
    function lerpValue(normalValue, hoverValue) {
        return normalValue + (hoverValue - normalValue) * hoverProgress
    }

    /**
     * Returns a scale value based on press and hover state
     */
    function getScale(normalScale, hoverScale, pressScale) {
        if (isPressed) return pressScale
        return normalScale + (hoverScale - normalScale) * hoverProgress
    }
}
