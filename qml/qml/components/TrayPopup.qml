import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TrayPopup.qml — Premium dark context menu for system tray
 *
 * Frameless, always-on-top window matching the app's dark theme.
 * Positioned near the tray icon and dismisses on click outside.
 */
Window {
    id: popup

    property string appVersion: Qt.application.version

    signal showRequested()
    signal checkUpdatesRequested()
    signal settingsRequested()
    signal quitRequested()

    width: 220
    height: menuCol.implicitHeight + 2 * _pad
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.NoDropShadowWindowHint
    color: "transparent"
    visible: false

    readonly property int _pad: 6
    readonly property int _itemH: 34

    function showAt(screenX: int, screenY: int) {
        // Position above the click point (taskbar is usually at bottom)
        var popW = popup.width
        var popH = popup.height

        var posX = screenX - popW / 2
        var posY = screenY - popH - 8

        // Clamp: if it would go above screen top, show below cursor instead
        if (posY < 0)
            posY = screenY + 8

        popup.x = posX
        popup.y = posY

        // Reset animation state before showing
        bg.opacity = 0
        bg.scale = 0.96

        popup._hadFocus = false
        popup.visible = true
        popup.requestActivate()
        fadeIn.restart()
        fadeScale.restart()
    }

    function dismiss() {
        if (visible)
            popup.visible = false
    }

    // Track whether the popup has received focus at least once after showing.
    // Without this guard, the dismiss timer fires immediately on slow systems
    // before AllowSetForegroundWindow + requestActivate() has taken effect.
    property bool _hadFocus: false

    onActiveChanged: {
        if (active) {
            _hadFocus = true
            dismissTimer.stop()
        } else if (visible && _hadFocus) {
            dismissTimer.restart()
        }
    }

    // Small delay prevents premature dismiss on W10 activation quirks
    Timer {
        id: dismissTimer
        interval: 250
        onTriggered: {
            if (!popup.active)
                popup.dismiss()
        }
    }

    NumberAnimation {
        id: fadeIn
        target: bg
        property: "opacity"
        from: 0; to: 1
        duration: 120
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: fadeScale
        target: bg
        property: "scale"
        from: 0.96; to: 1.0
        duration: 120
        easing.type: Easing.OutCubic
    }

    // Main container
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 10
        color: "#1C1C20"
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        opacity: 0
        scale: 0.96

        // Subtle inner glow at top
        Rectangle {
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            anchors.topMargin: 1; anchors.leftMargin: 1; anchors.rightMargin: 1
            height: 1; radius: parent.radius
            color: Qt.rgba(1, 1, 1, 0.04)
        }

        ColumnLayout {
            id: menuCol
            anchors.fill: parent
            anchors.margins: popup._pad
            spacing: 2

            // Header: App name + version
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 36

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        textFormat: Text.PlainText
                        text: "Makine Launcher"
                        font.pixelSize: 13; font.weight: Font.DemiBold
                        color: "#EAEAED"
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        textFormat: Text.PlainText
                        text: "v" + popup.appVersion
                        font.pixelSize: 11
                        color: "#6B6B76"
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                Layout.leftMargin: 8; Layout.rightMargin: 8
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            // Menu items
            TrayMenuItem {
                label: qsTr("Makine Launcher'ı Aç")
                onClicked: { popup.visible = false; popup.showRequested() }
            }

            TrayMenuItem {
                label: qsTr("Güncelleme Kontrolü")
                onClicked: { popup.visible = false; popup.checkUpdatesRequested() }
            }

            TrayMenuItem {
                label: qsTr("Ayarlar")
                onClicked: { popup.visible = false; popup.settingsRequested() }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                Layout.leftMargin: 8; Layout.rightMargin: 8
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            TrayMenuItem {
                label: qsTr("Tamamen Kapat")
                isDestructive: true
                onClicked: { popup.visible = false; popup.quitRequested() }
            }
        }
    }

    // Inline menu item component
    component TrayMenuItem: Item {
        id: menuItem
        property string label: ""
        property bool isDestructive: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: popup._itemH

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 2; anchors.rightMargin: 2
            radius: 6
            color: itemMouse.containsMouse
                ? (menuItem.isDestructive ? Qt.rgba(0.8, 0.2, 0.2, 0.12) : Qt.rgba(1, 1, 1, 0.06))
                : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }

            Text {
                textFormat: Text.PlainText
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                verticalAlignment: Text.AlignVCenter
                text: menuItem.label
                font.pixelSize: 13; font.weight: Font.Medium
                color: menuItem.isDestructive
                    ? (itemMouse.containsMouse ? "#E04848" : "#B83030")
                    : (itemMouse.containsMouse ? "#EAEAED" : "#C4C4CC")
            }

            MouseArea {
                id: itemMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: menuItem.clicked()
            }
        }
    }
}
