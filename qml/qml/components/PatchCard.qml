import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * PatchCard.qml - Translation patch card for "Türkçe Yamalar" page
 *
 * Three states:
 * - Available (blue badge): patch exists, not installed. Hover → download icon
 * - Installed (green badge): patch installed. Hover → dark overlay + red X
 * - Installing (progress): progress bar overlay with percentage
 *
 * Matches GameCard quality: rainbow border, scale transform, MultiEffect masking
 */
Item {
    id: root

    property string gameId: ""
    property string gameName: ""
    property string imageUrl: ""
    property bool packageInstalled: false
    property bool isInstalling: false
    property double installProgress: 0.0

    signal installClicked()
    signal uninstallClicked()
    signal cardClicked()

    width: Dimensions.cardWidth + Dimensions.patchCardExtraWidth   // 150
    height: Dimensions.cardHeight + Dimensions.patchCardExtraHeight // 230

    property bool isHovered: cardMouse.containsMouse || uninstallBtnMouse.containsMouse

    // Hover transforms: lift + subtle scale
    transform: [
        Translate {
            y: root.isHovered ? -4 : 0
            Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        },
        Scale {
            origin.x: root.width / 2
            origin.y: root.height / 2
            xScale: root.isHovered ? 1.02 : 1.0
            yScale: root.isHovered ? 1.02 : 1.0
            Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        }
    ]

    // Image container
    Rectangle {
        id: imgContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Dimensions.cardImageHeight + 40  // 200
        radius: Dimensions.cardBorderRadius
        color: Theme.withAlpha(Theme.textPrimary, 0.04)
        clip: true

        // ===== RAINBOW BORDER ANIMATION =====
        property real borderPhase: 0
        NumberAnimation on borderPhase {
            from: 0; to: 1
            duration: 8000
            loops: Animation.Infinite
            running: root.isHovered && !root.isInstalling
        }

        Canvas {
            anchors.fill: parent
            z: Dimensions.zContent
            property real phase: imgContainer.borderPhase
            onPhaseChanged: if (hov) requestPaint()
            property bool hov: root.isHovered
            onHovChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var angle = phase * Math.PI * 2
                var cx = width / 2, cy = height / 2
                var len = Math.max(width, height) * 0.7
                var x1 = cx + Math.cos(angle) * len
                var y1 = cy + Math.sin(angle) * len
                var x2 = cx - Math.cos(angle) * len
                var y2 = cy - Math.sin(angle) * len

                var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                var colors = Theme.brandGradient
                for (var i = 0; i < colors.length; i++)
                    grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                var r = Dimensions.cardBorderRadius
                var bw = 1.5
                var px = bw / 2, py = bw / 2
                var w = width - bw, h = height - bw

                ctx.beginPath()
                ctx.moveTo(px + r, py)
                ctx.lineTo(px + w - r, py)
                ctx.arcTo(px + w, py, px + w, py + r, r)
                ctx.lineTo(px + w, py + h - r)
                ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                ctx.lineTo(px + r, py + h)
                ctx.arcTo(px, py + h, px, py + h - r, r)
                ctx.lineTo(px, py + r)
                ctx.arcTo(px, py, px + r, py, r)
                ctx.closePath()

                ctx.globalAlpha = hov ? 0.8 : 0.0
                ctx.strokeStyle = grad
                ctx.lineWidth = bw
                ctx.stroke()
            }
        }

        // ===== MULTIEFFECT IMAGE MASKING =====
        Item {
            id: imageMask
            anchors.fill: parent
            visible: false
            layer.enabled: true
            Rectangle {
                anchors.fill: parent
                radius: Dimensions.cardBorderRadius
                color: "white"
            }
        }

        Image {
            id: gameImg
            anchors.fill: parent
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            sourceSize: Qt.size(imgContainer.width * 2, imgContainer.height * 2)
            visible: false
        }

        MultiEffect {
            id: maskedImage
            anchors.fill: parent
            source: gameImg
            maskEnabled: true
            maskSource: imageMask

            // Subtle brightness on hover
            brightness: root.isHovered && !root.isInstalling ? 0.06 : 0
            Behavior on brightness { NumberAnimation { duration: Dimensions.animFast } }

            opacity: 0
            states: State {
                name: "loaded"
                when: gameImg.status === Image.Ready
                PropertyChanges { target: maskedImage; opacity: 1.0 }
            }
            transitions: Transition {
                NumberAnimation { property: "opacity"; duration: Dimensions.fadeTransitionDuration; easing.type: Easing.OutCubic }
            }
        }

        // Skeleton placeholder (visible while loading)
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: gameImg.status !== Image.Ready
            color: Theme.withAlpha(Theme.textPrimary, 0.03)

            // Loading spinner
            BusyIndicator {
                anchors.centerIn: parent
                width: 20; height: 20
                running: gameImg.status === Image.Loading
                visible: gameImg.status === Image.Loading
            }

            // Game name as fallback text (on error or no URL)
            Label {
                anchors.centerIn: parent
                visible: gameImg.status !== Image.Loading
                text: root.gameName
                font.pixelSize: Dimensions.fontXS
                font.weight: Font.Medium
                color: Theme.textMuted
                width: parent.width - Dimensions.marginMD * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        // Bottom gradient overlay for badge/text contrast
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.35
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.withAlpha("#000000", 0.45) }
            }
        }

        // ===== STATUS BADGE (top-right) =====
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: Dimensions.marginSM
            anchors.rightMargin: Dimensions.marginSM
            visible: !root.isInstalling
            implicitWidth: badgeRow.width + Dimensions.marginSM
            implicitHeight: 20
            radius: Dimensions.radiusStandard
            color: root.packageInstalled
                ? Theme.withAlpha(Theme.success, 0.90)
                : Theme.withAlpha(Theme.primary, 0.90)
            z: 2

            Row {
                id: badgeRow
                anchors.centerIn: parent
                spacing: 3

                // Checkmark or download icon
                Label {
                    text: root.packageInstalled ? "\u2714" : "\u2B07"
                    font.pixelSize: 8
                    color: Theme.textOnColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: root.packageInstalled ? qsTr("Kurulu") : qsTr("Mevcut")
                    font.pixelSize: Dimensions.fontMini
                    font.weight: Font.Bold
                    color: Theme.textOnColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // ===== HOVER OVERLAY: Available (not installed) → download prompt =====
        Rectangle {
            id: installOverlay
            anchors.fill: parent
            radius: parent.radius
            color: Theme.withAlpha(Theme.bgPrimary, 0.55)
            opacity: cardMouse.containsMouse && !root.packageInstalled && !root.isInstalling ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                // Download circle icon
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40; height: 40
                    radius: 20
                    color: Theme.withAlpha(Theme.primary, 0.20)
                    border.color: Theme.withAlpha(Theme.primary, 0.40)
                    border.width: 1

                    Canvas {
                        anchors.centerIn: parent
                        width: 18; height: 18
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = Theme.primary
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            // Down arrow shaft
                            ctx.beginPath()
                            ctx.moveTo(9, 2)
                            ctx.lineTo(9, 13)
                            ctx.stroke()
                            // Arrow head
                            ctx.beginPath()
                            ctx.moveTo(5, 10)
                            ctx.lineTo(9, 14)
                            ctx.lineTo(13, 10)
                            ctx.stroke()
                            // Bottom line
                            ctx.beginPath()
                            ctx.moveTo(3, 16)
                            ctx.lineTo(15, 16)
                            ctx.stroke()
                        }
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Yama Kur")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.primary
                }
            }
        }

        // ===== HOVER OVERLAY: Installed → uninstall option =====
        Rectangle {
            id: uninstallOverlay
            anchors.fill: parent
            radius: parent.radius
            color: Theme.withAlpha(Theme.bgPrimary, 0.65)
            opacity: (cardMouse.containsMouse || uninstallBtnMouse.containsMouse) && root.packageInstalled && !root.isInstalling ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                // Red X circle button
                Rectangle {
                    id: uninstallBtn
                    Layout.alignment: Qt.AlignHCenter
                    width: 40; height: 40
                    radius: 20
                    color: uninstallBtnMouse.containsMouse
                        ? Theme.error
                        : Theme.withAlpha(Theme.error, 0.20)
                    border.color: Theme.withAlpha(Theme.error, 0.50)
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    Canvas {
                        anchors.centerIn: parent
                        width: 14; height: 14
                        property color strokeColor: uninstallBtnMouse.containsMouse ? Theme.textOnColor : Theme.error
                        onStrokeColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = strokeColor
                            ctx.lineWidth = 2.2
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(3, 3)
                            ctx.lineTo(11, 11)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(11, 3)
                            ctx.lineTo(3, 11)
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: uninstallBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.uninstallClicked()
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Yamayı Kaldır")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: uninstallBtnMouse.containsMouse ? Theme.error : Theme.textSecondary
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }
            }
        }

        // ===== INSTALLING OVERLAY: progress bar + percentage =====
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.withAlpha(Theme.bgPrimary, 0.75)
            visible: root.isInstalling

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    width: 24; height: 24
                    running: root.isInstalling
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.installProgress > 0
                        ? qsTr("Kuruluyor... %1%").arg(Math.round(root.installProgress * 100))
                        : qsTr("Hazırlanıyor...")
                    font.pixelSize: Dimensions.fontXS
                    font.weight: Font.Medium
                    color: Theme.primary
                }

                // Progress bar
                Rectangle {
                    Layout.preferredWidth: imgContainer.width * 0.65
                    Layout.preferredHeight: 4
                    Layout.alignment: Qt.AlignHCenter
                    radius: 2
                    color: Theme.withAlpha(Theme.primary, 0.15)

                    Rectangle {
                        width: parent.width * root.installProgress
                        height: parent.height
                        radius: parent.radius
                        color: Theme.primary
                        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }

        // ===== MAIN CLICK AREA =====
        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.isInstalling) return
                if (!root.packageInstalled) {
                    root.installClicked()
                } else {
                    root.cardClicked()
                }
            }
        }
    }

    // ===== GAME NAME =====
    Label {
        anchors.top: imgContainer.bottom
        anchors.topMargin: Dimensions.spacingXS
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.gameName
        font.pixelSize: Dimensions.fontXS
        font.weight: Font.Medium
        color: root.isHovered ? Theme.textPrimary : Theme.textSecondary
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        elide: Text.ElideRight

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        ToolTip.visible: truncated && cardMouse.containsMouse
        ToolTip.text: root.gameName
        ToolTip.delay: Dimensions.tooltipDelay
    }

    // ===== FOCUS INDICATOR =====
    Rectangle {
        anchors.fill: imgContainer
        anchors.margins: -2
        radius: Dimensions.cardBorderRadius + 2
        color: "transparent"
        border.color: Theme.borderFocus
        border.width: 2
        visible: root.activeFocus
    }

    // ===== ACCESSIBILITY =====
    Accessible.role: Accessible.Button
    Accessible.name: root.gameName + (root.packageInstalled ? " — " + qsTr("Kurulu") : " — " + qsTr("Mevcut"))
    Accessible.description: root.isInstalling
        ? qsTr("Installing, %1 percent").arg(Math.round(root.installProgress * 100))
        : ""
    activeFocusOnTab: true
    Keys.onReturnPressed: cardMouse.clicked(null)
    Keys.onSpacePressed: cardMouse.clicked(null)
}
