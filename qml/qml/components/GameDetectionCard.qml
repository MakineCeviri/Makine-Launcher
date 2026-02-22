import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

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

    // ── System status card ──
    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true
        radius: Dimensions.radiusSection
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        clip: true

        // Ambient glow (accent-colored, radial gradient)
        Canvas {
            anchors.fill: parent
            property color glowColor: Theme.accentDark
            onGlowColorChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cr = Dimensions.radiusSection
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
                var cx = width - 40
                var cy = 30
                var r = Math.max(width, height) * 0.55
                var gc = glowColor
                var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
                grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + ",0.12)")
                grad.addColorStop(0.3, "rgba(" + R + "," + G + "," + B + ",0.06)")
                grad.addColorStop(0.6, "rgba(" + R + "," + G + "," + B + ",0.02)")
                grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
                ctx.fillStyle = grad
                ctx.fillRect(0, 0, width, height)
            }
        }

        // Content — horizontal layout matching original
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16; anchors.rightMargin: 16
            anchors.topMargin: 14; anchors.bottomMargin: 14
            spacing: 12

            // Ring
            Item {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignVCenter

                // Static track ring
                Rectangle {
                    anchors.fill: parent; radius: width / 2
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
                }

                // Brand gradient comet arc
                Canvas {
                    id: arcCanvas
                    anchors.fill: parent
                    rotation: 0
                    NumberAnimation on rotation {
                        from: 0; to: 360; duration: 3000
                        loops: Animation.Infinite; running: root.visible
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

                // Turkish flag icon (circular)
                Rectangle {
                    anchors.centerIn: parent; width: 24; height: 24
                    radius: 12; color: "#E30A17"; clip: true

                    Rectangle { x: 4; y: 7; width: 10; height: 10; radius: 5; color: "#FFFFFF" }
                    Rectangle { x: 6.5; y: 8; width: 8; height: 8; radius: 4; color: "#E30A17" }
                    Canvas {
                        x: 13; y: 9; width: 6; height: 6; antialiasing: true
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
                    text: qsTr("Oyun Tespit Edilemedi")
                    font.pixelSize: Dimensions.fontLG; font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Label {
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
                Layout.preferredWidth: btnContent.implicitWidth + 28
                Layout.preferredHeight: 34

                property bool hovered: btnMa.containsMouse

                scale: hovered ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors.fill: parent
                    radius: 8; color: btn.hovered ? "#F5F5F5" : "#FFFFFF"
                    Behavior on color { ColorAnimation { duration: Dimensions.animNormal } }
                }

                Row {
                    id: btnContent
                    anchors.centerIn: parent
                    spacing: 6

                    // Minimal gamepad icon
                    Canvas {
                        width: 16; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var c = Theme.accentDark
                            ctx.strokeStyle = c; ctx.fillStyle = c
                            ctx.lineWidth = 1.2; ctx.lineCap = "round"; ctx.lineJoin = "round"

                            // Body — rounded pill shape
                            ctx.beginPath()
                            ctx.moveTo(4, 1); ctx.lineTo(12, 1)
                            ctx.quadraticCurveTo(15, 1, 15, 4)
                            ctx.lineTo(15, 6)
                            ctx.quadraticCurveTo(15, 11, 12, 11)
                            ctx.lineTo(4, 11)
                            ctx.quadraticCurveTo(1, 11, 1, 6)
                            ctx.lineTo(1, 4)
                            ctx.quadraticCurveTo(1, 1, 4, 1)
                            ctx.closePath()
                            ctx.stroke()

                            // D-pad (left side) — small cross
                            ctx.lineWidth = 1.2
                            ctx.beginPath(); ctx.moveTo(4, 6); ctx.lineTo(6.5, 6); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(5.25, 4.5); ctx.lineTo(5.25, 7.5); ctx.stroke()

                            // Buttons (right side) — two dots
                            ctx.beginPath(); ctx.arc(10.5, 5, 0.8, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.arc(12.5, 7, 0.8, 0, Math.PI * 2); ctx.fill()
                        }
                    }

                    Label {
                        id: btnLbl
                        text: qsTr("Oyun Ekle")
                        font.pixelSize: Dimensions.fontXS; font.weight: Font.Bold
                        color: Theme.accentDark
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Manuel Oyun Ekle")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.manualFolderRequested()
                Keys.onSpacePressed: root.manualFolderRequested()

                FocusRing { target: btn }
                MouseArea {
                    id: btnMa; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.manualFolderRequested()
                }
            }
        }
    }

    // ── Security footer with ambient glow continuation ──
    Rectangle {
        id: secFooter
        Layout.fillWidth: true; Layout.preferredHeight: 34
        radius: 14
        property bool hovered: secMa.containsMouse
        color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
        border.color: hovered ? Theme.withAlpha(Theme.accentBase, 0.30) : Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        clip: true
        Behavior on border.color { ColorAnimation { duration: Dimensions.animMedium } }

        // Subtle ambient glow continuation from system status card
        Canvas {
            anchors.fill: parent
            property color glowColor: Theme.accentDark
            onGlowColorChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var r = 14
                ctx.beginPath()
                ctx.moveTo(r, 0); ctx.lineTo(width - r, 0)
                ctx.quadraticCurveTo(width, 0, width, r)
                ctx.lineTo(width, height - r)
                ctx.quadraticCurveTo(width, height, width - r, height)
                ctx.lineTo(r, height)
                ctx.quadraticCurveTo(0, height, 0, height - r)
                ctx.lineTo(0, r)
                ctx.quadraticCurveTo(0, 0, r, 0)
                ctx.closePath(); ctx.clip()
                var gc = glowColor
                var R = Math.round(gc.r * 255), G = Math.round(gc.g * 255), B = Math.round(gc.b * 255)
                var grad = ctx.createRadialGradient(width - 20, -10, 0, width - 20, -10, width * 0.6)
                grad.addColorStop(0.0, "rgba(" + R + "," + G + "," + B + ",0.10)")
                grad.addColorStop(0.4, "rgba(" + R + "," + G + "," + B + ",0.04)")
                grad.addColorStop(1.0, "rgba(" + R + "," + G + "," + B + ",0.0)")
                ctx.fillStyle = grad
                ctx.fillRect(0, 0, width, height)
            }
        }

        Accessible.role: Accessible.Link
        Accessible.name: qsTr("Visit makineai.com")
        activeFocusOnTab: true
        Keys.onReturnPressed: Qt.openUrlExternally("https://makineai.com")
        Keys.onSpacePressed: Qt.openUrlExternally("https://makineai.com")

        Row {
            anchors.centerIn: parent; spacing: 4
            opacity: secFooter.hovered ? 0.95 : 0.55
            Behavior on opacity { NumberAnimation { duration: Dimensions.animMedium } }

            Image {
                width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
                sourceSize: Qt.size(12, 12)
            }

            Label {
                text: qsTr("G\u00FCvenli\u011Finiz i\u00E7in yaln\u0131zca")
                font.pixelSize: Dimensions.fontMini
                color: secFooter.hovered ? Theme.accentLight : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: Dimensions.animMedium } }
            }
            Label {
                text: "makineai.com"
                font.pixelSize: Dimensions.fontMini; font.weight: Font.Medium; font.underline: secFooter.hovered
                color: secFooter.hovered ? Theme.accentBase : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: Dimensions.animMedium } }
            }
            Label {
                text: qsTr("\u00FCzerinden indirin")
                font.pixelSize: Dimensions.fontMini
                color: secFooter.hovered ? Theme.accentLight : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: Dimensions.animMedium } }
            }
        }

        FocusRing { offset: -1 }

        MouseArea {
            id: secMa; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.openUrlExternally("https://makineai.com")
        }
    }
}
