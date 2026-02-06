import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root

    signal manualSelectClicked()

    implicitHeight: gameStatusCardLayout.implicitHeight + 40

    color: Qt.rgba(1, 1, 1, 0.03)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    radius: Dimensions.radiusXS

    ColumnLayout {
        id: gameStatusCardLayout
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        // Header Row
        RowLayout {
            Layout.fillWidth: true
            height: 44
            spacing: 14

            Image {
                source: "qrc:/qt/qml/MakineAI/resources/icons/flag-tr.svg"
                width: 44
                height: 44
                sourceSize: Qt.size(44, 44)
            }

            Column {
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Label {
                    text: "Türkçe Yama"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.3
                    color: Theme.textPrimary
                }

                Label {
                    text: "Oyun çeviri durumu"
                    font.pixelSize: 11
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            // Badge
            Rectangle {
                implicitWidth: badgeRow.implicitWidth + Dimensions.paddingMD
                implicitHeight: Dimensions.marginLG
                radius: Dimensions.radiusLG
                color: Theme.withAlpha(Theme.error, 0.12)

                Row {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Bekleniyor"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // Content Box
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: contentBoxLayout.implicitHeight + Dimensions.marginLG
            radius: Dimensions.radiusSM
            color: Qt.rgba(1, 1, 1, 0.02)
            border.color: Qt.rgba(1, 1, 1, 0.06)
            border.width: 1

            ColumnLayout {
                id: contentBoxLayout
                anchors.centerIn: parent
                spacing: 10

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: Dimensions.iconXL + Dimensions.marginSM
                    height: Dimensions.iconXL + Dimensions.marginSM
                    radius: (Dimensions.iconXL + Dimensions.marginSM) / 2
                    color: Theme.withAlpha(Theme.error, 0.1)

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/MakineAI/resources/icons/hourglass.svg"
                        width: Dimensions.iconMD
                        height: Dimensions.iconMD
                        sourceSize: Qt.size(Dimensions.iconMD, Dimensions.iconMD)
                        antialiasing: true
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Oyun Bekleniyor"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Desteklenen bir oyun çalıştırın veya manuel seçin"
                    font.pixelSize: 11
                    color: Theme.textMuted
                }
            }
        }

        // Manual Select Button
        Button {
            id: manualBtn
            Layout.fillWidth: true

            contentItem: RowLayout {
                spacing: Dimensions.marginSM

                Image {
                    source: "qrc:/qt/qml/MakineAI/resources/icons/plus.svg"
                    width: 16
                    height: 16
                    sourceSize: Qt.size(16, 16)
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "Manuel Oyun Seç"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            background: Rectangle {
                implicitHeight: 36
                color: manualBtn.hovered ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                radius: Dimensions.radiusSM
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            }

            onClicked: root.manualSelectClicked()
        }
    }
}
