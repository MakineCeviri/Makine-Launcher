import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * DonateButton.qml - Flutter donate_button.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/donate_button.dart
 *
 * Animasyonlu gradient'li bagis butonu
 *
 * Ozellikler:
 * - 2000ms gold-olive renk animasyonu
 * - Sinusoidal gecis
 * - Hover glow efekti
 * - Heart icon
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    /// Tema modu
    property bool isDark: true

    /// Animasyonlari devre disi birak
    property bool disableAnimations: false

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // SIZE
    // =========================================================================

    implicitWidth: contentRow.width + 24  // Flutter: horizontal padding 12 * 2
    implicitHeight: contentRow.height + 16  // Flutter: vertical padding 8 * 2

    // =========================================================================
    // HOVER STATE
    // =========================================================================

    readonly property bool isHovered: mouseArea.containsMouse

    // =========================================================================
    // ANIMATION - Flutter: 2000ms repeat
    // =========================================================================

    property real _animValue: 0.0

    NumberAnimation on _animValue {
        id: colorAnimation
        from: 0.0
        to: 1.0
        duration: 2000  // Flutter: 2000ms
        loops: Animation.Infinite
        running: !root.disableAnimations
    }

    // Flutter: Yumusak sinusoidal gecis
    // (1 - ((value * 2 - 1).abs())).clamp(0.0, 1.0)
    readonly property real smoothValue: {
        if (root.disableAnimations) return 0.0
        var v = _animValue * 2 - 1  // -1 to 1
        return 1 - Math.abs(v)  // 0 to 1 to 0 (smooth)
    }

    // =========================================================================
    // COLOR INTERPOLATION - Flutter Color.lerp
    // =========================================================================

    // Flutter: color1 = gold -> olive, color2 = olive -> gold
    readonly property color color1: Theme.lerpColor(Theme.gold, Theme.olive, smoothValue)
    readonly property color color2: Theme.lerpColor(Theme.olive, Theme.gold, smoothValue)

    // Flutter: 0.25 + (smoothValue * 0.25)
    readonly property real glowIntensity: 0.25 + (smoothValue * 0.25)

    // =========================================================================
    // GLOW - Flutter: BoxShadow
    // =========================================================================

    // Glow layer - Flutter: blurRadius hover ? 20 : (10 + smoothValue * 6)
    Rectangle {
        anchors.centerIn: buttonBg
        width: buttonBg.width + (isHovered ? 40 : (20 + smoothValue * 12))
        height: buttonBg.height + (isHovered ? 40 : (20 + smoothValue * 12))
        anchors.verticalCenterOffset: 4  // Flutter: offset(0, 4)
        radius: 4 + 10
        color: Qt.rgba(color1.r, color1.g, color1.b, isHovered ? 0.6 : glowIntensity)
        z: -1

        Behavior on width { NumberAnimation { duration: 200 } }
        Behavior on height { NumberAnimation { duration: 200 } }
        Behavior on color { ColorAnimation { duration: 200 } }
    }

    // =========================================================================
    // BUTTON BACKGROUND - Flutter: gradient, borderRadius 4
    // =========================================================================

    Rectangle {
        id: buttonBg
        anchors.fill: parent
        radius: 4  // Flutter: borderRadius 4

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.color1 }
            GradientStop { position: 1.0; color: root.color2 }
        }
    }

    // =========================================================================
    // CONTENT - Flutter: heart icon + text
    // =========================================================================

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8  // Flutter: SizedBox(width: 8)

        // Heart icon - Flutter: Icons.favorite_rounded, size 18
        Text {
            text: "\u2764"  // Heart symbol
            font.pixelSize: 18
            color: "white"
            anchors.verticalCenter: parent.verticalCenter
        }

        // Label - Flutter: "Destekci Ol", fontSize 14, w600
        Text {
            text: "Destekci Ol"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "white"
            anchors.verticalCenter: parent.verticalCenter
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
