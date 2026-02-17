import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
import "../components"

Item {
    id: homePage

    // Properties propagated from HomeScreen
    property bool animationsEnabled: true
    property real contentMargin: 16
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200
    property real layoutTopRowGap: 20
    property real layoutGamesSectionGap: 8
    property real layoutCardGap: 32
    property real layoutSepTopMargin: 4
    property real layoutSepBottomMargin: 8

    // Notification state
    property string notificationMessage: ""
    property string notificationType: "info"

    // Animation trigger - increment to replay all entry animations
    property int animationTrigger: 0

    // Signals
    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    signal settingsRequested()
    signal hideNotificationRequested()
    signal openAllGamesRequested()

    function replayEntryAnimations() {
        // Reset and replay top row animation
        topRowLayout.opacity = 0
        topRowTranslate.y = 14
        topRowEntryAnim.restart()

        // Reset and replay games section animation
        gamesSectionLayout.opacity = 0
        gamesSectionTranslate.y = 18
        gamesSectionEntryAnim.restart()

        // Trigger game cards to replay their animations
        animationTrigger++
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: homePage.contentMargin
        spacing: Dimensions.marginMD

            // ===== NOTIFICATION BANNER =====
            NotificationBanner {
                id: notificationBanner
                Layout.leftMargin: homePage.contentMargin
                Layout.rightMargin: homePage.contentMargin
                notificationMessage: homePage.notificationMessage
                notificationType: homePage.notificationType
                onSettingsRequested: homePage.settingsRequested()
                onDismissRequested: homePage.hideNotificationRequested()
            }

            // ===== UPDATE STATUS PILL =====
            Row {
                Layout.leftMargin: homePage.contentMargin
                Layout.rightMargin: homePage.contentMargin
                spacing: 6
                visible: UpdateChecker.statusType === "upToDate" || UpdateChecker.statusType === "updateAvailable"

                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.success
                }

                Text {
                    text: UpdateChecker.statusType === "updateAvailable"
                          ? qsTr("%1 mevcut").arg(UpdateChecker.latestVersion)
                          : qsTr("G\u00FCncel")
                    font.pixelSize: Dimensions.fontXS
                    color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.textMuted

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: UpdateChecker.statusType === "updateAvailable" ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (UpdateChecker.statusType === "updateAvailable")
                                homePage.settingsRequested()
                        }
                    }
                }
            }

            // ===== TOP ROW =====
            RowLayout {
                id: topRowLayout
                Layout.fillWidth: true
                Layout.preferredHeight: homePage.layoutTopRowHeight
                Layout.maximumHeight: homePage.layoutTopRowHeight
                Layout.leftMargin: homePage.contentMargin
                Layout.rightMargin: homePage.contentMargin
                spacing: homePage.layoutTopRowGap

                opacity: 0
                transform: Translate { id: topRowTranslate; y: 14 }
                Component.onCompleted: topRowEntryAnim.start()

                ParallelAnimation {
                    id: topRowEntryAnim
                    NumberAnimation {
                        target: topRowLayout
                        property: "opacity"
                        from: 0; to: 1
                        duration: Dimensions.animSlow
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: topRowTranslate
                        property: "y"
                        from: 14; to: 0
                        duration: Dimensions.animSlow
                        easing.type: Easing.OutCubic
                    }
                }

                // ============================================================
                // GAME STATUS CARD
                // ============================================================
                GameStatusCard {
                    animationsEnabled: homePage.animationsEnabled
                    layoutCardMargin: homePage.layoutCardMargin
                    layoutCardSpacing: homePage.layoutCardSpacing
                    layoutTopRowHeight: homePage.layoutTopRowHeight
                    onManualFolderRequested: homePage.manualFolderRequested()
                }

                // ============================================================
                // ANNOUNCEMENT CARD
                // ============================================================
                AnnouncementCard {
                    layoutCardMargin: homePage.layoutCardMargin
                    layoutCardSpacing: homePage.layoutCardSpacing
                    layoutTopRowHeight: homePage.layoutTopRowHeight
                }
            }

            // ===== BATCH OPERATIONS PANEL =====
            BatchOperationsPanel {
                Layout.fillWidth: true
                Layout.leftMargin: homePage.contentMargin
                Layout.rightMargin: homePage.contentMargin
                animationsEnabled: homePage.animationsEnabled
            }

            // ===== GAMES SECTION =====
            ColumnLayout {
                id: gamesSectionLayout
                Layout.fillWidth: true
                Layout.leftMargin: homePage.contentMargin
                Layout.rightMargin: homePage.contentMargin
                spacing: homePage.layoutGamesSectionGap

                opacity: 0
                transform: Translate { id: gamesSectionTranslate; y: 18 }
                Component.onCompleted: gamesSectionEntryAnim.start()

                SequentialAnimation {
                    id: gamesSectionEntryAnim
                    PauseAnimation { duration: Dimensions.transitionDuration }
                    ParallelAnimation {
                        NumberAnimation {
                            target: gamesSectionLayout
                            property: "opacity"
                            from: 0; to: 1
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: gamesSectionTranslate
                            property: "y"
                            from: 18; to: 0
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                RowLayout {
                    spacing: Dimensions.spacingLG

                    Label {
                        text: qsTr("T\u00FCrk\u00E7e Yama K\u00FCt\u00FCphanesi")
                        font.pixelSize: Dimensions.fontXL
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    BusyIndicator {
                        visible: GameService.isScanning
                        running: GameService.isScanning
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }

                    Item { Layout.fillWidth: true }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: homePage.layoutSepTopMargin
                    Layout.bottomMargin: homePage.layoutSepBottomMargin
                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                }

                Flow {
                    id: skeletonFlow
                    Layout.fillWidth: true
                    spacing: Dimensions.cardGap
                    visible: GameService.supportedGameCount === 0 && GameService.isScanning

                    Repeater {
                        model: 7

                        GameCardSkeleton {
                            animationsEnabled: homePage.animationsEnabled
                            animationDelay: index * 100
                        }
                    }
                }

                Row {
                    id: gamesRow
                    Layout.alignment: Qt.AlignHCenter
                    spacing: homePage.layoutCardGap
                    visible: !skeletonFlow.visible

                    readonly property int availableWidth: gamesSectionLayout.width
                    readonly property int cardTotal: Dimensions.cardWidth + homePage.layoutCardGap
                    readonly property int maxCards: Math.max(1, Math.floor((availableWidth - Dimensions.cardWidth) / cardTotal))

                    Repeater {
                        id: gamesRepeater
                        model: GameService.supportedGames.slice(0, gamesRow.maxCards)

                        GameCard {
                            id: gameCardDelegate
                            required property var modelData
                            required property int index
                            gameName: modelData.name || ""
                            imageUrl: modelData.headerImageUrl || ""
                            verified: modelData.isVerified || false
                            translated: modelData.hasTranslation || false
                            gameId: modelData.steamAppId || modelData.id || ""
                            installPath: modelData.installPath || ""
                            steamAppId: modelData.steamAppId || ""

                            // Staggered fade-in (no transform - fixes click issues)
                            opacity: 0
                            Component.onCompleted: entryAnimation.start()

                            // Re-trigger animation when animationTrigger changes
                            Connections {
                                target: homePage
                                function onAnimationTriggerChanged() {
                                    gameCardDelegate.opacity = 0
                                    entryAnimation.restart()
                                }
                            }

                            SequentialAnimation {
                                id: entryAnimation
                                PauseAnimation { duration: index * 40 }
                                NumberAnimation {
                                    target: gameCardDelegate
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: Dimensions.fadeTransitionDuration
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onClicked: {
                                if ((modelData.isInstalled || false) && (modelData.hasTranslation || false)) {
                                    homePage.installAndShowDetail(
                                        modelData.id || "",
                                        modelData.name || "",
                                        modelData.installPath || "",
                                        modelData.engine || ""
                                    )
                                } else {
                                    homePage.gameSelected(
                                        modelData.id || "",
                                        modelData.name || "",
                                        modelData.installPath || "",
                                        modelData.engine || ""
                                    )
                                }
                            }
                        }
                    }

                    ViewAllCard {
                        id: viewAllCardItem
                        remainingCount: Math.max(0, GameService.supportedGameCount - gamesRepeater.count)

                        onClicked: homePage.openAllGamesRequested()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    radius: Dimensions.radiusStandard
                    color: Theme.withAlpha(Theme.textPrimary, 0.03)
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                    border.width: 1
                    visible: GameService.supportedGameCount === 0 && !GameService.isScanning

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingLG

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "\uD83C\uDFAE"
                            font.pixelSize: Dimensions.fontBanner
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("\u00C7eviri paketi bulunamad\u0131")
                            font.pixelSize: Dimensions.fontMD
                            color: Theme.textSecondary
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("translation_data klas\u00F6r\u00FCn\u00FC kontrol edin")
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.textMuted
                        }
                    }
                }
            }

        Item { Layout.fillHeight: true }
    }

    // ===== VIEW ALL CARD - Modern, GameCard ile uyumlu =====
    component ViewAllCard: Item {
        id: viewAllRoot
        property int remainingCount: 0

        signal clicked()

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("View All (+%1)").arg(remainingCount)
        Accessible.onPressAction: clicked()
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: viewAllMouse.containsMouse

        transform: [
            Translate { y: viewAllRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: viewAllRoot.width / 2; origin.y: viewAllRoot.height / 2
                xScale: viewAllRoot.isHovered ? 1.02 : 1.0; yScale: viewAllRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            }
        ]

        AnimatedGradientGlow {
            anchors.centerIn: viewAllContent
            width: viewAllContent.width + 40
            height: viewAllContent.height + 40
            active: viewAllRoot.isHovered
            animationsEnabled: homePage.animationsEnabled
            z: -2
        }

        Rectangle {
            id: viewAllContent
            anchors.fill: parent
            radius: 14
            color: Theme.bgPrimary

            property real borderPhase: 0
            NumberAnimation on borderPhase {
                from: 0; to: 1
                duration: 8000
                loops: Animation.Infinite
                running: viewAllRoot.visible && homePage.animationsEnabled
            }

            Canvas {
                anchors.fill: parent
                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (viewAllRoot.isHovered) requestPaint()

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

                    var r = 14
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

                    ctx.strokeStyle = grad
                    ctx.lineWidth = bw
                    ctx.globalAlpha = viewAllRoot.isHovered ? 0.8 : 0.4
                    ctx.stroke()
                }

                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
            }

            Canvas {
                id: viewAllCanvas
                anchors.fill: parent

                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (hov) requestPaint()
                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
                property int count: viewAllRoot.remainingCount
                onCountChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width / 2;
                    var angle = phase * Math.PI * 2;
                    var len = width * 0.6;
                    var x1 = cx + Math.cos(angle) * len;
                    var x2 = cx - Math.cos(angle) * len;
                    var colors = Theme.brandGradient;

                    var numGrad = ctx.createLinearGradient(x1, 0, x2, 0);
                    for (var i = 0; i < colors.length; i++)
                        numGrad.addColorStop(i / Math.max(1, colors.length - 1), colors[i]);

                    // Sideways "+N" number - centered
                    ctx.save();
                    ctx.translate(cx + 5, height / 2 - 4);
                    ctx.rotate(-Math.PI / 2);
                    ctx.font = "bold 46px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    ctx.globalAlpha = hov ? 1.0 : 0.6;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.5)";
                    ctx.fillText("+" + count, 0, 0);
                    ctx.restore();

                    // Gradient separator line - exact center
                    var lineY = height - 34;
                    var lineW = 36;
                    var lineX = Math.round(cx - lineW / 2);
                    ctx.globalAlpha = hov ? 0.5 : 0.15;
                    ctx.fillStyle = numGrad;
                    ctx.fillRect(lineX, lineY, lineW, 1.5);

                    // "Tumunu Gor" - exact center
                    ctx.font = "500 11px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "top";
                    ctx.globalAlpha = hov ? 0.9 : 0.4;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.4)";
                    var label = hov ? qsTr("T\u00FCm\u00FCn\u00FC G\u00F6r") + " \u2192" : qsTr("T\u00FCm\u00FCn\u00FC G\u00F6r");
                    ctx.fillText(label, Math.round(cx), lineY + 6);
                }
            }
        }

        // Focus indicator
        Rectangle {
            anchors.fill: viewAllContent
            anchors.margins: -2
            radius: viewAllContent.radius + 2
            color: "transparent"
            border.color: Theme.withAlpha(Theme.primary, 0.6)
            border.width: 2
            visible: viewAllRoot.activeFocus
            z: Dimensions.zBase
        }

        MouseArea {
            id: viewAllMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewAllRoot.clicked()
        }
    }
}
