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
        spacing: Dimensions.spacingXL

        // Warning icon
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: Dimensions.radiusFull
            color: Theme.withAlpha(Theme.warning, 0.12)

            Text {
                anchors.centerIn: parent
                text: "\u26A0"
                font.pixelSize: Dimensions.fontHero
                color: Theme.warning
            }
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Oyun Güncellendi")
            font.pixelSize: Dimensions.fontTitle
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        // Description
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.marginMD
            Layout.rightMargin: Dimensions.marginMD
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
            font.pixelSize: Dimensions.fontBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.4
        }

        // Integrity badge
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Dimensions.spacingMD

            Text {
                text: qsTr("Bütünlük:")
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
            }

            Text {
                text: integrityPercent + "%"
                font.pixelSize: Dimensions.fontMD
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
            Layout.topMargin: Dimensions.marginSM
            spacing: Dimensions.spacingLG

            // Restore backup button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: restoreMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : Theme.withAlpha(Theme.textPrimary, 0.04)
                border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                border.width: 1
                scale: restoreMouse.pressed ? 0.97 : 1.0
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Yedeği Geri Yükle")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.restoreBackup(); root.close() }
                Keys.onSpacePressed: { root.restoreBackup(); root.close() }

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Yedeği Geri Yükle")
                    font.pixelSize: Dimensions.fontBody
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                    border.width: 2
                    visible: parent.activeFocus
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
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Yine de Devam Et")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.proceedAnyway(); root.close() }
                Keys.onSpacePressed: { root.proceedAnyway(); root.close() }
                color: proceedMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.2) : Theme.withAlpha(Theme.warning, 0.12)
                scale: proceedMouse.pressed ? 0.97 : 1.0

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Yine de Devam Et")
                    font.pixelSize: Dimensions.fontBody
                    font.weight: Font.DemiBold
                    color: Theme.warning
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.warning, 0.6)
                    border.width: 2
                    visible: parent.activeFocus
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
