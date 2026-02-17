import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

Rectangle {
    id: root

    property bool animationsEnabled: true
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    signal manualFolderRequested()

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 2
    Layout.preferredHeight: layoutTopRowHeight

    radius: 20
    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1
    clip: true

    // Ambient glow (soft purple, radial gradient)
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
            // Radial glow at top-right
            var cx = width - 40
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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20; anchors.rightMargin: 20
        anchors.topMargin: 18; anchors.bottomMargin: 18
        spacing: 16

        // Ring
        Item {
            Layout.preferredWidth: 56; Layout.preferredHeight: 56
            Layout.alignment: Qt.AlignVCenter

            // Static track ring
            Rectangle {
                anchors.fill: parent; radius: width / 2
                color: "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
            }

            // Brand gradient comet arc — painted once, rotated by GPU
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

                    // Brand gradient colors for the comet trail
                    var colors = [
                        [252, 205, 102],  // gold
                        [247, 174, 118],  // orange
                        [238, 150, 143],  // coral
                        [204, 159, 216],  // purple
                        [144, 194, 230],  // blue
                        [119, 219, 200]   // teal
                    ]

                    ctx.lineCap = "round"

                    // Glow layer (wider, softer)
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

                    // Main arc (crisp brand gradient trail)
                    ctx.lineWidth = 2
                    for (var i = 0; i < segs; i++) {
                        var t = i / segs
                        var s = -Math.PI / 2 + t * sweep
                        var e = -Math.PI / 2 + (t + 1.5 / segs) * sweep
                        var alpha = (0.03 + 0.6 * t * t).toFixed(3)

                        // Interpolate through brand colors
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

                    // Leading Turkish flag star (5-pointed) at arc head
                    var headAngle = -Math.PI / 2 + sweep
                    var sx = cx + rad * Math.cos(headAngle)
                    var sy = cy + rad * Math.sin(headAngle)
                    var starPts = 5, outerR = 4.5, innerR = outerR * 0.382
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
                anchors.centerIn: parent; width: 30; height: 30
                radius: 15; color: "#E30A17"; clip: true

                Rectangle { x: 5; y: 9; width: 12; height: 12; radius: 6; color: "#FFFFFF" }
                Rectangle { x: 8; y: 10; width: 10; height: 10; radius: 5; color: "#E30A17" }
                Canvas {
                    x: 16; y: 11; width: 8; height: 8; antialiasing: true
                    onPaint: {
                        var ctx = getContext("2d")
                        var cx = 4, cy = 4, R = 3.5, r = R * 0.382
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
                text: qsTr("Sistem Beklemede")
                font.pixelSize: Dimensions.fontXL; font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("MakineAI oyun aktivitesi bekliyor. Desteklenen bir oyun ba\u015Flat\u0131n veya manuel olarak tarama yap\u0131n.")
                font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                wrapMode: Text.WordWrap; lineHeight: 1.4
                maximumLineCount: 3; elide: Text.ElideRight
            }
        }

        // Button
        Item {
            id: btn
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: btnLbl.implicitWidth + 40
            Layout.preferredHeight: 38

            property bool hovered: btnMa.containsMouse

            scale: hovered ? 1.03 : 1.0
            Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

            // Button background
            Rectangle {
                id: btnBg
                anchors.fill: parent
                radius: 10; color: btn.hovered ? "#F5F5F5" : "#FFFFFF"
                Behavior on color { ColorAnimation { duration: 250 } }
            }

            Label {
                id: btnLbl
                anchors.centerIn: parent
                text: qsTr("+ Oyun Ekle")
                font.pixelSize: Dimensions.fontSM; font.weight: Font.Bold
                color: "#000"
            }

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Manuel Oyun Ekle")
            activeFocusOnTab: true
            Keys.onReturnPressed: root.manualFolderRequested()
            Keys.onSpacePressed: root.manualFolderRequested()

            Rectangle {
                anchors.fill: parent; anchors.margins: -2; radius: 12
                color: "transparent"; border.color: Theme.withAlpha(Theme.primary, 0.6); border.width: 2
                visible: btn.activeFocus
            }
            MouseArea {
                id: btnMa; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.manualFolderRequested()
            }
        }
    }
}
