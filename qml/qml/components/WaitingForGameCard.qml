import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * WaitingForGameCard.qml - Native Qt WaitingForGameCard birebir port
 * Kaynak: ui/src/widgets/waitingforgamecard.cpp
 *
 * Features:
 * - 400px width, 32px padding
 * - Search icon in 64px circle
 * - "Oyun Aktif Degil" title (18px, weight 600)
 * - Hint box with platform info
 * - Full-width stop button
 */
Rectangle {
    id: root

    signal stopClicked()

    // Native Qt: fixedWidth 400
    implicitWidth: 400
    implicitHeight: contentLayout.height + 64  // padding 32 top/bottom
    radius: Dimensions.radiusXS  // 4

    // Native Qt: Container decoration
    // color: isDark ? 0xFF151515 : 0xFFF5F5F5
    color: "#151515"

    // Border - Native Qt: white 6%
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1

    ColumnLayout {
        id: contentLayout
        anchors.centerIn: parent
        width: parent.width - 64  // padding 32
        spacing: 0

        // Search icon circle - Native Qt: 64x64, borderRadius 32
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 32
            color: Qt.rgba(1, 1, 1, 0.05)

            Text {
                anchors.centerIn: parent
                text: "\uD83D\uDD0D"  // Magnifying glass
                font.pixelSize: 32
                color: Theme.textMuted
            }
        }

        Item { Layout.preferredHeight: 20 }

        // Title - Native Qt: "Oyun Aktif Degil" (18px, weight 600)
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Oyun Aktif Degil"
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Item { Layout.preferredHeight: 12 }

        // Subtitle - Native Qt: line break included
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: "Desteklenen bir oyunu baslattignizda\notomatik olarak tespit edilecektir."
            font.pixelSize: 13
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.5
        }

        Item { Layout.preferredHeight: 24 }

        // Hint box - Native Qt: white 3% bg, borderRadius 4
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 4
            color: Qt.rgba(1, 1, 1, 0.03)

            Row {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "\u2139"  // Info icon
                    font.pixelSize: 14
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Steam, Epic Games, GOG desteklenir"
                    font.pixelSize: 12
                    color: Theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        // Stop button - Native Qt: full width
        StopButton {
            Layout.fillWidth: true
            onClicked: root.stopClicked()
        }
    }
}
