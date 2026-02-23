import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * InstallNotesDialog.qml - Show pre-install notes before installation
 *
 * Usage:
 *   InstallNotesDialog {
 *       notes: "Oyun dilini İngilizce yapın, sonra yamayı kurun."
 *       onAccepted: installTranslation()
 *   }
 */
BaseDialog {
    id: root

    property string notes: ""

    title: qsTr("Kurulum Notu")
    width: 440
    contentHeight: contentColumn.implicitHeight
    accentColor: Theme.accent

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
                color: Theme.withAlpha(Theme.warning, 0.10)
                border.color: Theme.withAlpha(Theme.warning, 0.20)
                border.width: 1

                Canvas {
                    anchors.centerIn: parent
                    width: 16; height: 16
                    property color c: Theme.warning
                    onCChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = c
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.arc(8, 8, 6.5, 0, Math.PI * 2)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(8, 5, 0.8, 0, Math.PI * 2)
                        ctx.fillStyle = c
                        ctx.fill()
                        ctx.beginPath()
                        ctx.moveTo(8, 7.5)
                        ctx.lineTo(8, 12)
                        ctx.stroke()
                    }
                }
            }

            Label {
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            DialogCloseButton { onClicked: { root.cancelled(); root.close() } }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingSM

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            Layout.preferredHeight: notesLabel.implicitHeight + Dimensions.paddingMD * 2
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(Theme.warning, 0.06)
            border.color: Theme.withAlpha(Theme.warning, 0.15)
            border.width: 1

            Label {
                id: notesLabel
                anchors.fill: parent
                anchors.margins: Dimensions.paddingMD
                text: root.notes
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                lineHeight: 1.5
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 56

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Label {
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
                color: _cancelMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : "transparent"
                border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Cancel")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }

                Label {
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
                Layout.preferredWidth: _continueLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _continueMouse.containsMouse ? Theme.accent : Theme.withAlpha(Theme.accent, 0.85)
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                scale: _continueMouse.pressed ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Continue")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.accepted(); root.close() }

                Label {
                    id: _continueLbl
                    anchors.centerIn: parent
                    text: qsTr("Devam Et")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }

                MouseArea {
                    id: _continueMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.accepted(); root.close() }
                }
            }
        }
    }
}
