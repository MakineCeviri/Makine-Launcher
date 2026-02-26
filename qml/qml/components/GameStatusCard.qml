import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * GameStatusCard.qml - Game scanning status overview card
 */
Rectangle {
    id: root

    property bool animationsEnabled: true
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    signal manualFolderRequested()

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 1
    Layout.preferredHeight: layoutTopRowHeight

    radius: Dimensions.radiusSection
    color: Theme.surface
    border.color: Theme.textPrimary06
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.layoutCardMargin + 8
        spacing: root.layoutCardSpacing

        // Title
        Label {
            textFormat: Text.PlainText
            text: qsTr("Oyun Durumu")
            font.pixelSize: Dimensions.fontLG
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        // Stats row
        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMD

            // Installed games count
            Column {
                spacing: 2
                Label {
                    textFormat: Text.PlainText
                    text: GameService.gameCount
                    font.pixelSize: Dimensions.fontHero
                    font.weight: Font.Bold
                    color: Theme.primary
                }
                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Oyun Bulundu")
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                }
            }

            // Patched games count
            Column {
                spacing: 2
                Label {
                    textFormat: Text.PlainText
                    text: GameService.installedTranslationCount
                    font.pixelSize: Dimensions.fontHero
                    font.weight: Font.Bold
                    color: Theme.accentBase
                }
                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Yama Kurulu")
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Scanning indicator
        RowLayout {
            visible: GameService.isScanning
            spacing: Dimensions.spacingSM

            BusyIndicator {
                running: GameService.isScanning
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }

            Label {
                textFormat: Text.PlainText
                text: qsTr("Taranıyor...")
                font.pixelSize: Dimensions.fontXS
                color: Theme.textMuted
            }
        }

        // Manual folder button
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            radius: Dimensions.radiusSM
            color: addFolderMa.containsMouse
                ? Theme.primary12
                : Theme.textPrimary04
            border.color: Theme.textPrimary08
            border.width: 1

            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            Label {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: qsTr("+ Klas\u00F6r Ekle")
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            MouseArea {
                id: addFolderMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.manualFolderRequested()
            }
        }
    }
}
