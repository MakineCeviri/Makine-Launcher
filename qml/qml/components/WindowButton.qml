import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * WindowButton.qml - Flutter window_button.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/window_button.dart
 *
 * TitleBar pencere kontrol butonlari (minimize, maximize, close)
 *
 * Ozellikler:
 * - Flutter boyutlari: width 46, height 32
 * - Hover renk animasyonu
 * - isClose: true ise hover'da beyaz icon
 * - Tooltip destegi
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter WindowButton birebir
    // =========================================================================

    /// Icon karakter veya icon name
    property string iconText: ""

    /// Hover arka plan rengi - Flutter: hoverColor
    property color hoverColor: Qt.rgba(1, 1, 1, 0.1)

    /// Close button mu? - Flutter: isClose
    /// Hover'da icon beyaz olur
    property bool isClose: false

    /// Tooltip metni - Flutter: tooltip
    property string tooltip: ""

    /// Tema modu - Flutter: isDark
    property bool isDark: true

    // =========================================================================
    // SIZE - Flutter: width 46, height 32
    // =========================================================================

    width: 46
    height: 32

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

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
            ColorAnimation { duration: 150 }  // Flutter: shortAnimationDuration
        }

        // Icon - Flutter: size 16
        Text {
            anchors.centerIn: parent
            text: root.iconText
            font.pixelSize: 16
            font.family: "Segoe MDL2 Assets"  // Windows icon font

            // Flutter: isHovered && isClose ? white : textSecondary
            color: {
                if (root.isHovered && root.isClose) {
                    return "white"
                }
                return root.isDark ? Theme.textSecondary : Theme.lightTextSecondary
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
        cursorShape: Qt.ArrowCursor  // Windows butonları için normal cursor

        onClicked: root.clicked()
    }

    // =========================================================================
    // TOOLTIP - Flutter: Tooltip widget
    // =========================================================================

    ToolTip.visible: root.tooltip.length > 0 && isHovered
    ToolTip.delay: 500
    ToolTip.text: root.tooltip
}
