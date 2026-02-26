import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: root

    property bool animationsEnabled: true
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    signal manualFolderRequested()

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 1
    Layout.preferredHeight: layoutTopRowHeight
    spacing: 6

    // Main card
    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true
        radius: Dimensions.radiusSection
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1

        // Ambient glow
        AmbientGlow {
            anchors.fill: parent
            cornerRadius: Dimensions.radiusSection
            position: "top-right"
            intensity: 0.12
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16; anchors.rightMargin: 16
            anchors.topMargin: 14; anchors.bottomMargin: 14
            spacing: 12

            // Ring with rotating comet arc
            Item {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignVCenter

                // Static track ring
                Rectangle {
                    anchors.fill: parent; radius: width / 2
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
                }

                // Rotating gradient comet arc + star
                Canvas {
                    id: arcCanvas
                    anchors.fill: parent
                    NumberAnimation on rotation {
                        from: 0; to: 360; duration: 3000
                        loops: Animation.Infinite; running: root.visible && root.animationsEnabled
                        easing.type: Easing.Linear
                    }

                    Component.onCompleted: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var cx = width / 2, cy = height / 2, rad = (width - 4) / 2
                        var segs = 36, sweep = Math.PI * 0.9

                        var colors = [
                            [252, 205, 102], [247, 174, 118], [238, 150, 143],
                            [204, 159, 216], [144, 194, 230], [119, 219, 200]
                        ]

                        ctx.lineCap = "round"

                        // Glow layer
                        ctx.lineWidth = 4
                        for (var g = 0; g < segs; g++) {
                            var gt = g / segs
                            var gs = -Math.PI / 2 + gt * sweep
                            var ge = -Math.PI / 2 + (gt + 1.5 / segs) * sweep
                            var ga = (0.08 * gt * gt).toFixed(3)
                            var gi = Math.floor(gt * (colors.length - 1))
                            var gc = colors[Math.min(gi, colors.length - 1)]
                            ctx.beginPath(); ctx.arc(cx, cy, rad, gs, ge)
                            ctx.strokeStyle = "rgba(" + gc[0] + "," + gc[1] + "," + gc[2] + "," + ga + ")"
                            ctx.stroke()
                        }

                        // Main arc
                        ctx.lineWidth = 2
                        for (var i = 0; i < segs; i++) {
                            var t = i / segs
                            var s = -Math.PI / 2 + t * sweep
                            var e = -Math.PI / 2 + (t + 1.5 / segs) * sweep
                            var alpha = (0.03 + 0.6 * t * t).toFixed(3)

                            var ci = t * (colors.length - 1)
                            var idx = Math.min(Math.floor(ci), colors.length - 2)
                            var frac = ci - idx
                            var c1 = colors[idx], c2 = colors[idx + 1]
                            var cr = Math.round(c1[0] + (c2[0] - c1[0]) * frac)
                            var cg = Math.round(c1[1] + (c2[1] - c1[1]) * frac)
                            var cb = Math.round(c1[2] + (c2[2] - c1[2]) * frac)

                            ctx.beginPath(); ctx.arc(cx, cy, rad, s, e)
                            ctx.strokeStyle = "rgba(" + cr + "," + cg + "," + cb + "," + alpha + ")"
                            ctx.stroke()
                        }

                        // Leading star
                        var headAngle = -Math.PI / 2 + sweep
                        var sx = cx + rad * Math.cos(headAngle)
                        var sy = cy + rad * Math.sin(headAngle)
                        var starPts = 5, outerR = 3.5, innerR = outerR * 0.382
                        ctx.beginPath()
                        for (var p = 0; p < starPts * 2; p++) {
                            var a = p * Math.PI / starPts - Math.PI / 2
                            var pr = (p % 2 === 0) ? outerR : innerR
                            if (p === 0) ctx.moveTo(sx + pr * Math.cos(a), sy + pr * Math.sin(a))
                            else ctx.lineTo(sx + pr * Math.cos(a), sy + pr * Math.sin(a))
                        }
                        ctx.closePath()
                        ctx.fillStyle = "rgba(255, 255, 255, 0.9)"
                        ctx.fill()
                    }
                }

                // Turkish flag icon
                Rectangle {
                    anchors.centerIn: parent; width: 24; height: 24
                    radius: 12; color: "#E30A17"
                    Rectangle { x: 4; y: 7; width: 10; height: 10; radius: 5; color: "#FFFFFF" }
                    Rectangle { x: 6.5; y: 8; width: 8; height: 8; radius: 4; color: "#E30A17" }
                    Canvas {
                        x: 13; y: 9; width: 6; height: 6
                        onPaint: {
                            var ctx = getContext("2d")
                            var cx = 3, cy = 3, R = 2.8, r = R * 0.382
                            ctx.beginPath()
                            for (var i = 0; i < 5; i++) {
                                var oa = i * 72 * Math.PI / 180
                                var ia = (i * 72 + 36) * Math.PI / 180
                                if (i === 0) ctx.moveTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                                else ctx.lineTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                                ctx.lineTo(cx - r * Math.cos(ia), cy - r * Math.sin(ia))
                            }
                            ctx.closePath(); ctx.fillStyle = "white"; ctx.fill()
                        }
                    }
                }
            }

            // Text
            ColumnLayout {
                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter
                spacing: 4

                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Oyun Tespit Edilemedi")
                    font.pixelSize: Dimensions.fontLG; font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Desteklenen bir oyun bulunamad\u0131. Oyununuzu ba\u015Flat\u0131n ya da a\u015Fa\u011F\u0131dan manuel olarak ekleyin.")
                    font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                    wrapMode: Text.WordWrap; lineHeight: 1.4
                    maximumLineCount: 3; elide: Text.ElideRight
                }
            }

            // Button
            Item {
                id: btn
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: btnRow.implicitWidth + 28
                Layout.preferredHeight: 34

                scale: btnMa.containsMouse ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors.fill: parent
                    radius: 8; color: btnMa.containsMouse ? "#F5F5F5" : "#FFFFFF"
                }

                Row {
                    id: btnRow
                    anchors.centerIn: parent
                    spacing: 6

                    // Gamepad icon
                    Canvas {
                        width: 16; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var c = Theme.accentDark
                            ctx.strokeStyle = c; ctx.fillStyle = c
                            ctx.lineWidth = 1.2; ctx.lineCap = "round"; ctx.lineJoin = "round"
                            // Body
                            ctx.beginPath()
                            ctx.moveTo(4, 1); ctx.lineTo(12, 1)
                            ctx.quadraticCurveTo(15, 1, 15, 4)
                            ctx.lineTo(15, 6)
                            ctx.quadraticCurveTo(15, 11, 12, 11)
                            ctx.lineTo(4, 11)
                            ctx.quadraticCurveTo(1, 11, 1, 6)
                            ctx.lineTo(1, 4)
                            ctx.quadraticCurveTo(1, 1, 4, 1)
                            ctx.closePath(); ctx.stroke()
                            // D-pad
                            ctx.lineWidth = 1.2
                            ctx.beginPath(); ctx.moveTo(4, 6); ctx.lineTo(6.5, 6); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(5.25, 4.5); ctx.lineTo(5.25, 7.5); ctx.stroke()
                            // Buttons
                            ctx.beginPath(); ctx.arc(10.5, 5, 0.8, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.arc(12.5, 7, 0.8, 0, Math.PI * 2); ctx.fill()
                        }
                    }

                    Label {
                        textFormat: Text.PlainText
                        id: btnLbl
                        text: qsTr("Oyun Ekle")
                        font.pixelSize: Dimensions.fontXS; font.weight: Font.Bold
                        color: Theme.accentDark
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: btnMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.manualFolderRequested()
                }
            }
        }
    }

    // Security footer
    Rectangle {
        id: secFooter
        Layout.fillWidth: true; Layout.preferredHeight: 34
        radius: 14
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: secMa.containsMouse ? Theme.withAlpha(Theme.accentBase, 0.30) : Qt.rgba(1, 1, 1, 0.06)
        border.width: 1

        // Ambient glow
        AmbientGlow {
            anchors.fill: parent
            cornerRadius: 14
            originX: parent.width - 20; originY: -10
            intensity: 0.10; spread: 0.6
        }

        Row {
            anchors.centerIn: parent; spacing: 4
            opacity: secMa.containsMouse ? 0.95 : 0.55

            Image {
                width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
                sourceSize: Qt.size(12, 12)
                asynchronous: true
            }
            Label {
                textFormat: Text.PlainText
                text: qsTr("G\u00FCvenli\u011Finiz i\u00E7in yaln\u0131zca")
                font.pixelSize: Dimensions.fontMini; color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                textFormat: Text.PlainText
                text: "makineai.com"
                font.pixelSize: Dimensions.fontMini; font.weight: Font.Medium
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                textFormat: Text.PlainText
                text: qsTr("\u00FCzerinden indirin")
                font.pixelSize: Dimensions.fontMini; color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            id: secMa; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.openUrlExternally("https://makineai.com")
        }
    }
}
