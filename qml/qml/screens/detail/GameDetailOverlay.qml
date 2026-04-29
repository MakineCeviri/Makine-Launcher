import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameDetailOverlay — Premium loading and error overlays for game detail.
 */
Item {
    id: overlay

    required property var vm

    // Loading overlay
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: loadingRow.width + 48; height: 48
        radius: Dimensions.radiusFull
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: overlay.vm.isLoadingSteamDetails && !overlay.vm.hasSteamDetails
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        layer.enabled: false

        RowLayout {
            id: loadingRow; anchors.centerIn: parent; spacing: Dimensions.spacingLG

            // Custom spinner — 3 pulsating dots
            Row {
                spacing: 6
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 8; height: 8; radius: 4
                        color: Theme.primary
                        opacity: 0.3

                        SequentialAnimation on opacity {
                            running: overlay.vm.isLoadingSteamDetails && !overlay.vm.hasSteamDetails &&
                                     SettingsManager.enableAnimations &&
                                     Qt.application.state === Qt.ApplicationActive
                            loops: Animation.Infinite
                            PauseAnimation { duration: index * 150 }
                            NumberAnimation { to: 1.0; duration: 400; easing.type: Easing.OutQuad }
                            NumberAnimation { to: 0.3; duration: 400; easing.type: Easing.InQuad }
                            PauseAnimation { duration: (2 - index) * 150 }
                        }
                    }
                }
            }

            Text {
                textFormat: Text.PlainText
                text: qsTr("Oyun bilgileri yükleniyor...")
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.Medium
                color: Theme.textSecondary
            }
        }
    }

    // Error + Retry
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: errorCol.width + 48; height: errorCol.height + 28
        radius: Dimensions.radiusStandard
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: overlay.vm.steamFetchFailed && !overlay.vm.hasSteamDetails
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        ColumnLayout {
            id: errorCol; anchors.centerIn: parent; spacing: Dimensions.spacingMD
            Text {
                textFormat: Text.PlainText
                text: qsTr("Oyun bilgileri alınamadı")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: retryLbl.width + 28; implicitHeight: 32
                radius: Dimensions.radiusFull
                color: retryMouse.containsMouse ? Theme.primaryHover : Theme.primary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                scale: retryMouse.pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Tekrar Dene")

                Text {
                    textFormat: Text.PlainText
                    id: retryLbl
                    anchors.centerIn: parent
                    text: qsTr("Tekrar Dene")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }
                MouseArea {
                    id: retryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        overlay.vm.steamFetchFailed = false
                        overlay.vm.isLoadingSteamDetails = true
                        GameService.fetchSteamDetails(overlay.vm.steamAppId)
                    }
                }
            }
        }
    }
}
