import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

ColumnLayout {
    id: root

    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 1
    Layout.preferredHeight: layoutTopRowHeight
    spacing: 8

    // ── News card ──
    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true
        radius: 20
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: newsMa.containsMouse
            ? Theme.withAlpha(Theme.primary, 0.25) : Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        clip: true
        Behavior on border.color { ColorAnimation { duration: 250 } }

        // Ambient glow (soft purple, top-left)
        Canvas {
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                // Clip to container's rounded rect
                var cr = 20
                ctx.beginPath()
                ctx.moveTo(cr, 0)
                ctx.lineTo(width - cr, 0)
                ctx.quadraticCurveTo(width, 0, width, cr)
                ctx.lineTo(width, height - cr)
                ctx.quadraticCurveTo(width, height, width - cr, height)
                ctx.lineTo(cr, height)
                ctx.quadraticCurveTo(0, height, 0, height - cr)
                ctx.lineTo(0, cr)
                ctx.quadraticCurveTo(0, 0, cr, 0)
                ctx.closePath()
                ctx.clip()
                // Radial glow at top-left
                var cx = 40
                var cy = 30
                var r = Math.max(width, height) * 0.55
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
                grad.addColorStop(0.0, "rgba(130, 60, 200, 0.12)")
                grad.addColorStop(0.3, "rgba(120, 50, 190, 0.06)")
                grad.addColorStop(0.6, "rgba(110, 40, 180, 0.02)")
                grad.addColorStop(1.0, "rgba(100, 30, 170, 0.0)")
                ctx.fillStyle = grad
                ctx.fillRect(0, 0, width, height)
            }
        }

        MouseArea { id: newsMa; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 14; spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: badgeLbl.width + 14; Layout.preferredHeight: 20
                    radius: 4; color: Theme.withAlpha(Theme.primary, 0.15)
                    border.color: Theme.withAlpha(Theme.primary, 0.20); border.width: 1
                    Label {
                        id: badgeLbl; anchors.centerIn: parent
                        text: qsTr("YEN\u0130"); font.pixelSize: Dimensions.fontMicro
                        font.weight: Font.Bold; color: Theme.primary
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: new Date().toLocaleDateString("tr-TR", { day: '2-digit', month: '2-digit', year: 'numeric' })
                    font.pixelSize: Dimensions.fontMicro; color: Theme.textMuted
                }
            }

            Label {
                text: qsTr("K\u00FCt\u00FCphane D\u0131\u015F\u0131 Oyunlar")
                font.pixelSize: Dimensions.fontSM; font.weight: Font.Bold; color: Theme.textPrimary
            }

            Label {
                Layout.fillWidth: true; Layout.fillHeight: true
                text: qsTr("\u00C7eviri k\u00FCt\u00FCphanesinde hen\u00FCz yer almayan oyunlarda beklenmedik sorunlar i\u00E7in yeni \"G\u00FCvenli Mod\" aktif edildi.")
                font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                wrapMode: Text.WordWrap; lineHeight: 1.4
                verticalAlignment: Text.AlignTop; elide: Text.ElideRight
            }
        }
    }

    // ── Security footer ──
    Rectangle {
        id: secFooter
        Layout.fillWidth: true; Layout.preferredHeight: 34
        radius: 14
        property bool hovered: secMa.containsMouse
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: 200 } }

        Accessible.role: Accessible.Link
        Accessible.name: qsTr("Visit makineai.com")
        activeFocusOnTab: true
        Keys.onReturnPressed: Qt.openUrlExternally("https://makineai.com")
        Keys.onSpacePressed: Qt.openUrlExternally("https://makineai.com")

        Row {
            anchors.centerIn: parent; spacing: 4
            opacity: secFooter.hovered ? 0.95 : 0.55
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Image {
                width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
                sourceSize: Qt.size(12, 12)
            }

            Label {
                text: qsTr("G\u00FCvenli\u011Finiz i\u00E7in yaln\u0131zca")
                font.pixelSize: Dimensions.fontMini; color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: "makineai.com"
                font.pixelSize: Dimensions.fontMini; font.weight: Font.Medium; font.underline: secFooter.hovered
                color: secFooter.hovered ? Theme.primary : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 200 } }
            }
            Label {
                text: qsTr("\u00FCzerinden indirin")
                font.pixelSize: Dimensions.fontMini; color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            anchors.fill: parent; anchors.margins: -1; radius: parent.radius + 1
            color: "transparent"; border.color: Theme.withAlpha(Theme.primary, 0.6); border.width: 2
            visible: parent.activeFocus
        }

        MouseArea {
            id: secMa; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.openUrlExternally("https://makineai.com")
        }
    }
}
