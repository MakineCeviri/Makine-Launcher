import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * GameToast - Overlay notification for detected running games
 *
 * Shows at the top of the window when a supported game is detected
 * that is not yet in the user's library. Auto-dismisses after 8 seconds.
 */
Item {
    id: root

    property string gameName: ""
    property string gameId: ""
    property string customTitle: ""
    property string customSubtitle: ""
    property bool shown: false

    anchors.top: parent.top
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.topMargin: shown ? 24 : -100
    width: Math.min(520, parent.width - 48)
    height: toastCard.height
    z: Dimensions.zToast
    visible: true

    Behavior on anchors.topMargin {
        NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic }
    }

    // Auto-dismiss after 8 seconds
    Timer {
        id: autoHide
        interval: 8000
        onTriggered: root.shown = false
    }

    function show(gId, gName, title, subtitle) {
        root.gameId = gId
        root.gameName = gName
        root.customTitle = title || ""
        root.customSubtitle = subtitle || ""
        root.shown = true
        autoHide.restart()
    }

    Rectangle {
        id: toastCard
        width: parent.width
        height: contentLayout.implicitHeight + 28
        radius: Dimensions.radiusSection
        color: Qt.rgba(0.06, 0.06, 0.06, 0.95)
        border.color: Theme.accent
        border.width: 1

        GradientBorder {
            cornerRadius: parent.radius
            topColor: Theme.accentBase30
        }

        RowLayout {
            id: contentLayout
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16; anchors.rightMargin: 12
            spacing: 12

            // Gamepad icon
            Canvas {
                Layout.preferredWidth: 24; Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignVCenter
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = Theme.accent; ctx.fillStyle = Theme.accent
                    ctx.lineWidth = 1.6; ctx.lineCap = "round"; ctx.lineJoin = "round"
                    // Controller body
                    ctx.beginPath()
                    ctx.moveTo(6, 1.5); ctx.lineTo(18, 1.5)
                    ctx.quadraticCurveTo(22.5, 1.5, 22.5, 6)
                    ctx.lineTo(22.5, 9)
                    ctx.quadraticCurveTo(22.5, 16.5, 18, 16.5)
                    ctx.lineTo(6, 16.5)
                    ctx.quadraticCurveTo(1.5, 16.5, 1.5, 9)
                    ctx.lineTo(1.5, 6)
                    ctx.quadraticCurveTo(1.5, 1.5, 6, 1.5)
                    ctx.closePath(); ctx.stroke()
                    // D-pad
                    ctx.lineWidth = 1.4
                    ctx.beginPath(); ctx.moveTo(6, 9); ctx.lineTo(10, 9); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(8, 6.5); ctx.lineTo(8, 11.5); ctx.stroke()
                    // Buttons
                    ctx.beginPath(); ctx.arc(16, 7.5, 1.2, 0, Math.PI * 2); ctx.fill()
                    ctx.beginPath(); ctx.arc(19, 10.5, 1.2, 0, Math.PI * 2); ctx.fill()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    textFormat: Text.PlainText
                    text: root.customTitle || (root.gameName + qsTr(" tespit edildi!"))
                    font.pixelSize: Dimensions.fontSM; font.weight: Font.Bold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    textFormat: Text.PlainText
                    text: root.customSubtitle || qsTr("Türkçe yama yüklemek için oyunu kapatın ve kütüphaneyi kontrol edin.")
                    font.pixelSize: Dimensions.fontXS; color: Theme.textSecondary
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                    lineHeight: 1.3
                }
            }

            // Close button
            Rectangle {
                Layout.preferredWidth: 28; Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignVCenter
                radius: 14
                color: closeMa.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                Behavior on color { ColorAnimation { duration: 150 } }

                Label {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: "✕"
                    color: Theme.textMuted
                    font.pixelSize: Dimensions.fontSM
                }

                MouseArea {
                    id: closeMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.shown = false
                }
            }
        }
    }
}
