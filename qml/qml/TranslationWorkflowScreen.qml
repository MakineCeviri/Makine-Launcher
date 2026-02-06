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

                // Geri butonu
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Dimensions.radiusStandard
                    color: backBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                    Label {
                        anchors.centerIn: parent
                        text: "\u2190"  // Left arrow
                        font.pixelSize: 20
                        color: backBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
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
                        text: root.gameName || "Oyun Adı"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: root.gameEngine + " motoru"
                        font.pixelSize: 13
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
                            font.pixelSize: 14
                            color: TranslationService.phase === 6 ? Theme.success :
                                   TranslationService.phase === 7 ? Theme.error : Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: getPhaseStatusText()
                            font.pixelSize: 12
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
                                    font.pixelSize: 20
                                    color: Theme.primary
                                }
                            }

                            Label {
                                text: "İlerleme"
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: Math.round(TranslationService.progress * 100) + "%"
                                font.pixelSize: 24
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
                                    font.pixelSize: 20
                                    color: Theme.success
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: TranslationService.statusMessage || "Hazır"
                                    font.pixelSize: 14
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

                            // Iptal butonu
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                color: cancelBtnMouse.containsMouse ? Theme.withAlpha(Theme.error, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                                border.color: cancelBtnMouse.containsMouse ? Theme.error : Qt.rgba(1, 1, 1, 0.1)
                                border.width: 1
                                visible: TranslationService.isProcessing

                                Label {
                                    anchors.centerIn: parent
                                    text: "İptal Et"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: cancelBtnMouse.containsMouse ? Theme.error : Theme.textSecondary
                                }

                                MouseArea {
                                    id: cancelBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: TranslationService.stopTranslation()
                                }
                            }

                            // Bitir butonu (tamamlandiysa)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: Dimensions.radiusStandard
                                visible: TranslationService.phase === 6

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Theme.gold }
                                    GradientStop { position: 1.0; color: Theme.olive }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: "Tamam"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: "white"
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
                                    font.pixelSize: 20
                                }
                            }

                            Label {
                                text: "Aktivite"
                                font.pixelSize: 18
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
                                        font.pixelSize: 12
                                        color: Theme.textSecondary
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: model.time
                                        font.pixelSize: 10
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
            { name: "Tespit", icon: "\uD83D\uDD0D" },
            { name: "Çıkarma", icon: "\uD83D\uDCE4" },
            { name: "Eşleştirme", icon: "\uD83D\uDD17" },
            { name: "İnceleme", icon: "\uD83D\uDC41" },
            { name: "Uygulama", icon: "\u2699" },
            { name: "Bitti", icon: "\u2713" }
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
                            border.color: Qt.rgba(Theme.gold.r, Theme.gold.g, Theme.gold.b, 0.3)
                            border.width: 3

                            SequentialAnimation on scale {
                                running: parent.visible
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.15; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                            }

                            SequentialAnimation on opacity {
                                running: parent.visible
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
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
                            border.color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.25)
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
                                ColorAnimation { duration: 300 }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: 300 }
                            }

                            Label {
                                anchors.centerIn: parent
                                text: phaseItem.isCompleted ? "\u2713" : modelData.icon
                                font.pixelSize: 16
                                font.weight: phaseItem.isCompleted ? Font.Bold : Font.Normal
                                color: phaseItem.isCompleted || phaseItem.isFinal ? Theme.success :
                                       phaseItem.isActive ? Theme.gold : Theme.textMuted

                                Behavior on color {
                                    ColorAnimation { duration: 300 }
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
                        font.pixelSize: 11
                        font.weight: index + 1 === phaseIndicator.currentPhase ? Font.DemiBold : Font.Normal
                        color: index + 1 < phaseIndicator.currentPhase ? Theme.success :
                               index + 1 === phaseIndicator.currentPhase ? Theme.gold :
                               Theme.textMuted

                        Behavior on color {
                            ColorAnimation { duration: 300 }
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
                                GradientStop { position: 1.0; color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.6) }
                            }

                            Behavior on width {
                                NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
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
        title: "Çeviriyi İptal Et?"

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
                text: "Devam eden çeviri işlemi iptal edilecek.\nEmin misiniz?"
                font.pixelSize: 14
                color: Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    text: "Vazgeç"
                    onClicked: cancelConfirmDialog.close()

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                        radius: Dimensions.radiusStandard
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 13
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "İptal Et"
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
                        font.pixelSize: 13
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
        case 0: return "Hazır"
        case 1: return "Tespit Ediliyor"
        case 2: return "Çıkarılıyor"
        case 3: return "Eşleştiriliyor"
        case 4: return "İnceleniyor"
        case 5: return "Uygulanıyor"
        case 6: return "Tamamlandı"
        case 7: return "Hata"
        default: return "Bilinmiyor"
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
            addActivity("QA sorunları yoksayıldı, devam ediliyor...", "warning")
        }

        onFixIssues: {
            addActivity("QA sorunları inceleniyor...", "info")
        }
    }

    // ===== CONNECTIONS =====
    Connections {
        target: TranslationService

        function onPhaseChanged() {
            var phaseNames = ["Hazır", "Tespit", "Çıkarma", "Eşleştirme", "İnceleme", "Uygulama", "Tamamlandı", "Hata"]
            addActivity("Aşama: " + phaseNames[TranslationService.phase], "info")
        }

        function onStatusMessageChanged() {
            if (TranslationService.statusMessage) {
                addActivity(TranslationService.statusMessage, "info")
            }
        }

        function onTranslationCompleted(gameId) {
            addActivity("Çeviri başarıyla tamamlandı!", "success")
        }

        function onTranslationError(gameId, error) {
            addActivity("Hata: " + error, "error")
        }

        function onQaCompleted(passed, failed, avgScore, issues) {
            addActivity("QA tamamlandı: " + passed + " geçti, " + failed + " başarısız (Skor: " + avgScore + ")",
                       failed > 0 ? "warning" : "success")

            // Show dialog if there are issues
            if (issues && issues.length > 0) {
                // Convert C++ issues to dialog format
                var dialogIssues = issues.map(function(issue) {
                    var severityNum = issue.severity === "critical" ? 4 :
                                      (issue.severity === "warning" ? 2 : 1)
                    return {
                        code: "QA_" + severityNum,
                        message: issue.sourceText.substring(0, 50) + (issue.sourceText.length > 50 ? "..." : ""),
                        severity: severityNum,
                        penaltyPoints: 100 - issue.score
                    }
                })

                qaResultsDialog.score = avgScore
                qaResultsDialog.passed = (failed === 0)
                qaResultsDialog.hasCriticalIssues = issues.some(function(i) { return i.severity === "critical" })
                qaResultsDialog.issues = dialogIssues
                qaResultsDialog.open()
            }
        }

        function onTmMatchFound(source, target, similarity) {
            var percent = Math.round(similarity * 100)
            if (percent >= 90) {
                addActivity("TM eşleşme (%100): " + source.substring(0, 30) + "...", "success")
            } else if (percent >= 70) {
                addActivity("TM eşleşme (%" + percent + "): " + source.substring(0, 30) + "...", "info")
            }
        }

        function onMatchingCompleted(matched, total) {
            addActivity("Eşleştirme tamamlandı: " + matched + "/" + total + " çeviri bulundu", "success")
        }
    }

    // Start translation when screen loads (if game info is provided)
    Component.onCompleted: {
        if (root.gameId && root.gamePath && root.gameEngine) {
            addActivity("Çeviri başlatılıyor: " + root.gameName, "info")
            TranslationService.startTranslation(root.gameId, root.gameName, root.gamePath)
        }
    }
}
