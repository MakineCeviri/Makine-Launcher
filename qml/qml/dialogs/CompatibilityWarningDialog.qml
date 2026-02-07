import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * CompatibilityWarningDialog.qml - Warns user when game files changed
 * since last translation, translation may be incompatible.
 */
Dialog {
    id: root

    property string gameName: ""
    property string compatibilityLevel: "unknown"
    property int integrityPercent: 100
    property int modifiedCount: 0

    signal proceedAnyway()
    signal restoreBackup()

    title: qsTr("Uyumluluk Uyarısı")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 440
    height: contentCol.height + 140

    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Theme.withAlpha(Theme.warning, 0.3)
        border.width: 1
    }

    contentItem: ColumnLayout {
        id: contentCol
        spacing: 16

        // Warning icon
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: 28
            color: Theme.withAlpha(Theme.warning, 0.12)

            Text {
                anchors.centerIn: parent
                text: "\u26A0"
                font.pixelSize: 28
                color: Theme.warning
            }
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Oyun Güncellendi")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        // Description
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            text: {
                var msg = qsTr("%1 oyunu, çeviri kurulduktan sonra güncellenmiş.").arg(gameName)
                if (modifiedCount > 0)
                    msg += " " + qsTr("%1 dosya değiştirilmiş.").arg(modifiedCount)
                if (compatibilityLevel === "incompatible")
                    msg += "\n\n" + qsTr("Çeviri büyük olasılıkla bozulmuş olabilir. Yedekleme geri yüklemesi önerilir.")
                else
                    msg += "\n\n" + qsTr("Çeviri kısmen uyumsuz olabilir. Devam edebilir veya yedeği geri yükleyebilirsiniz.")
                return msg
            }
            font.pixelSize: 13
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.4
        }

        // Integrity badge
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Text {
                text: qsTr("Bütünlük:")
                font.pixelSize: 12
                color: Theme.textMuted
            }

            Text {
                text: integrityPercent + "%"
                font.pixelSize: 14
                font.weight: Font.Bold
                color: {
                    if (integrityPercent >= 95) return Theme.success
                    if (integrityPercent >= 70) return Theme.warning
                    return Theme.destructive
                }
            }
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 12

            // Restore backup button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: restoreMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.12)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Yedeği Geri Yükle")
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }

                MouseArea {
                    id: restoreMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.restoreBackup()
                        root.close()
                    }
                }
            }

            // Proceed anyway button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: proceedMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.2) : Theme.withAlpha(Theme.warning, 0.12)

                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Yine de Devam Et")
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: Theme.warning
                }

                MouseArea {
                    id: proceedMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.proceedAnyway()
                        root.close()
                    }
                }
            }
        }
    }
}
