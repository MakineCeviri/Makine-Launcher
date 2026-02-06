import QtQuick

/**
 * HoverBuilder.qml - Flutter hover_builder.dart birebir port
 *
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/hover_builder.dart
 *
 * Flutter'daki gibi hover state yonetimi icin optimize edilmis widget.
 * ValueNotifier yerine QML property binding kullaniyor.
 *
 * Kullanim:
 * HoverBuilder {
 *     id: myHoverBuilder
 *     onClicked: { console.log("Clicked!") }
 *
 *     Rectangle {
 *         color: myHoverBuilder.isHovered ? "red" : "blue"
 *         // ...
 *     }
 * }
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter HoverBuilder birebir
    // =========================================================================

    /// Mouse hover durumu - Flutter: ValueNotifier<bool>
    readonly property bool isHovered: mouseArea.containsMouse

    /// Mouse press durumu - ekstra
    readonly property bool isPressed: mouseArea.pressed

    /// Cursor tipi - Flutter: MouseCursor cursor
    property int cursorShape: Qt.PointingHandCursor

    /// Hover animasyon suresi - Flutter'da 150ms varsayilan
    property int hoverDuration: 150

    /// Aktif mi (tiklanabilir mi)
    property bool enabled: true

    // =========================================================================
    // SIGNALS - Flutter VoidCallback? onTap karsiligi
    // =========================================================================

    /// Tiklandiginda tetiklenir
    signal clicked()

    /// Sag tiklandiginda tetiklenir
    signal rightClicked()

    /// Cift tiklandiginda tetiklenir
    signal doubleClicked()

    /// Hover durumu degistiginde tetiklenir
    signal hoverChanged(bool hovered)

    /// Press durumu degistiginde tetiklenir
    signal pressChanged(bool pressed)

    // =========================================================================
    // INTERNAL STATE
    // =========================================================================

    /// Hover animasyonu icin interpolasyon degeri (0.0 - 1.0)
    property real hoverProgress: isHovered ? 1.0 : 0.0

    Behavior on hoverProgress {
        NumberAnimation {
            duration: root.hoverDuration
            easing.type: Easing.OutQuad
        }
    }

    // =========================================================================
    // MOUSE AREA - Flutter MouseRegion + GestureDetector karsiligi
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
     * Hover durumuna gore iki renk arasinda interpolasyon yapar
     * Flutter: Color.lerp() karsiligi
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
     * Hover durumuna gore iki deger arasinda interpolasyon yapar
     * Flutter: lerpDouble() karsiligi
     */
    function lerpValue(normalValue, hoverValue) {
        return normalValue + (hoverValue - normalValue) * hoverProgress
    }

    /**
     * Press durumuna gore scale degeri dondurur
     * Flutter'daki AnimatedContainer transform karsiligi
     */
    function getScale(normalScale, hoverScale, pressScale) {
        if (isPressed) return pressScale
        return normalScale + (hoverScale - normalScale) * hoverProgress
    }
}
