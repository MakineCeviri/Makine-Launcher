import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * OnboardingWizard.qml - First-launch experience
 *
 * 3-step wizard: Welcome → Library Scan → Ready
 * Emits wizardFinished() — parent handles settings persistence.
 */
Rectangle {
    id: root
    color: Theme.bgPrimary

    signal wizardFinished()

    property int currentStep: 0
    readonly property int totalSteps: 3
    readonly property bool isLastStep: currentStep === totalSteps - 1

    // Scan state
    property bool isScanning: false
    property bool scanDone: false
    property int foundGames: 0
    property int scanStage: 0  // 0=Steam, 1=Epic, 2=GOG, 3=done

    readonly property var scanStageTexts: [
        qsTr("Steam kütüphanesi aranıyor..."),
        qsTr("Epic Games aranıyor..."),
        qsTr("GOG aranıyor...")
    ]

    Connections {
        target: typeof GameService !== "undefined" ? GameService : null
        function onScanCompleted(count) {
            scanStageTimer.stop()
            root.isScanning = false
            root.scanDone = true
            root.foundGames = count
        }
    }

    // Timer to cycle through scan stages visually
    Timer {
        id: scanStageTimer
        interval: 1200
        repeat: true
        onTriggered: {
            if (root.scanStage < 2)
                root.scanStage++
        }
    }

    // ===== BACKGROUND =====

    // Subtle top gradient
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.03) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Top brand line
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.brandGold }
            GradientStop { position: 0.25; color: Theme.brandCoral }
            GradientStop { position: 0.5; color: Theme.brandPurple }
            GradientStop { position: 0.75; color: Theme.brandBlue }
            GradientStop { position: 1.0; color: Theme.brandGreen }
        }
    }

    // ===== MAIN CONTENT =====
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 48
        anchors.bottomMargin: 40
        anchors.leftMargin: 60
        anchors.rightMargin: 60
        spacing: 0

        // ===== STEP CONTENT =====
        StackLayout {
            id: stepStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentStep

            // ==========================================
            //  STEP 1: WELCOME
            // ==========================================
            Item {
                id: welcomeStep

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 0
                    width: Math.min(parent.width, 500)

                    // Logo
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                        sourceSize: Qt.size(80, 80)
                        fillMode: Image.PreserveAspectFit
                    }

                    Item { Layout.preferredHeight: 24 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "MakineAI"
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Oyunlarını Türkçe oyna")
                        font.pixelSize: 16
                        color: Theme.textSecondary
                    }

                    Item { Layout.preferredHeight: 40 }

                    // Feature cards
                    Repeater {
                        model: [
                            {
                                useTrBadge: true,
                                title: qsTr("Türkçe Yama"),
                                desc: qsTr("Steam, Epic, GOG — hangisini kullanıyorsan")
                            },
                            {
                                useTrBadge: false,
                                icon: "qrc:/qt/qml/MakineAI/resources/icons/arrow_right.svg",
                                title: qsTr("Türkçe yapar"),
                                desc: qsTr("Topluluk çevirilerini kurar, orijinalleri yedekler")
                            },
                            {
                                useTrBadge: false,
                                icon: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg",
                                title: qsTr("Geri alınır"),
                                desc: qsTr("Beğenmediysen kaldır, hiçbir şey bozulmaz")
                            }
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            Layout.topMargin: index > 0 ? 8 : 0
                            radius: 10
                            color: Theme.withAlpha(Theme.surface, 0.5)
                            border.color: Theme.glassBorder
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 14

                                // TR badge or icon
                                Rectangle {
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    radius: 8
                                    color: modelData.useTrBadge
                                           ? Theme.turkishRed
                                           : Theme.withAlpha(Theme.primary, 0.08)

                                    Text {
                                        visible: modelData.useTrBadge
                                        anchors.centerIn: parent
                                        text: "TR"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                    }

                                    Image {
                                        visible: !modelData.useTrBadge
                                        anchors.centerIn: parent
                                        source: modelData.icon || ""
                                        sourceSize: Qt.size(18, 18)
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: modelData.title
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.desc
                                        font.pixelSize: 12
                                        color: Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 36 }

                    // Start button
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 44

                        contentItem: Text {
                            text: qsTr("Devam Et")
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: Theme.textOnColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: parent.parent.hovered ? Theme.primaryHover : Theme.primary }
                                GradientStop { position: 1.0; color: parent.parent.hovered ? Theme.accentHover : Theme.accent }
                            }
                        }

                        scale: pressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 80 } }

                        onClicked: root.currentStep = 1
                    }
                }
            }

            // ==========================================
            //  STEP 2: LIBRARY SCAN
            // ==========================================
            Item {
                id: scanStep

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 0
                    width: Math.min(parent.width, 460)

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Oyunlarımı Bul")
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        font.letterSpacing: -0.3
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Kurulu oyunları tespit et")
                        font.pixelSize: 14
                        color: Theme.textSecondary
                    }

                    Item { Layout.preferredHeight: 32 }

                    // Scan button or result
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 360
                        Layout.preferredHeight: scanContent.implicitHeight + 40
                        radius: 12
                        color: Theme.withAlpha(Theme.surface, 0.5)
                        border.color: Theme.glassBorder
                        border.width: 1

                        ColumnLayout {
                            id: scanContent
                            anchors.centerIn: parent
                            width: parent.width - 40
                            spacing: 16

                            // Not scanned yet
                            ColumnLayout {
                                visible: !root.isScanning && !root.scanDone
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 12

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Steam, Epic ve GOG oyunlarını arar.")
                                    font.pixelSize: 13
                                    color: Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }

                                Button {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 180
                                    Layout.preferredHeight: 40

                                    contentItem: Text {
                                        text: qsTr("Taramayı Başlat")
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: Theme.textOnColor
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.parent.hovered ? Theme.primaryHover : Theme.primary
                                    }

                                    onClicked: {
                                        root.isScanning = true
                                        root.scanStage = 0
                                        scanStageTimer.start()
                                        if (typeof GameService !== "undefined") {
                                            GameService.scanAllLibraries()
                                        } else {
                                            // GameService unavailable — complete scan with 0 results
                                            root.isScanning = false
                                            root.scanDone = true
                                            root.foundGames = 0
                                            scanStageTimer.stop()
                                        }
                                    }
                                }
                            }

                            // Scanning in progress
                            ColumnLayout {
                                visible: root.isScanning
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 12

                                BusyIndicator {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    running: root.isScanning
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.scanStage < root.scanStageTexts.length
                                          ? root.scanStageTexts[root.scanStage]
                                          : root.scanStageTexts[root.scanStageTexts.length - 1]
                                    font.pixelSize: 14
                                    color: Theme.textSecondary

                                    Behavior on text {
                                        SequentialAnimation {
                                            NumberAnimation { target: parent; property: "opacity"; to: 0; duration: 120 }
                                            PropertyAction {}
                                            NumberAnimation { target: parent; property: "opacity"; to: 1; duration: 120 }
                                        }
                                    }
                                }
                            }

                            // Scan completed
                            ColumnLayout {
                                visible: root.scanDone
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 8

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.foundGames > 0
                                          ? qsTr("%1 oyun tespit edildi").arg(root.foundGames)
                                          : qsTr("Oyun bulunamadı")
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    color: root.foundGames > 0 ? Theme.success : Theme.textSecondary
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.maximumWidth: 300
                                    text: root.foundGames > 0
                                          ? qsTr("Ana ekrandan Türkçe çevirisi olanları görebilirsin.")
                                          : qsTr("Sorun değil, ana ekrandan klasör olarak da ekleyebilirsin.")
                                    font.pixelSize: 13
                                    color: Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 32 }

                    // Navigation row
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        Button {
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 40
                            flat: true

                            contentItem: Text {
                                text: qsTr("Geri")
                                font.pixelSize: 14
                                color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 8
                                color: parent.parent.hovered ? Theme.withAlpha(Theme.surface, 0.6) : "transparent"
                            }

                            onClicked: root.currentStep = 0
                        }

                        Button {
                            Layout.preferredWidth: 160
                            Layout.preferredHeight: 40

                            contentItem: Text {
                                text: qsTr("Devam Et")
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: Theme.textOnColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 8
                                color: parent.parent.hovered ? Theme.primaryHover : Theme.primary
                            }

                            onClicked: root.currentStep = 2
                        }
                    }
                }
            }

            // ==========================================
            //  STEP 3: READY
            // ==========================================
            Item {
                id: readyStep

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 0
                    width: Math.min(parent.width, 460)

                    // Checkmark circle
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                        radius: 32
                        color: Theme.withAlpha(Theme.success, 0.1)
                        border.color: Theme.withAlpha(Theme.success, 0.2)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                            color: Theme.success
                        }
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Hazırsın!")
                        font.pixelSize: 28
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 380
                        text: qsTr("Bir oyun seç, Türkçe çevirisini yükle, oyna.")
                        font.pixelSize: 15
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    Item { Layout.preferredHeight: 32 }

                    // Finish button
                    Button {
                        id: finishBtn
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 48

                        contentItem: Text {
                            text: qsTr("Başlayalım")
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: Theme.textOnColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: finishBtn.hovered ? Theme.primaryHover : Theme.primary }
                                GradientStop { position: 1.0; color: finishBtn.hovered ? Theme.accentHover : Theme.accent }
                            }
                        }

                        scale: pressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 80 } }

                        onClicked: root.wizardFinished()
                    }
                }
            }
        }

        // ===== BOTTOM: Step indicators + Skip =====
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.topMargin: 16

            Item { Layout.fillWidth: true }

            // Step dots
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 6

                Repeater {
                    model: root.totalSteps
                    Rectangle {
                        required property int index
                        width: index === root.currentStep ? 24 : 8
                        height: 6
                        radius: 3
                        color: index === root.currentStep ? Theme.primary
                             : index < root.currentStep ? Theme.withAlpha(Theme.success, 0.6)
                             : Theme.withAlpha(Theme.surfaceActive, 0.5)

                        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Skip / continue link (right side)
            Text {
                visible: !root.isLastStep
                text: root.currentStep === 0 ? qsTr("Atla") : qsTr("Devam Et")
                font.pixelSize: 13
                color: skipMa.containsMouse ? Theme.textSecondary : Theme.textMuted
                Behavior on color { ColorAnimation { duration: 150 } }

                MouseArea {
                    id: skipMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.currentStep === 0)
                            root.wizardFinished()  // Skip wizard entirely
                        else
                            root.currentStep++  // Navigate to next step
                    }
                }
            }
        }
    }
}
