import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * WaitingForGameCard.qml - Idle state card when no game is detected
 */
Rectangle {
    id: root

    signal stopClicked()

    implicitWidth: 400
    implicitHeight: contentLayout.height + 64
    radius: Dimensions.radiusXS
    color: "#151515"
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1

    ColumnLayout {
        id: contentLayout
        anchors.centerIn: parent
        width: parent.width - 64
        spacing: 0

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 32
            color: Qt.rgba(1, 1, 1, 0.05)

            Text {
                anchors.centerIn: parent
                text: "\uD83D\uDD0D"
                font.pixelSize: 32
                color: Theme.textMuted
            }
        }

        Item { Layout.preferredHeight: 20 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Oyun Aktif Değil"
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Item { Layout.preferredHeight: 12 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: "Desteklenen bir oyunu başlattığınızda\notomatik olarak tespit edilecektir."
            font.pixelSize: 13
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.5
        }

        Item { Layout.preferredHeight: 24 }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Dimensions.radiusStandard
            color: Qt.rgba(1, 1, 1, 0.03)

            Row {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "\u2139"
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

        StopButton {
            Layout.fillWidth: true
            onClicked: root.stopClicked()
        }
    }
}
