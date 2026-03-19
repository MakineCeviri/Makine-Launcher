import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameDetailOverlay — Loading spinner and Steam fetch error/retry overlays.
 *
 * Positioned over the game detail screen. Only one state visible at a time:
 * loading (spinner) or error (message + retry button).
 */
Item {
    id: overlay

    required property var vm

    // Loading overlay (Steam details)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: loadingRow.width + 40; height: 44
        radius: Dimensions.radiusFull
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: overlay.vm.isLoadingSteamDetails && !overlay.vm.hasSteamDetails

        RowLayout {
            id: loadingRow; anchors.centerIn: parent; spacing: Dimensions.spacingLG
            BusyIndicator { width: 20; height: 20; running: visible }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Steam bilgileri yükleniyor...")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
            }
        }
    }

    // Error + Retry
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: errorCol.width + 40; height: errorCol.height + 24
        radius: Dimensions.radiusStandard
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: overlay.vm.steamFetchFailed && !overlay.vm.hasSteamDetails

        ColumnLayout {
            id: errorCol; anchors.centerIn: parent; spacing: Dimensions.spacingMD
            Text {
                textFormat: Text.PlainText
                text: qsTr("Steam bilgileri alınamadı")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: retryLbl.width + 24; implicitHeight: 30
                radius: Dimensions.radiusStandard
                color: retryMouse.containsMouse ? Theme.primaryHover : Theme.primary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

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
