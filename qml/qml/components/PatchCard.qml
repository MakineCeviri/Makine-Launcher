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
    property bool hasUpdate: false
    property bool isInstalling: false
    property double installProgress: 0.0

    signal installClicked()
    signal uninstallClicked()
    signal updateClicked()
    signal cardClicked()

    width: Dimensions.cardWidth + Dimensions.patchCardExtraWidth   // 150
    height: Dimensions.cardHeight + Dimensions.patchCardExtraHeight // 230

    property bool isHovered: cardMouse.containsMouse

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
            implicitWidth: badgeText.implicitWidth + 10
            implicitHeight: 18
            radius: 9
            color: root.hasUpdate && root.packageInstalled
                ? Theme.withAlpha(Theme.warning, 0.85)
                : root.packageInstalled
                    ? Theme.withAlpha(Theme.success, 0.85)
                    : Theme.withAlpha(Theme.textPrimary, 0.55)
            z: 2

            Label {
                id: badgeText
                anchors.centerIn: parent
                text: root.hasUpdate && root.packageInstalled
                    ? qsTr("Güncelleme")
                    : root.packageInstalled ? qsTr("Kurulu") : qsTr("Kurulu Değil")
                font.pixelSize: Dimensions.fontMicro
                font.weight: Font.Bold
                color: Theme.textOnColor
            }
        }

        // ===== HOVER OVERLAY =====
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.withAlpha(Theme.bgPrimary, 0.65)
            opacity: root.isHovered && !root.isInstalling ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                // Gradient icon circle
                Canvas {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 38; Layout.preferredHeight: 38
                    property bool installed: root.packageInstalled
                    property real phase: imgContainer.borderPhase
                    onPhaseChanged: if (root.isHovered) requestPaint()
                    onInstalledChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var colors = Theme.brandGradient
                        var cx = width / 2, cy = height / 2, r = 17

                        // Rotating gradient
                        var angle = phase * Math.PI * 2
                        var len = r * 1.4
                        var grad = ctx.createLinearGradient(
                            cx + Math.cos(angle) * len, cy + Math.sin(angle) * len,
                            cx - Math.cos(angle) * len, cy - Math.sin(angle) * len)
                        for (var i = 0; i < colors.length; i++)
                            grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                        // Circle background
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, Math.PI * 2)
                        ctx.fillStyle = Theme.withAlpha(Theme.bgPrimary, 0.50)
                        ctx.fill()

                        // Circle border with gradient
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, Math.PI * 2)
                        ctx.strokeStyle = grad
                        ctx.lineWidth = 1.5
                        ctx.stroke()

                        // Icon with gradient stroke
                        ctx.strokeStyle = grad
                        ctx.lineWidth = 2
                        ctx.lineCap = "round"
                        if (root.hasUpdate && installed) {
                            // Refresh circular arrow
                            ctx.beginPath()
                            ctx.arc(cx, cy, 6, -Math.PI * 0.7, Math.PI * 0.5)
                            ctx.stroke()
                            // Arrowhead
                            var ax = cx + 6 * Math.cos(Math.PI * 0.5)
                            var ay = cy + 6 * Math.sin(Math.PI * 0.5)
                            ctx.beginPath(); ctx.moveTo(ax - 3, ay - 3); ctx.lineTo(ax, ay); ctx.lineTo(ax + 3, ay - 3); ctx.stroke()
                        } else if (!installed) {
                            // Download arrow
                            ctx.beginPath(); ctx.moveTo(cx, cy - 7); ctx.lineTo(cx, cy + 3); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(cx - 4, cy); ctx.lineTo(cx, cy + 4); ctx.lineTo(cx + 4, cy); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(cx - 5, cy + 7); ctx.lineTo(cx + 5, cy + 7); ctx.stroke()
                        } else {
                            // X mark
                            ctx.beginPath(); ctx.moveTo(cx - 4, cy - 4); ctx.lineTo(cx + 4, cy + 4); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(cx + 4, cy - 4); ctx.lineTo(cx - 4, cy + 4); ctx.stroke()
                        }
                    }
                }

                // Gradient text label
                Canvas {
                    id: actionLabel
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: actionMetrics.width + 2
                    Layout.preferredHeight: actionMetrics.height + 2
                    property string actionText: root.hasUpdate && root.packageInstalled
                        ? qsTr("Güncelle")
                        : root.packageInstalled ? qsTr("Yamayı Kaldır") : qsTr("Yama Kur")
                    property real phase: imgContainer.borderPhase
                    onPhaseChanged: if (root.isHovered) requestPaint()
                    onActionTextChanged: requestPaint()

                    TextMetrics {
                        id: actionMetrics
                        text: actionLabel.actionText
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.DemiBold
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var colors = Theme.brandGradient

                        var angle = phase * Math.PI * 2
                        var len = width * 0.6
                        var cx = width / 2, cy = height / 2
                        var grad = ctx.createLinearGradient(
                            cx + Math.cos(angle) * len, cy + Math.sin(angle) * len,
                            cx - Math.cos(angle) * len, cy - Math.sin(angle) * len)
                        for (var i = 0; i < colors.length; i++)
                            grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                        ctx.fillStyle = grad
                        ctx.font = Font.DemiBold + " " + Dimensions.fontXS + "px sans-serif"
                        ctx.textAlign = "center"
                        ctx.textBaseline = "middle"
                        ctx.fillText(actionText, width / 2, height / 2)
                    }
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
                if (root.hasUpdate && root.packageInstalled)
                    root.updateClicked()
                else if (!root.packageInstalled)
                    root.installClicked()
                else
                    root.uninstallClicked()
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
    Accessible.name: root.gameName + (root.hasUpdate && root.packageInstalled ? " — " + qsTr("Güncelleme mevcut") : root.packageInstalled ? " — " + qsTr("Kurulu") : " — " + qsTr("Çevir"))
    Accessible.description: root.isInstalling
        ? qsTr("Installing, %1 percent").arg(Math.round(root.installProgress * 100))
        : ""
    activeFocusOnTab: true
    Keys.onReturnPressed: cardMouse.clicked(null)
    Keys.onSpacePressed: cardMouse.clicked(null)
}
