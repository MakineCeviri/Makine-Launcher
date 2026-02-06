import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

/**
 * MinimalTitleBar.qml - Minimal window title bar with controls
 *
 * Features:
 * - 32px height
 * - Turkish flag icon (when translation active)
 * - Window drag support
 * - Pin to tray, Minimize, Maximize, Close buttons
 * - Hover animations
 */
Rectangle {
    id: root

    property bool translationActive: false
    property bool showMaximize: true
    property bool showPinToTray: false
    property bool isDark: true

    signal pinToTrayClicked()
    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()

    implicitHeight: Dimensions.titlebarHeight  // 32
    color: isDark ? Theme.bgPrimary : Theme.lightBackground

    // Drag support
    DragHandler {
        id: dragHandler
        target: null
        onActiveChanged: if (active) root.Window.window.startSystemMove()
    }

    // Double-click to maximize
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: root.maximizeClicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginMD
        spacing: Dimensions.marginSM

        // Turkish flag - 18x18 when translation active
        Rectangle {
            id: flagContainer
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            radius: 2
            visible: root.translationActive
            color: Theme.turkishRed  // Turkish red

            // White crescent
            Rectangle {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: -2
                width: 10
                height: 10
                radius: 5
                color: "white"

                // Inner red circle for crescent effect
                Rectangle {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 2
                    width: 8
                    height: 8
                    radius: Dimensions.radiusStandard
                    color: Theme.turkishRed
                }
            }

            // Star (simplified as 5-point)
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                text: "\u2605"  // Star
                font.pixelSize: 6
                color: "white"
            }
        }

        // Title
        Text {
            text: "MakineAI"
            font.pixelSize: 12
            font.weight: Font.Medium
            color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
        }

        Item { Layout.fillWidth: true }

        // Pin to tray button - Windows 11 style (Segoe MDL2 Assets)
        WindowButton {
            visible: root.showPinToTray
            iconText: "\uE840"  // Segoe MDL2: Pin
            tooltip: "Gizli simgelere küçült"
            hoverColor: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            isDark: root.isDark
            onClicked: root.pinToTrayClicked()
        }

        // Minimize button - Windows 11 style
        WindowButton {
            iconText: "\uE921"  // Segoe MDL2: ChromeMinimize
            tooltip: "Küçült"
            hoverColor: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            isDark: root.isDark
            onClicked: root.minimizeClicked()
        }

        // Maximize button - Windows 11 style
        WindowButton {
            id: maximizeButton
            visible: root.showMaximize
            iconText: root.Window.window && root.Window.window.visibility === Window.Maximized
                ? "\uE923" : "\uE922"  // Segoe MDL2: ChromeRestore or ChromeMaximize
            tooltip: root.Window.window && root.Window.window.visibility === Window.Maximized
                ? "Geri Yükle" : "Ekranı Kapla"
            hoverColor: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            isDark: root.isDark
            onClicked: root.maximizeClicked()
        }

        // Close button - Windows 11 style with red hover
        WindowButton {
            iconText: "\uE8BB"  // Segoe MDL2: ChromeClose
            tooltip: "Kapat"
            hoverColor: Theme.closeButtonHover  // #E81123
            isClose: true
            isDark: root.isDark
            onClicked: root.closeClicked()
        }
    }

}
