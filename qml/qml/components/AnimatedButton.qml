import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * AnimatedButton.qml - Native Qt AnimatedButton birebir port
 * Kaynak: ui/src/widgets/animatedbutton.cpp
 *
 * Features:
 * - Primary/Secondary/Ghost variants
 * - Hover scale animation
 * - Press animation
 * - Optional icon
 */
Rectangle {
    id: root

    property string text: "Button"
    property string icon: ""
    property bool isPrimary: true
    property bool isGhost: false
    property bool enabled: true

    signal clicked()

    implicitWidth: buttonContent.width + 48
    implicitHeight: 44
    radius: Dimensions.radiusMD  // 8

    // Colors based on variant
    color: {
        if (!enabled) return Qt.rgba(1, 1, 1, 0.05)
        if (isGhost) {
            if (mouseArea.pressed) return Qt.rgba(1, 1, 1, 0.08)
            if (mouseArea.containsMouse) return Qt.rgba(1, 1, 1, 0.05)
            return "transparent"
        }
        if (isPrimary) {
            if (mouseArea.pressed) return Qt.darker(Theme.primary, 1.1)
            if (mouseArea.containsMouse) return Theme.primaryHover
            return Theme.primary
        }
        // Secondary
        if (mouseArea.pressed) return Qt.rgba(1, 1, 1, 0.15)
        if (mouseArea.containsMouse) return Qt.rgba(1, 1, 1, 0.12)
        return Qt.rgba(1, 1, 1, 0.1)
    }

    border.color: {
        if (!enabled) return Qt.rgba(1, 1, 1, 0.08)
        if (isGhost) return Qt.rgba(1, 1, 1, mouseArea.containsMouse ? 0.2 : 0.1)
        if (isPrimary) return "transparent"
        return Qt.rgba(1, 1, 1, mouseArea.containsMouse ? 0.25 : 0.2)
    }
    border.width: isGhost || !isPrimary ? 1 : 0

    scale: mouseArea.pressed ? 0.97 : (mouseArea.containsMouse && enabled ? 1.02 : 1.0)

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    opacity: enabled ? 1.0 : 0.5

    Row {
        id: buttonContent
        anchors.centerIn: parent
        spacing: 8

        // Icon (optional)
        Text {
            visible: root.icon !== ""
            text: root.icon
            font.pixelSize: 16
            color: {
                if (!root.enabled) return Theme.textDisabled
                if (root.isPrimary && !root.isGhost) return "white"
                return Theme.textPrimary
            }
            anchors.verticalCenter: parent.verticalCenter
        }

        // Text - Native Qt: fontSize 14, w600
        Text {
            text: root.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: {
                if (!root.enabled) return Theme.textDisabled
                if (root.isPrimary && !root.isGhost) return "white"
                return Theme.textPrimary
            }
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.enabled) root.clicked()
    }
}
