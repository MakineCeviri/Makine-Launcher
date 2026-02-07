import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * AnnouncementCard.qml - Announcement card with glassmorphism and hover effects
 */
Rectangle {
    id: root

    property string title: qsTr("Desteklenmeyen Oyunlar Hakkında")
    property string content: qsTr("MakineAI, resmi olarak desteklenen oyunlar dışında kullanıldığında beklendiği gibi çalışmayabilir. Desteklenmeyen oyunlarda çeviri hataları veya performans sorunları yaşanabilir.")
    property string date: "18 Ocak 2026"
    property string warning: qsTr("Uygulamayı yalnızca resmi web sitemiz makineai.com üzerinden indirdiğinizden emin olun. Başka kaynaklardan indirilen sürümler güvenlik riski taşıyabilir.")
    property bool isImportant: true
    property bool isDark: true
    property bool hoverEnabled: true

    readonly property bool isHovered: hoverArea.containsMouse

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

    scale: isHovered ? 1.01 : 1.0
    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.info, 0.15)

                Text {
                    anchors.centerIn: parent
                    text: "\uD83D\uDCE2"
                    font.pixelSize: 18
                    color: Theme.info
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

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
                            text: qsTr("ÖNEMLİ")
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

                Text {
                    text: root.title
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: isDark ? Theme.textPrimary : Theme.lightTextPrimary
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.content
            font.pixelSize: 14
            color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.6
        }

        RowLayout {
            spacing: 6

            Text {
                text: "\u23F0"
                font.pixelSize: 14
                color: isDark ? Theme.textMuted : Theme.lightTextMuted
            }

            Text {
                text: root.date
                font.pixelSize: 12
                color: isDark ? Theme.textMuted : Theme.lightTextMuted
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warningContent.height + 24
            radius: Dimensions.radiusStandard
            color: Qt.rgba(0.898, 0.224, 0.208, 0.1)
            border.color: Qt.rgba(0.898, 0.224, 0.208, 0.2)
            border.width: 1
            visible: root.warning !== ""

            RowLayout {
                id: warningContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "\uD83D\uDEE1"
                    font.pixelSize: 18
                    color: Theme.destructive
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
