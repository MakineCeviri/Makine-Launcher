import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ConfirmDialog.qml - Reusable confirmation dialog for destructive actions
 *
 * Usage:
 *   ConfirmDialog {
 *       title: "Delete Backups"
 *       message: "All backups will be deleted."
 *       confirmText: "Delete"
 *       onConfirmed: doDestructiveAction()
 *   }
 */
BaseDialog {
    id: root

    property string message: ""
    property string confirmText: qsTr("Onayla")
    accentColor: Theme.error

    signal confirmed()

    width: 400
    contentHeight: contentColumn.implicitHeight

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.withAlpha(root.accentColor, 0.10)
                border.color: Theme.withAlpha(root.accentColor, 0.20)
                border.width: 1

                Canvas {
                    anchors.centerIn: parent
                    width: 16; height: 16
                    property color c: root.accentColor
                    onCChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = c
                        ctx.lineWidth = 1.6
                        ctx.lineJoin = "round"
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.moveTo(8, 2); ctx.lineTo(15, 14); ctx.lineTo(1, 14)
                        ctx.closePath(); ctx.stroke()
                        ctx.fillStyle = c
                        ctx.beginPath(); ctx.arc(8, 11.5, 1, 0, Math.PI * 2); ctx.fill()
                        ctx.beginPath(); ctx.moveTo(8, 5.5); ctx.lineTo(8, 9.5); ctx.stroke()
                    }
                }
            }

            Label {
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            DialogCloseButton {
                onClicked: { root.cancelled(); root.close() }
            }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.textPrimary06
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingMD

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            text: root.message
            font.pixelSize: Dimensions.fontSM
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.5
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 56

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.textPrimary06
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Label {
                textFormat: Text.PlainText
                text: qsTr("Esc")
                font.pixelSize: Dimensions.fontMicro
                color: Theme.textMuted
                opacity: 0.5
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: _cancelLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _cancelMouse.containsMouse ? Theme.textPrimary08 : "transparent"
                border.color: Theme.textPrimary12
                border.width: 1
                scale: _cancelMouse.pressed ? Dimensions.pressScale : 1.0
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("İptal")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }

                Label {
                    textFormat: Text.PlainText
                    id: _cancelLbl
                    anchors.centerIn: parent
                    text: qsTr("Vazgeç")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: _cancelMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }

                MouseArea {
                    id: _cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.cancelled(); root.close() }
                }
            }

            Rectangle {
                Layout.preferredWidth: _confirmLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _confirmMouse.containsMouse ? root.accentColor : Theme.withAlpha(root.accentColor, 0.85)
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                scale: _confirmMouse.pressed ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: root.confirmText
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.confirmed(); root.close() }

                Label {
                    textFormat: Text.PlainText
                    id: _confirmLbl
                    anchors.centerIn: parent
                    text: root.confirmText
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }

                MouseArea {
                    id: _confirmMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.confirmed(); root.close() }
                }
            }
        }
    }
}
