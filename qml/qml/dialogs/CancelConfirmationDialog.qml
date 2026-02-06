import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

/**
 * CancelConfirmationDialog.qml - Cancel confirmation dialog (Native Qt)
 */
Dialog {
    id: root

    property string gameId: ""

    signal accepted()

    title: "Çeviriyi İptal Et?"

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    width: 400
    padding: 0

    background: Rectangle {
        color: Theme.bgSecondary
        radius: Dimensions.radiusStandard
        border.color: Theme.withAlpha(Theme.textMuted, 0.2)
        border.width: 1

        // Drop shadow simulation
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            radius: 13
            color: Qt.rgba(0, 0, 0, 0.3)
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.textMuted, 0.1)
        }

        // Message
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: messageText.implicitHeight + 48

            Text {
                id: messageText
                anchors.fill: parent
                anchors.margins: 24
                text: "Devam eden çeviri işlemi iptal edilecek.\nEmin misiniz?"
                font.pixelSize: 14
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.4
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.textMuted, 0.1)
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            Layout.margins: 16
            spacing: 12

            Item { Layout.fillWidth: true }

            GradientButton {
                text: "Vazgeç"
                isPrimary: false
                onClicked: root.close()
            }

            GradientButton {
                text: "İptal Et"
                isPrimary: true
                onClicked: {
                    TranslationService.stopTranslation()
                    root.close()
                    root.accepted()
                }
            }
        }
    }
}
