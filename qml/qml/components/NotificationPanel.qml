import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * NotificationPanel.qml - Dropdown notification panel
 *
 * Shows notification list with categories, read/unread state,
 * and actions. Anchors below the bell icon in NavBar.
 */
Popup {
    id: root

    property alias model: notificationList.model

    signal notificationClicked(int index)
    signal markAllRead()
    signal clearAll()

    width: 360
    height: Math.min(panelContentHeight, 480)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property real panelContentHeight: headerItem.height + listContainer.implicitHeight + footerItem.height + 2

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.animFast }
        NumberAnimation { property: "y"; from: root.y - 8; to: root.y; duration: Dimensions.animFast; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animVeryFast }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusLG
        border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
        border.width: 1

        // Drop shadow
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.withAlpha(Theme.bgPrimary, 0.4)
            shadowBlur: 1.0
            shadowVerticalOffset: 8
            shadowHorizontalOffset: 0
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        Item {
            id: headerItem
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginMD
                anchors.rightMargin: Dimensions.marginMS

                Text {
                    text: qsTr("Bildirimler")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                // Mark all read button
                Rectangle {
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: markAllText.implicitWidth + 16
                    radius: Dimensions.radiusXS
                    color: markAllMouse.containsMouse
                        ? Theme.withAlpha(Theme.primary, 0.1) : "transparent"
                    visible: root.model && root.model.count > 0
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Tümünü oku")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: root.markAllRead()
                    Keys.onSpacePressed: root.markAllRead()

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    Text {
                        id: markAllText
                        anchors.centerIn: parent
                        text: qsTr("Tümünü oku")
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.Medium
                        color: Theme.primary
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: markAllMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.markAllRead()
                    }
                }
            }

            // Bottom border
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }
        }

        // Notification list
        Item {
            id: listContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 80
            implicitHeight: notificationList.count > 0
                ? Math.min(notificationList.contentHeight, 360) : 120

            ListView {
                id: notificationList
                anchors.fill: parent
                clip: true
                cacheBuffer: 200
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: notificationList.contentHeight > notificationList.height
                        ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                delegate: Rectangle {
                    id: delegateRoot
                    width: notificationList.width
                    height: delegateLayout.implicitHeight + 20
                    color: delegateMouse.containsMouse
                        ? Theme.withAlpha(Theme.textPrimary, 0.03) : "transparent"
                    Accessible.role: Accessible.ListItem
                    Accessible.name: delegateRoot.title

                    required property int index
                    required property string title
                    required property string message
                    required property string time
                    required property string type
                    required property bool read

                    Behavior on color { ColorAnimation { duration: Dimensions.animVeryFast } }

                    RowLayout {
                        id: delegateLayout
                        anchors.fill: parent
                        anchors.leftMargin: Dimensions.marginMD
                        anchors.rightMargin: Dimensions.marginMD
                        anchors.topMargin: Dimensions.marginBase
                        anchors.bottomMargin: Dimensions.marginBase
                        spacing: Dimensions.spacingLG

                        // Type indicator
                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignTop
                            radius: 8
                            color: {
                                switch(delegateRoot.type) {
                                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.15)
                                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.15)
                                    case "error": return Theme.withAlpha(Theme.notificationError, 0.15)
                                    case "success": return Theme.withAlpha(Theme.success, 0.15)
                                    default: return Theme.withAlpha(Theme.primary, 0.15)
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                font.pixelSize: Dimensions.fontMD
                                text: {
                                    switch(delegateRoot.type) {
                                        case "update": return "\u2B06" // up arrow
                                        case "warning": return "\u26A0" // warning
                                        case "error": return "\u2716" // cross
                                        case "success": return "\u2714" // check
                                        case "translation": return "\uD83C\uDF0D" // globe
                                        default: return "\u2139" // info
                                    }
                                }
                            }
                        }

                        // Content
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXXS

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingMD

                                Text {
                                    Layout.fillWidth: true
                                    text: delegateRoot.title
                                    font.pixelSize: Dimensions.fontSM
                                    font.weight: delegateRoot.read ? Font.Normal : Font.DemiBold
                                    color: delegateRoot.read ? Theme.textSecondary : Theme.textPrimary
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                // Unread dot
                                Rectangle {
                                    visible: !delegateRoot.read
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 3
                                    color: Theme.primary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: delegateRoot.message
                                font.pixelSize: Dimensions.fontXS
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            Text {
                                text: delegateRoot.time
                                font.pixelSize: Dimensions.fontCaption
                                color: Theme.textMuted
                                opacity: 0.7
                            }
                        }
                    }

                    MouseArea {
                        id: delegateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.notificationClicked(delegateRoot.index)
                    }

                    // Bottom separator
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Dimensions.marginMD
                        anchors.rightMargin: Dimensions.marginMD
                        height: 1
                        color: Theme.withAlpha(Theme.textPrimary, 0.04)
                        visible: delegateRoot.index < notificationList.count - 1
                    }
                }

                // Empty state
                Item {
                    anchors.fill: parent
                    visible: notificationList.count === 0

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingMD

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "\uD83D\uDD14" // bell emoji
                            font.pixelSize: Dimensions.fontHero
                            opacity: 0.5
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Bildirim yok")
                            font.pixelSize: Dimensions.fontBody
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Yeni çeviriler ve güncellemeler burada görünecek")
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                        }
                    }
                }
            }
        }

        // Footer
        Item {
            id: footerItem
            Layout.fillWidth: true
            Layout.preferredHeight: notificationList.count > 0 ? 40 : 0
            visible: notificationList.count > 0
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Tümünü temizle")
            activeFocusOnTab: true
            Keys.onReturnPressed: root.clearAll()
            Keys.onSpacePressed: root.clearAll()

            // Top border
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }

            Text {
                anchors.centerIn: parent
                text: qsTr("Tümünü temizle")
                font.pixelSize: Dimensions.fontXS
                font.weight: Font.Medium
                color: clearMouse.containsMouse ? Theme.error : Theme.textMuted
                opacity: clearMouse.containsMouse ? 1.0 : 0.7

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
            }

            // Focus indicator
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: Dimensions.radiusXS + 1
                color: "transparent"
                border.color: Theme.withAlpha(Theme.destructive, 0.6)
                border.width: 2
                visible: footerItem.activeFocus
            }

            MouseArea {
                id: clearMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.clearAll()
            }
        }
    }
}
