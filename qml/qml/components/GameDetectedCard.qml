import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * GameDetectedCard.qml - Native Qt GameDetectedCard birebir port
 * Kaynak: ui/src/widgets/gamedetectedcard.cpp
 *
 * Features:
 * - Game header image
 * - Game name with verified badge
 * - Translation phase badge
 * - Progress indicator
 * - Anti-cheat warning
 * - Stop button
 */
Rectangle {
    id: root

    // Game info
    property string gameId: ""
    property string gameName: "Game Name"
    property string gameImageUrl: ""
    property string gameLogoUrl: ""
    property bool isVerified: false

    // Translation state
    property int phase: TranslationPhaseBadge.Phase.Idle
    property real progress: 0.0
    property string progressMessage: ""
    property bool gameRunning: false
    property bool antiCheatDetected: false
    property string antiCheatSummary: ""

    signal stopClicked()

    // Native Qt: maxWidth 500
    implicitWidth: 500
    implicitHeight: contentLayout.height + 48  // padding 24
    radius: Dimensions.radiusXS  // 4

    // Native Qt: surface 95% alpha
    color: Theme.withAlpha(Theme.surface, 0.95)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Header section with game image and info
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // Game image - Native Qt: 100x60, borderRadius 4
            Rectangle {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 60
                radius: Dimensions.radiusStandard
                color: Theme.surfaceActive
                clip: true

                Image {
                    anchors.fill: parent
                    source: root.gameImageUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: root.gameImageUrl !== ""
                }

                // Placeholder
                Text {
                    anchors.centerIn: parent
                    text: gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: Theme.textMuted
                    visible: root.gameImageUrl === ""
                }
            }

            // Game info column
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                // Game name row
                RowLayout {
                    spacing: 8

                    Text {
                        text: root.gameName
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // Verified badge
                    Rectangle {
                        visible: root.isVerified
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(Theme.primary, 0.9)

                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: "white"
                        }
                    }
                }

                // Translation phase badge
                TranslationPhaseBadge {
                    phase: root.phase
                }
            }
        }

        // Progress section (visible when translating)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: progressContent.height + 24
            radius: Dimensions.radiusStandard
            color: Qt.rgba(1, 1, 1, 0.03)
            visible: phase !== TranslationPhaseBadge.Phase.Idle && phase !== TranslationPhaseBadge.Phase.Completed

            ColumnLayout {
                id: progressContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.1)

                    Rectangle {
                        width: parent.width * root.progress
                        height: parent.height
                        radius: 2
                        color: Theme.primary

                        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                    }
                }

                // Progress message
                Text {
                    text: root.progressMessage || Math.round(root.progress * 100) + "%"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
            }
        }

        // Game running indicator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(Theme.success, 0.1)
            border.color: Theme.withAlpha(Theme.success, 0.2)
            border.width: 1
            visible: root.gameRunning

            Row {
                anchors.centerIn: parent
                spacing: 8

                // Pulsing dot
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: root.gameRunning
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 1000 }
                        NumberAnimation { to: 1.0; duration: 1000 }
                    }
                }

                Text {
                    text: "Oyun calisiyor"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Anti-cheat warning
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: antiCheatContent.height + 24
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(Theme.warning, 0.1)
            border.color: Theme.withAlpha(Theme.warning, 0.2)
            border.width: 1
            visible: root.antiCheatDetected

            ColumnLayout {
                id: antiCheatContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    spacing: 8

                    Text {
                        text: "\u26A0"  // Warning
                        font.pixelSize: 16
                        color: Theme.warning
                    }

                    Text {
                        text: "Anti-Cheat Tespit Edildi"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: Theme.warning
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.antiCheatSummary || "Bu oyun anti-cheat sistemi kullanıyor. Ceviri bazi dosyalari etkilemeyebilir."
                    font.pixelSize: 12
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Stop button
        StopButton {
            Layout.fillWidth: true
            onClicked: root.stopClicked()
        }
    }
}
