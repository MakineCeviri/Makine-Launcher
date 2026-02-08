import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "dialogs"
import "components"

/**
 * TranslationWorkflowScreen.qml - Çeviri Akışı Ekranı
 *
 * Tek tuşla çeviri:
 * - Tespit → Çıkarma → Eşleştirme → İnceleme → Uygulama → Tamamlandı
 *
 * Kullanıcı sadece izler, gerekirse iptal eder.
 */
Item {
    id: root

    // Game info passed from detail screen
    property string gameId: ""
    property string gameName: ""
    property string gamePath: ""
    property string gameEngine: ""
    property string headerImageUrl: ""

    // Signals
    signal backClicked()
    signal completed(string gameId)
    signal cancelled(string gameId)

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Dimensions.marginXXL
            spacing: Dimensions.marginLG

            // ===== HEADER - Oyun bilgisi ve geri butonu =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // Back button
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Dimensions.radiusStandard
                    color: backBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Geri")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { if (TranslationService.isActive) cancelConfirmDialog.open(); else root.backClicked() }
                    Keys.onSpacePressed: { if (TranslationService.isActive) cancelConfirmDialog.open(); else root.backClicked() }

                    Label {
                        anchors.centerIn: parent
                        text: "\u2190"  // Left arrow
                        font.pixelSize: Dimensions.fontXL
                        color: backBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: backBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (TranslationService.isActive) {
                                // Show confirmation dialog
                                cancelConfirmDialog.open()
                            } else {
                                root.backClicked()
                            }
                        }
                    }
                }

                // Oyun bilgisi
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: root.gameName || qsTr("Oyun Adı")
                        font.pixelSize: Dimensions.headlineLarge
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: qsTr("%1 motoru").arg(root.gameEngine)
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                    }
                }

                // Durum rozeti
                Rectangle {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: statusRow.width + 24
                    radius: Dimensions.radiusStandard
                    color: TranslationService.phase === 6 ?
                        Theme.withAlpha(Theme.success, 0.15) :
                        TranslationService.phase === 7 ?
                            Theme.withAlpha(Theme.error, 0.15) :
                            Theme.withAlpha(Theme.primary, 0.15)

                    Row {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 8

                        Label {
                            text: TranslationService.phase === 6 ? "\u2713" :
                                  TranslationService.phase === 7 ? "\u2717" : "\u23F3"
                            font.pixelSize: Dimensions.fontMD
                            color: TranslationService.phase === 6 ? Theme.success :
                                   TranslationService.phase === 7 ? Theme.error : Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: getPhaseStatusText()
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.DemiBold
                            color: TranslationService.phase === 6 ? Theme.success :
                                   TranslationService.phase === 7 ? Theme.error : Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ===== PHASE INDICATOR - Adim gostergesi =====
            PhaseIndicator {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                currentPhase: TranslationService.phase
            }

            // ===== MAIN CONTENT - Progress ve aktivite =====
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Dimensions.marginLG

                // Sol: Progress karti
                GlassCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginLG
                        spacing: 20

                        // Progress header
                        RowLayout {
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.primary, 0.15)

                                Label {
                                    anchors.centerIn: parent
                                    text: "\u23F3"
                                    font.pixelSize: Dimensions.fontXL
                                    color: Theme.primary
                                }
                            }

                            Label {
                                text: qsTr("İlerleme")
                                font.pixelSize: Dimensions.fontTitle
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: Math.round(TranslationService.progress * 100) + "%"
                                font.pixelSize: Dimensions.headlineLarge
                                font.weight: Font.Bold
                                color: Theme.gold
                            }
                        }

                        // Premium progress bar with glow and shimmer
                        TranslationProgressBar {
                            Layout.fillWidth: true
                            value: TranslationService.progress
                            animationsEnabled: TranslationService.isProcessing
                            barHeight: 14
                        }

                        // Status mesaji
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            radius: Dimensions.radiusStandard
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 12

                                // Animated spinner
                                Item {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    visible: TranslationService.isProcessing

                                    Rectangle {
                                        id: spinner
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        radius: 10
                                        color: "transparent"
                                        border.color: Theme.gold
                                        border.width: 2
                                        opacity: 0.3
                                    }

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        radius: 10
                                        color: "transparent"
                                        border.color: Theme.gold
                                        border.width: 2

                                        Rectangle {
                                            width: 10
                                            height: 10
                                            color: Theme.bgPrimary
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                        }

                                        RotationAnimation on rotation {
                                            from: 0
                                            to: 360
                                            duration: 1000
                                            loops: Animation.Infinite
                                            running: TranslationService.isProcessing
                                        }
                                    }
                                }

                                // Completed icon
                                Label {
                                    visible: TranslationService.phase === 6
                                    text: "\u2713"
                                    font.pixelSize: Dimensions.fontXL
                                    color: Theme.success
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: TranslationService.statusMessage || qsTr("Hazır")
                                    font.pixelSize: Dimensions.fontMD
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        // Butonlar
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            // Cancel button (visible during processing)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                color: cancelBtnMouse.containsMouse ? Theme.withAlpha(Theme.error, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                                border.color: cancelBtnMouse.containsMouse ? Theme.error : Qt.rgba(1, 1, 1, 0.1)
                                border.width: 1
                                visible: TranslationService.isProcessing
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("İptal Et")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: TranslationService.stopTranslation()
                                Keys.onSpacePressed: TranslationService.stopTranslation()

                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("İptal Et")
                                    font.pixelSize: Dimensions.fontMD
                                    font.weight: Font.Medium
                                    color: cancelBtnMouse.containsMouse ? Theme.error : Theme.textSecondary
                                }

                                // Focus indicator
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: parent.radius + 1
                                    color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                                    border.width: 2
                                    visible: parent.activeFocus
                                }

                                MouseArea {
                                    id: cancelBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: TranslationService.stopTranslation()
                                }
                            }

                            // Retry button (visible on error)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                visible: TranslationService.phase === 7
                                color: retryBtnMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                                border.color: retryBtnMouse.containsMouse ? Theme.warning : Qt.rgba(1, 1, 1, 0.1)
                                border.width: 1
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Tekrar Dene")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: { addActivity(qsTr("Çeviri yeniden başlatılıyor..."), "info"); TranslationService.startTranslation(root.gameId, root.gameName, root.gamePath) }
                                Keys.onSpacePressed: { addActivity(qsTr("Çeviri yeniden başlatılıyor..."), "info"); TranslationService.startTranslation(root.gameId, root.gameName, root.gamePath) }

                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Tekrar Dene")
                                    font.pixelSize: Dimensions.fontMD
                                    font.weight: Font.Medium
                                    color: retryBtnMouse.containsMouse ? Theme.warning : Theme.textSecondary
                                }

                                // Focus indicator
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: parent.radius + 1
                                    color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                                    border.width: 2
                                    visible: parent.activeFocus
                                }

                                MouseArea {
                                    id: retryBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        addActivity(qsTr("Çeviri yeniden başlatılıyor..."), "info")
                                        TranslationService.startTranslation(root.gameId, root.gameName, root.gamePath)
                                    }
                                }
                            }

                            // Back button (visible on error)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                visible: TranslationService.phase === 7
                                color: errorBackMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                                border.color: Qt.rgba(1, 1, 1, 0.1)
                                border.width: 1
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Geri Dön")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: root.backClicked()
                                Keys.onSpacePressed: root.backClicked()

                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Geri Dön")
                                    font.pixelSize: Dimensions.fontMD
                                    font.weight: Font.Medium
                                    color: Theme.textSecondary
                                }

                                // Focus indicator
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: parent.radius + 1
                                    color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                                    border.width: 2
                                    visible: parent.activeFocus
                                }

                                MouseArea {
                                    id: errorBackMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.backClicked()
                                }
                            }

                            // Complete button (visible when done)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                visible: TranslationService.phase === 6
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Tamam")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: root.completed(root.gameId)
                                Keys.onSpacePressed: root.completed(root.gameId)

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Theme.gold }
                                    GradientStop { position: 1.0; color: Theme.olive }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Tamam")
                                    font.pixelSize: Dimensions.fontMD
                                    font.weight: Font.DemiBold
                                    color: "white"
                                }

                                // Focus indicator
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    radius: parent.radius + 2
                                    color: "transparent"
                                    border.color: Theme.withAlpha(Theme.gold, 0.6)
                                    border.width: 2
                                    visible: parent.activeFocus
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.completed(root.gameId)
                                }
                            }
                        }
                    }
                }

                // Sag: Aktivite karti
                GlassCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginLG
                        spacing: 16

                        // Activity header
                        RowLayout {
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.accent, 0.15)

                                Label {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDCDD"  // Memo
                                    font.pixelSize: Dimensions.fontXL
                                }
                            }

                            Label {
                                text: qsTr("Aktivite")
                                font.pixelSize: Dimensions.fontTitle
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                        }

                        // Activity log
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Dimensions.radiusStandard
                            color: Qt.rgba(0, 0, 0, 0.2)

                            // Empty state
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Henüz aktivite yok")
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textMuted
                                visible: activityModel.count === 0
                            }

                            ListView {
                                id: activityList
                                anchors.fill: parent
                                anchors.margins: 12
                                clip: true
                                spacing: 8

                                model: ListModel {
                                    id: activityModel
                                }

                                delegate: RowLayout {
                                    width: activityList.width
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 6
                                        Layout.preferredHeight: 6
                                        radius: 3
                                        color: model.type === "success" ? Theme.success :
                                               model.type === "error" ? Theme.error :
                                               model.type === "warning" ? Theme.warning : Theme.primary
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: model.message
                                        font.pixelSize: Dimensions.fontSM
                                        color: Theme.textSecondary
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: model.time
                                        font.pixelSize: Dimensions.fontCaption
                                        color: Theme.textMuted
                                    }
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== PHASE INDICATOR COMPONENT - Premium version =====
    component PhaseIndicator: Rectangle {
        id: phaseIndicator
        property int currentPhase: 0

        color: Qt.rgba(1, 1, 1, 0.03)
        radius: Dimensions.radiusStandard
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        // Subtle gradient overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.02) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        readonly property var phases: [
            { name: qsTr("Tespit"), icon: "\uD83D\uDD0D" },
            { name: qsTr("Çıkarma"), icon: "\uD83D\uDCE4" },
            { name: qsTr("Eşleştirme"), icon: "\uD83D\uDD17" },
            { name: qsTr("İnceleme"), icon: "\uD83D\uDC41" },
            { name: qsTr("Uygulama"), icon: "\u2699" },
            { name: qsTr("Bitti"), icon: "\u2713" }
        ]

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 0

            Repeater {
                model: phaseIndicator.phases

                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Phase circle with glow
                    Item {
                        id: phaseItem
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40

                        property bool isActive: index + 1 === phaseIndicator.currentPhase
                        property bool isCompleted: index + 1 < phaseIndicator.currentPhase
                        property bool isFinal: index === 5 && phaseIndicator.currentPhase === 6

                        // Glow layer for active state
                        Rectangle {
                            anchors.centerIn: parent
                            width: 44
                            height: 44
                            radius: 22
                            visible: parent.isActive
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.gold, 0.3)
                            border.width: 3

                            SequentialAnimation on scale {
                                running: parent.visible
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.15; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                            }

                            SequentialAnimation on opacity {
                                running: parent.visible
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                            }
                        }

                        // Success glow for completed
                        Rectangle {
                            anchors.centerIn: parent
                            width: 42
                            height: 42
                            radius: 21
                            visible: parent.isCompleted || parent.isFinal
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.success, 0.25)
                            border.width: 2
                        }

                        // Main circle
                        Rectangle {
                            id: phaseCircle
                            anchors.centerIn: parent
                            width: 36
                            height: 36
                            radius: 18

                            color: parent.isCompleted || parent.isFinal ? Theme.withAlpha(Theme.success, 0.2) :
                                   parent.isActive ? Theme.withAlpha(Theme.gold, 0.2) :
                                   Qt.rgba(1, 1, 1, 0.05)

                            border.color: parent.isCompleted || parent.isFinal ? Theme.success :
                                          parent.isActive ? Theme.gold :
                                          Qt.rgba(1, 1, 1, 0.15)
                            border.width: parent.isActive ? 2 : 1

                            Behavior on color {
                                ColorAnimation { duration: Dimensions.fadeTransitionDuration }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: Dimensions.fadeTransitionDuration }
                            }

                            Label {
                                anchors.centerIn: parent
                                text: phaseItem.isCompleted ? "\u2713" : modelData.icon
                                font.pixelSize: Dimensions.fontLG
                                font.weight: phaseItem.isCompleted ? Font.Bold : Font.Normal
                                color: phaseItem.isCompleted || phaseItem.isFinal ? Theme.success :
                                       phaseItem.isActive ? Theme.gold : Theme.textMuted

                                Behavior on color {
                                    ColorAnimation { duration: Dimensions.fadeTransitionDuration }
                                }
                            }

                            // Pulse animation for active phase
                            SequentialAnimation on scale {
                                running: phaseItem.isActive
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.08; duration: 600; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
                            }
                        }
                    }

                    // Phase name with transition
                    Label {
                        text: modelData.name
                        font.pixelSize: Dimensions.fontXS
                        font.weight: index + 1 === phaseIndicator.currentPhase ? Font.DemiBold : Font.Normal
                        color: index + 1 < phaseIndicator.currentPhase ? Theme.success :
                               index + 1 === phaseIndicator.currentPhase ? Theme.gold :
                               Theme.textMuted

                        Behavior on color {
                            ColorAnimation { duration: Dimensions.fadeTransitionDuration }
                        }
                    }

                    // Animated connector line (except last)
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
                        visible: index < 5

                        // Background track
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 1
                            anchors.bottomMargin: 1
                            radius: 1
                            color: Qt.rgba(1, 1, 1, 0.1)
                        }

                        // Fill progress
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.topMargin: 1
                            anchors.bottomMargin: 1
                            width: index + 1 < phaseIndicator.currentPhase ? parent.width : 0
                            radius: 1

                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Theme.success }
                                GradientStop { position: 1.0; color: Theme.withAlpha(Theme.success, 0.6) }
                            }

                            Behavior on width {
                                NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== GLASS CARD COMPONENT =====
    component GlassCard: Rectangle {
        radius: Dimensions.radiusStandard
        color: Qt.rgba(1, 1, 1, 0.03)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    // ===== CANCEL CONFIRMATION DIALOG =====
    Dialog {
        id: cancelConfirmDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Çeviriyi İptal Et?")

        // Explicit size to prevent binding loop
        width: 320
        implicitWidth: 320

        background: Rectangle {
            color: Theme.surface
            radius: Dimensions.radiusStandard
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 16

            Label {
                text: qsTr("Devam eden çeviri işlemi iptal edilecek.\nEmin misiniz?")
                font.pixelSize: Dimensions.fontMD
                color: Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Vazgeç")
                    onClicked: cancelConfirmDialog.close()

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                        radius: Dimensions.radiusStandard
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("İptal Et")
                    onClicked: {
                        TranslationService.stopTranslation()
                        cancelConfirmDialog.close()
                        root.cancelled(root.gameId)
                    }

                    background: Rectangle {
                        color: parent.hovered ? Theme.error : Theme.withAlpha(Theme.error, 0.8)
                        radius: Dimensions.radiusStandard
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // ===== HELPER FUNCTIONS =====
    function getPhaseStatusText() {
        switch (TranslationService.phase) {
        case 0: return qsTr("Hazır")
        case 1: return qsTr("Tespit Ediliyor")
        case 2: return qsTr("Çıkarılıyor")
        case 3: return qsTr("Eşleştiriliyor")
        case 4: return qsTr("İnceleniyor")
        case 5: return qsTr("Uygulanıyor")
        case 6: return qsTr("Tamamlandı")
        case 7: return qsTr("Hata")
        default: return qsTr("Bilinmiyor")
        }
    }

    function addActivity(message, type) {
        var now = new Date()
        var timeStr = now.getHours().toString().padStart(2, '0') + ":" +
                      now.getMinutes().toString().padStart(2, '0')

        activityModel.insert(0, {
            message: message,
            type: type || "info",
            time: timeStr
        })

        // Keep only last 100 entries
        while (activityModel.count > 100) {
            activityModel.remove(activityModel.count - 1)
        }
    }

    // ===== QA RESULTS DIALOG =====
    QAResultsDialog {
        id: qaResultsDialog
        parent: Overlay.overlay

        onIgnoreAndContinue: {
            addActivity(qsTr("QA sorunları yoksayıldı, devam ediliyor..."), "warning")
        }

        onFixIssues: {
            addActivity(qsTr("QA sorunları inceleniyor..."), "info")
        }
    }

    // ===== CONNECTIONS =====
    Connections {
        target: TranslationService

        function onPhaseChanged() {
            var phaseNames = [qsTr("Hazır"), qsTr("Tespit"), qsTr("Çıkarma"), qsTr("Eşleştirme"), qsTr("İnceleme"), qsTr("Uygulama"), qsTr("Tamamlandı"), qsTr("Hata")]
            addActivity(qsTr("Aşama: %1").arg(phaseNames[TranslationService.phase]), "info")
        }

        function onStatusMessageChanged() {
            if (TranslationService.statusMessage) {
                addActivity(TranslationService.statusMessage, "info")
            }
        }

        function onTranslationCompleted(gameId) {
            addActivity(qsTr("Çeviri başarıyla tamamlandı!"), "success")
        }

        function onTranslationError(gameId, error) {
            addActivity(qsTr("Hata: %1").arg(error), "error")
        }

        function onQaCompleted(passed, failed, avgScore, issues) {
            addActivity(qsTr("QA tamamlandı: %1 geçti, %2 başarısız (Skor: %3)").arg(passed).arg(failed).arg(avgScore),
                       failed > 0 ? "warning" : "success")

            // Show dialog if there are issues
            if (issues && issues.length > 0) {
                // Map severity strings from C++ to numeric values
                var severityMap = {
                    "info": 1,
                    "warning": 2,
                    "major": 3,
                    "critical": 4
                }

                var dialogIssues = issues.map(function(issue) {
                    return {
                        code: issue.code || "QA_UNKNOWN",
                        message: issue.message || (issue.sourceText
                            ? issue.sourceText.substring(0, 50) + (issue.sourceText.length > 50 ? "..." : "")
                            : qsTr("Bilinmeyen sorun")),
                        severity: severityMap[issue.severity] || 1,
                        penaltyPoints: issue.penaltyPoints || 0
                    }
                })

                qaResultsDialog.score = avgScore
                qaResultsDialog.passed = (failed === 0)
                qaResultsDialog.hasCriticalIssues = dialogIssues.some(function(i) { return i.severity >= 4 })
                qaResultsDialog.issues = dialogIssues
                qaResultsDialog.open()
            }
        }

        function onTmMatchFound(source, target, similarity) {
            var percent = Math.round(similarity * 100)
            if (percent >= 90) {
                addActivity(qsTr("TM eşleşme (%%1): %2...").arg(100).arg(source.substring(0, 30)), "success")
            } else if (percent >= 70) {
                addActivity(qsTr("TM eşleşme (%%1): %2...").arg(percent).arg(source.substring(0, 30)), "info")
            }
        }

        function onMatchingCompleted(matched, total) {
            addActivity(qsTr("Eşleştirme tamamlandı: %1/%2 çeviri bulundu").arg(matched).arg(total), "success")
        }
    }

    // Start translation when screen loads (if game info is provided)
    Component.onCompleted: {
        if (root.gameId && root.gamePath && root.gameEngine) {
            addActivity(qsTr("Çeviri başlatılıyor: %1").arg(root.gameName), "info")
            TranslationService.startTranslation(root.gameId, root.gameName, root.gamePath)
        }
    }
}
