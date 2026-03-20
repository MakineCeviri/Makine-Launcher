import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: root

    property bool animationsEnabled: true
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    // Dynamic game detection state
    property bool detectionEnabled: SettingsManager.autoDetectGames
    property bool gameDetected: detectionEnabled && ProcessScanner.gameRunning
    property string detectedGameName: ProcessScanner.runningGameName
    property var heavyProcs: ProcessScanner.heavyProcesses
    property bool hasHeavyProcesses: detectionEnabled && heavyProcs.length > 0 && !gameDetected

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

        GradientBorder { cornerRadius: parent.radius }

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
                // Wrapper rotates the pre-rendered texture via GPU transform (no per-frame rasterization)
                Item {
                    id: arcWrapper
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height

                    NumberAnimation on rotation {
                        from: 0; to: 360; duration: 3000
                        loops: Animation.Infinite; running: root.visible && root.animationsEnabled
                        easing.type: Easing.Linear
                    }

                    Canvas {
                        id: arcCanvas
                        anchors.fill: parent
                        renderStrategy: Canvas.Cooperative

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
                }

                // Turkish flag icon
                Rectangle {
                    anchors.centerIn: parent; width: 24; height: 24
                    radius: 12; color: Theme.turkishRed
                    Rectangle { x: 4; y: 7; width: 10; height: 10; radius: 5; color: "#FFFFFF" }
                    Rectangle { x: 6.5; y: 8; width: 8; height: 8; radius: 4; color: Theme.turkishRed }
                    Canvas {
                        x: 13; y: 9; width: 6; height: 6
                        renderStrategy: Canvas.Cooperative
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
                    text: !root.detectionEnabled ? qsTr("Oyun Tespiti Devre D\u0131\u015F\u0131")
                          : root.gameDetected ? root.detectedGameName + qsTr(" \u00e7al\u0131\u015f\u0131yor")
                          : root.hasHeavyProcesses ? qsTr("Oyun tespit ediliyor...")
                          : qsTr("Oyun Tespit Edilemedi")
                    font.pixelSize: Dimensions.fontLG; font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !root.gameDetected && !root.hasHeavyProcesses
                    text: !root.detectionEnabled
                          ? qsTr("Ayarlardan otomatik oyun tespitini a\u00e7abilirsiniz.")
                          : qsTr("Desteklenen bir oyun \u00e7al\u0131\u015ft\u0131r\u0131n veya bir oyun ekleyin.")
                    font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                    wrapMode: Text.WordWrap; lineHeight: 1.4
                    maximumLineCount: 3; elide: Text.ElideRight
                }

                // Game running description
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: root.gameDetected
                    text: qsTr("T\u00fcrk\u00e7e yama y\u00fcklemek i\u00e7in oyunu kapat\u0131n.")
                    font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                    wrapMode: Text.WordWrap; lineHeight: 1.4
                }

                // Heavy process selection list
                Column {
                    Layout.fillWidth: true
                    visible: root.hasHeavyProcesses
                    spacing: 3

                    Label {
                        textFormat: Text.PlainText
                        text: qsTr("Oyununuz bunlardan biri olabilir:")
                        font.pixelSize: Dimensions.fontXS; color: Theme.textMuted
                    }

                    Repeater {
                        model: root.heavyProcs

                        Rectangle {
                            id: procItem
                            required property var modelData
                            required property int index
                            width: parent.width
                            height: 26
                            radius: 6
                            color: procMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                            Behavior on color { ColorAnimation { duration: 120 } }

                            Label {
                                textFormat: Text.PlainText
                                text: procItem.modelData.name
                                font.pixelSize: Dimensions.fontXS
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.leftMargin: 6; anchors.rightMargin: 6
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: procMa; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: ProcessScanner.resolveSelectedProcess(procItem.modelData.pid)
                            }
                        }
                    }
                }
            }

            // Premium CTA button
            Item {
                id: btn
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: btnRow.implicitWidth + 32
                Layout.preferredHeight: 38

                scale: btnMa.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

                // Button body
                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: btnMa.containsMouse ? "#FFFFFF" : "#F8F8FA" }
                        GradientStop { position: 1.0; color: btnMa.containsMouse ? "#F2F0F4" : "#ECEAEF" }
                    }

                    // Top highlight
                    Rectangle {
                        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                        anchors.topMargin: 1; anchors.leftMargin: 4; anchors.rightMargin: 4
                        height: 1; radius: 1
                        color: Qt.rgba(1, 1, 1, 0.85)
                    }
                }

                Row {
                    id: btnRow
                    anchors.centerIn: parent
                    spacing: 6

                    // Gamepad icon
                    Canvas {
                        width: 16; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        renderStrategy: Canvas.Cooperative
                        property color _accent: Theme.accentDark
                        on_AccentChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = _accent; ctx.fillStyle = _accent
                            ctx.lineWidth = 1.2; ctx.lineCap = "round"; ctx.lineJoin = "round"
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
                            ctx.lineWidth = 1.2
                            ctx.beginPath(); ctx.moveTo(4, 6); ctx.lineTo(6.5, 6); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(5.25, 4.5); ctx.lineTo(5.25, 7.5); ctx.stroke()
                            ctx.beginPath(); ctx.arc(10.5, 5, 0.8, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.arc(12.5, 7, 0.8, 0, Math.PI * 2); ctx.fill()
                        }
                    }

                    Label {
                        textFormat: Text.PlainText
                        text: root.gameDetected ? qsTr("Kütüphaneye Git") : qsTr("Oyun Ekle")
                        font.pixelSize: Dimensions.fontSM; font.weight: Font.Bold
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

        GradientBorder {
            cornerRadius: 14
            topColor: secMa.containsMouse ? Theme.accentBase30 : Qt.rgba(1, 1, 1, 0.10)
            bottomColor: secMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.02)
            Behavior on topColor { ColorAnimation { duration: Dimensions.animNormal } }
        }

        Row {
            anchors.centerIn: parent; spacing: 4
            opacity: secMa.containsMouse ? 0.95 : 0.55

            Canvas {
                width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter
                renderStrategy: Canvas.Cooperative
                property color _c: Theme.accentLight
                on_CChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var r = Math.round(_c.r * 255), g = Math.round(_c.g * 255), b = Math.round(_c.b * 255)
                    ctx.strokeStyle = "rgb(" + r + "," + g + "," + b + ")"
                    ctx.lineWidth = 1.2; ctx.lineCap = "round"; ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(6, 1)
                    ctx.lineTo(10.5, 3); ctx.lineTo(10.5, 6.5)
                    ctx.quadraticCurveTo(10.5, 10.5, 6, 11.5)
                    ctx.quadraticCurveTo(1.5, 10.5, 1.5, 6.5)
                    ctx.lineTo(1.5, 3); ctx.closePath(); ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(3.8, 6.2); ctx.lineTo(5.3, 7.8); ctx.lineTo(8.2, 4.8)
                    ctx.stroke()
                }
            }
            Label {
                textFormat: Text.PlainText
                text: qsTr("G\u00FCvenli\u011Finiz i\u00E7in yaln\u0131zca")
                font.pixelSize: Dimensions.fontMini; color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                textFormat: Text.PlainText
                text: "makineceviri.org"
                font.pixelSize: Dimensions.fontMini; font.weight: Font.DemiBold
                color: Theme.accentLight
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
            onClicked: Qt.openUrlExternally("https://makineceviri.org")
        }
    }
}
