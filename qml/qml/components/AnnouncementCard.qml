import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * AnnouncementCard.qml - Flutter announcement_card.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/announcement_card.dart
 *
 * Features:
 * - Glassmorphism background with BackdropFilter blur (10, 10)
 * - Important badge with gradient (orange-gold)
 * - Icon + title header
 * - Content with date
 * - Warning box with red styling
 * - Hover glow effect
 */
Rectangle {
    id: root

    property string title: "Desteklenmeyen Oyunlar Hakkinda"
    property string content: "MakineAI, resmi olarak desteklenen oyunlar disinda kullanildiginda beklendigi gibi calismayabilir. Desteklenmeyen oyunlarda ceviri hatalari veya performans sorunlari yasanabilir."
    property string date: "18 Ocak 2026"
    property string warning: "Uygulamayi yalnizca resmi web sitemiz makineai.com uzerinden indirdiginizden emin olun. Baska kaynaklardan indirilen surumler guvenlik riski tasiyabilir."
    property bool isImportant: true
    property bool isDark: true
    property bool hoverEnabled: true

    // Hover state
    readonly property bool isHovered: hoverArea.containsMouse

    // Flutter: BackdropFilter blur (10, 10), ClipRRect borderRadius 4
    implicitHeight: contentLayout.height + 48
    radius: Dimensions.radiusStandard
    color: isDark
        ? Qt.rgba(1, 1, 1, isHovered ? 0.05 : 0.03)
        : Qt.rgba(0, 0, 0, isHovered ? 0.05 : 0.03)
    border.color: isDark
        ? Qt.rgba(1, 1, 1, isHovered ? 0.12 : 0.08)
        : Qt.rgba(0, 0, 0, isHovered ? 0.12 : 0.08)
    border.width: 1

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    // Hover scale
    scale: isHovered ? 1.01 : 1.0
    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    // Note: Flutter AnnouncementCard doesn't have glow effects
    // Just BackdropFilter blur which we simulate with glass background

    // Mouse area for hover
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        acceptedButtons: Qt.NoButton
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Header row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // Icon container - Native Qt: info color 15% alpha, padding 10, borderRadius 4
            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.info, 0.15)

                Text {
                    anchors.centerIn: parent
                    text: "\uD83D\uDCE2"  // Megaphone
                    font.pixelSize: 18
                    color: Theme.info
                }
            }

            // Title column
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                // Badge row - Native Qt: "ONEMLI" gradient orange-gold
                RowLayout {
                    spacing: 8

                    Rectangle {
                        visible: root.isImportant
                        Layout.preferredWidth: importantLabel.width + 16
                        Layout.preferredHeight: 20
                        radius: Dimensions.radiusStandard

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.splashOrange }
                            GradientStop { position: 1.0; color: Theme.splashGold }
                        }

                        Text {
                            id: importantLabel
                            anchors.centerIn: parent
                            text: "ÖNEMLİ"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            color: "white"
                        }
                    }

                    Text {
                        text: "Duyuru"
                        font.pixelSize: 12
                        color: isDark ? Theme.textMuted : Theme.lightTextMuted
                    }
                }

                // Title - Flutter: fontSize 16, fontWeight w600
                Text {
                    text: root.title
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: isDark ? Theme.textPrimary : Theme.lightTextPrimary
                }
            }
        }

        // Content text - Flutter: fontSize 14, height 1.6
        Text {
            Layout.fillWidth: true
            text: root.content
            font.pixelSize: 14
            color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.6
        }

        // Date row - Flutter: Icons.schedule_rounded
        RowLayout {
            spacing: 6

            Text {
                text: "\u23F0"  // Clock
                font.pixelSize: 14
                color: isDark ? Theme.textMuted : Theme.lightTextMuted
            }

            Text {
                text: root.date
                font.pixelSize: 12
                color: isDark ? Theme.textMuted : Theme.lightTextMuted
            }
        }

        // Warning box - Flutter: #E53935 10% bg, 20% border
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warningContent.height + 24
            radius: Dimensions.radiusStandard
            color: Qt.rgba(0.898, 0.224, 0.208, 0.1)  // #E53935 10%
            border.color: Qt.rgba(0.898, 0.224, 0.208, 0.2)  // #E53935 20%
            border.width: 1
            visible: root.warning !== ""

            RowLayout {
                id: warningContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "\uD83D\uDEE1"  // Shield / verified_user_rounded
                    font.pixelSize: 18
                    color: "#E53935"
                }

                Text {
                    Layout.fillWidth: true
                    text: root.warning
                    font.pixelSize: 12
                    color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
                    wrapMode: Text.WordWrap
                    lineHeight: 1.5
                }
            }
        }
    }
}
