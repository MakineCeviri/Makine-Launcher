import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * OnboardingWizard.qml - Professional first-launch experience
 *
 * 5-step wizard: Welcome → Library Scan → Results → Settings → Done
 * Features: animated step transitions, glass cards, Canvas icons, brand accents
 */
Rectangle {
    id: root
    color: Theme.bgPrimary
    visible: !SettingsManager.onboardingCompleted

    signal completed()

    property int currentStep: 0
    readonly property int totalSteps: 5
    readonly property bool isFirstStep: currentStep === 0
    readonly property bool isLastStep: currentStep === totalSteps - 1

    property bool isScanning: false
    property int foundGames: 0

    function nextStep() {
        if (isLastStep) {
            SettingsManager.onboardingCompleted = true
            root.completed()
        } else {
            currentStep++
        }
    }

    function previousStep() {
        if (currentStep > 0) currentStep--
    }

    function skip() {
        SettingsManager.onboardingCompleted = true
        root.completed()
    }

    Connections {
        target: GameService
        function onScanCompleted(count) {
            root.isScanning = false
            root.foundGames = count
        }
    }

    // ===== INLINE COMPONENTS =====

    // Animated wrapper for each step — fade + slide-up on enter
    component AnimatedStep: Item {
        id: _step
        opacity: 0
        transform: Translate { id: _t; y: 18 }

        property bool isCurrent: StackLayout.isCurrentItem

        ParallelAnimation {
            id: _enterAnim
            NumberAnimation { target: _step; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
            NumberAnimation { target: _t; property: "y"; from: 18; to: 0; duration: 320; easing.type: Easing.OutCubic }
        }

        onIsCurrentChanged: {
            if (isCurrent) {
                _enterAnim.restart()
            } else {
                _enterAnim.stop()
                opacity = 0
                _t.y = 18
            }
        }
    }

    // Glass card with subtle border and hover lift
    component GlassCard: Rectangle {
        id: _card
        color: Theme.withAlpha(Theme.surface, 0.6)
        border.color: Theme.glassBorder
        border.width: 1
        radius: 8

        property bool hoverable: false
        readonly property bool isHovered: hoverable && _cardMa.containsMouse

        transform: Translate { y: _card.isHovered ? -2 : 0; Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } } }

        Behavior on border.color { ColorAnimation { duration: 200 } }
        border.color: isHovered ? Theme.withAlpha(Theme.primary, 0.3) : Theme.glassBorder

        MouseArea {
            id: _cardMa
            anchors.fill: parent
            hoverEnabled: _card.hoverable
            acceptedButtons: Qt.NoButton
        }
    }

    // Canvas-drawn stroke icon
    component StrokeIcon: Canvas {
        id: _icon
        property string iconType: "search"
        property color iconColor: Theme.primary
        property real strokeWidth: 2.0
        width: 32; height: 32

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = iconColor
            ctx.fillStyle = iconColor
            ctx.lineWidth = strokeWidth
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            var s = width

            if (iconType === "search") {
                // Magnifying glass
                ctx.beginPath()
                ctx.arc(s * 0.38, s * 0.38, s * 0.25, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(s * 0.56, s * 0.56)
                ctx.lineTo(s * 0.78, s * 0.78)
                ctx.stroke()
            } else if (iconType === "translate") {
                // Globe with meridians
                ctx.beginPath()
                ctx.arc(s * 0.5, s * 0.5, s * 0.34, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(s * 0.16, s * 0.5)
                ctx.lineTo(s * 0.84, s * 0.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.ellipse(s * 0.32, s * 0.16, s * 0.36, s * 0.68)
                ctx.stroke()
            } else if (iconType === "shield") {
                // Shield outline
                ctx.beginPath()
                ctx.moveTo(s * 0.5, s * 0.12)
                ctx.lineTo(s * 0.82, s * 0.26)
                ctx.lineTo(s * 0.82, s * 0.52)
                ctx.quadraticCurveTo(s * 0.82, s * 0.78, s * 0.5, s * 0.9)
                ctx.quadraticCurveTo(s * 0.18, s * 0.78, s * 0.18, s * 0.52)
                ctx.lineTo(s * 0.18, s * 0.26)
                ctx.closePath()
                ctx.stroke()
                // Checkmark inside
                ctx.beginPath()
                ctx.moveTo(s * 0.34, s * 0.52)
                ctx.lineTo(s * 0.46, s * 0.64)
                ctx.lineTo(s * 0.66, s * 0.4)
                ctx.stroke()
            } else if (iconType === "check") {
                // Large checkmark in circle
                ctx.beginPath()
                ctx.arc(s * 0.5, s * 0.5, s * 0.38, 0, Math.PI * 2)
                ctx.stroke()
                ctx.lineWidth = strokeWidth + 0.5
                ctx.beginPath()
                ctx.moveTo(s * 0.3, s * 0.5)
                ctx.lineTo(s * 0.45, s * 0.65)
                ctx.lineTo(s * 0.7, s * 0.36)
                ctx.stroke()
            } else if (iconType === "gear") {
                // Simplified gear: circle + 6 teeth
                var cx = s * 0.5, cy = s * 0.5
                ctx.beginPath()
                ctx.arc(cx, cy, s * 0.16, 0, Math.PI * 2)
                ctx.stroke()
                for (var a = 0; a < 6; a++) {
                    var angle = a * Math.PI / 3
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(angle) * s * 0.22, cy + Math.sin(angle) * s * 0.22)
                    ctx.lineTo(cx + Math.cos(angle) * s * 0.36, cy + Math.sin(angle) * s * 0.36)
                    ctx.stroke()
                }
                ctx.beginPath()
                ctx.arc(cx, cy, s * 0.3, 0, Math.PI * 2)
                ctx.stroke()
            } else if (iconType === "empty") {
                // Folder with question mark
                ctx.beginPath()
                ctx.moveTo(s * 0.14, s * 0.3)
                ctx.lineTo(s * 0.14, s * 0.78)
                ctx.lineTo(s * 0.86, s * 0.78)
                ctx.lineTo(s * 0.86, s * 0.3)
                ctx.lineTo(s * 0.48, s * 0.3)
                ctx.lineTo(s * 0.4, s * 0.2)
                ctx.lineTo(s * 0.14, s * 0.2)
                ctx.closePath()
                ctx.stroke()
                // Question mark
                ctx.font = "bold " + (s * 0.28) + "px Inter, Segoe UI"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText("?", s * 0.5, s * 0.56)
            }
        }
    }

    // Feature row: icon + title + description
    component FeatureRow: Item {
        id: _fr
        property string iconType: "search"
        property color accentColor: Theme.primary
        property string title: ""
        property string description: ""

        Layout.preferredWidth: 400
        Layout.preferredHeight: 60
        Layout.alignment: Qt.AlignHCenter

        RowLayout {
            anchors.fill: parent
            spacing: 16

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: 10
                color: Theme.withAlpha(_fr.accentColor, 0.1)
                border.color: Theme.withAlpha(_fr.accentColor, 0.15)
                border.width: 1

                StrokeIcon {
                    anchors.centerIn: parent
                    width: 24; height: 24
                    iconType: _fr.iconType
                    iconColor: _fr.accentColor
                    strokeWidth: 1.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: _fr.title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    text: _fr.description
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // Platform card for scan step
    component PlatformCard: GlassCard {
        id: _pc
        property string platformName: ""
        property alias checked: _pcCheck.checked
        hoverable: true

        Layout.preferredWidth: 240
        Layout.preferredHeight: 48
        color: _pcCheck.checked ? Theme.withAlpha(Theme.primary, 0.06) : Theme.withAlpha(Theme.surface, 0.4)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            CheckBox {
                id: _pcCheck
                checked: true
            }

            Text {
                Layout.fillWidth: true
                text: _pc.platformName
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

    // Setting row with toggle
    component SettingRow: Item {
        id: _sr
        property string title: ""
        property string description: ""
        property alias checked: _srToggle.checked
        signal toggled()

        Layout.fillWidth: true
        Layout.preferredHeight: 58

        RowLayout {
            anchors.fill: parent
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: _sr.title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    text: _sr.description
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }

            Switch {
                id: _srToggle
                onCheckedChanged: _sr.toggled()
            }
        }
    }

    // Gradient action button
    component GradientButton: Button {
        id: _gb
        property bool useGradient: false

        contentItem: Text {
            text: _gb.text
            font: _gb.font
            color: Theme.textOnColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 6
            gradient: _gb.useGradient ? _gradObj : null
            color: _gb.useGradient ? "transparent" : (_gb.hovered ? Theme.primaryHover : Theme.primary)

            Gradient {
                id: _gradObj
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: _gb.hovered ? Theme.primaryHover : Theme.primary }
                GradientStop { position: 1.0; color: _gb.hovered ? Theme.accentHover : Theme.accent }
            }
        }

        scale: pressed ? 0.97 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }

        Accessible.role: Accessible.Button
        activeFocusOnTab: true
    }

    // ===== BACKGROUND =====
    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        // Top ambient glow
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height * 0.45
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.04) }
                GradientStop { position: 0.6; color: Theme.withAlpha(Theme.secondary, 0.015) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Brand accent line at very top (1px)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.brandGold }
                GradientStop { position: 0.25; color: Theme.brandCoral }
                GradientStop { position: 0.5; color: Theme.brandPurple }
                GradientStop { position: 0.75; color: Theme.brandBlue }
                GradientStop { position: 1.0; color: Theme.brandGreen }
            }
        }
    }

    // ===== MAIN LAYOUT =====
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Dimensions.marginXL
        anchors.bottomMargin: Dimensions.marginLG
        anchors.leftMargin: Dimensions.marginXXL
        anchors.rightMargin: Dimensions.marginXXL
        spacing: 0

        // Skip button (top right)
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                visible: !root.isLastStep
                text: qsTr("Atla")
                flat: true
                font.pixelSize: Dimensions.fontSM
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.hovered ? Theme.textSecondary : Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.withAlpha(Theme.surface, 0.5) : "transparent"
                    radius: 6
                    implicitWidth: 52
                    implicitHeight: 28
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                onClicked: root.skip()
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Skip onboarding")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.skip()
            }
        }

        // Content area
        StackLayout {
            id: stepStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentStep

            // ===== STEP 1: WELCOME =====
            AnimatedStep {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    // Logo
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                        sourceSize: Qt.size(72, 72)
                        fillMode: Image.PreserveAspectFit
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("MakineAI'a Hoş Geldin")
                        font.pixelSize: Dimensions.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 6 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Oyunlarını Türkçe oynamanın en kolay yolu")
                        font.pixelSize: Dimensions.fontSubtitle
                        color: Theme.textSecondary
                    }

                    Item { Layout.preferredHeight: 32 }

                    // Feature highlights
                    ColumnLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        FeatureRow {
                            iconType: "search"
                            accentColor: Theme.brandBlue
                            title: qsTr("Otomatik Oyun Algılama")
                            description: qsTr("Steam, Epic Games ve GOG kütüphanelerini otomatik tarar")
                        }
                        FeatureRow {
                            iconType: "translate"
                            accentColor: Theme.brandPurple
                            title: qsTr("Tek Tıkla Türkçe")
                            description: qsTr("Topluluk çevirilerini kolayca yükle ve uygula")
                        }
                        FeatureRow {
                            iconType: "shield"
                            accentColor: Theme.brandGreen
                            title: qsTr("Güvenli ve Geri Alınabilir")
                            description: qsTr("Orijinal dosyalar yedeklenir, her zaman geri dönebilirsin")
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== STEP 2: LIBRARY SCAN =====
            AnimatedStep {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    StrokeIcon {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48; height: 48
                        iconType: "search"
                        iconColor: Theme.primary
                        strokeWidth: 2.2
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Oyun Kütüphaneni Tarayalım")
                        font.pixelSize: Dimensions.headlineLarge
                        font.weight: Font.Bold
                        font.letterSpacing: -0.3
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 420
                        text: qsTr("Bilgisayarındaki oyun kütüphanelerini otomatik olarak tarayıp\nçevirisi mevcut oyunları bulacağız.")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    Item { Layout.preferredHeight: 24 }

                    // Platform cards
                    GlassCard {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 280
                        Layout.preferredHeight: platformCol.implicitHeight + 24
                        color: Theme.withAlpha(Theme.surface, 0.35)

                        ColumnLayout {
                            id: platformCol
                            anchors.centerIn: parent
                            anchors.margins: 12
                            spacing: 4

                            PlatformCard {
                                platformName: "Steam"
                                enabled: !root.isScanning
                            }
                            PlatformCard {
                                platformName: "Epic Games"
                                enabled: !root.isScanning
                            }
                            PlatformCard {
                                platformName: "GOG Galaxy"
                                enabled: !root.isScanning
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 20 }

                    // Scan button
                    GradientButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 42
                        text: root.isScanning ? qsTr("Taranıyor...") : qsTr("Taramayı Başlat")
                        enabled: !root.isScanning
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        Accessible.name: qsTr("Start library scan")

                        onClicked: {
                            root.isScanning = true
                            GameService.scanAllLibraries()
                        }
                        Keys.onReturnPressed: { root.isScanning = true; GameService.scanAllLibraries() }
                    }

                    // Scanning indicator
                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        visible: root.isScanning
                        running: root.isScanning
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== STEP 3: SCAN RESULTS =====
            AnimatedStep {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    StrokeIcon {
                        Layout.alignment: Qt.AlignHCenter
                        width: 56; height: 56
                        iconType: root.foundGames > 0 ? "check" : "empty"
                        iconColor: root.foundGames > 0 ? Theme.success : Theme.textMuted
                        strokeWidth: 2.5
                    }

                    Item { Layout.preferredHeight: 16 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.foundGames > 0
                              ? qsTr("%1 Oyun Bulundu!").arg(root.foundGames)
                              : qsTr("Henüz Oyun Bulunamadı")
                        font.pixelSize: Dimensions.headlineLarge
                        font.weight: Font.Bold
                        font.letterSpacing: -0.3
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 420
                        text: root.foundGames > 0
                              ? qsTr("Oyun kütüphanen başarıyla tarandı.\nAna ekrandan çevirisi olan oyunları görebilirsin.")
                              : qsTr("Endişelenme! Ana ekrandan manuel olarak\noyun klasörü ekleyebilirsin.")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    Item { Layout.preferredHeight: 20 }

                    // Stats card
                    GlassCard {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 280
                        Layout.preferredHeight: 70
                        visible: root.foundGames > 0

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 24

                            Column {
                                spacing: 2
                                Text {
                                    text: root.foundGames.toString()
                                    font.pixelSize: Dimensions.fontHeadline
                                    font.weight: Font.Bold
                                    color: Theme.primary
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                                Text {
                                    text: qsTr("Toplam Oyun")
                                    font.pixelSize: Dimensions.fontXS
                                    color: Theme.textMuted
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            Rectangle { width: 1; height: 36; color: Theme.glassBorder }

                            Column {
                                spacing: 2
                                Text {
                                    text: GameService.gameCount.toString()
                                    font.pixelSize: Dimensions.fontHeadline
                                    font.weight: Font.Bold
                                    color: Theme.success
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                                Text {
                                    text: qsTr("Kütüphanede")
                                    font.pixelSize: Dimensions.fontXS
                                    color: Theme.textMuted
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== STEP 4: QUICK SETTINGS =====
            AnimatedStep {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    StrokeIcon {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48; height: 48
                        iconType: "gear"
                        iconColor: Theme.primary
                        strokeWidth: 2.0
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Hızlı Ayarlar")
                        font.pixelSize: Dimensions.headlineLarge
                        font.weight: Font.Bold
                        font.letterSpacing: -0.3
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 6 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Temel tercihlerini ayarla \u2014 sonra istediğin zaman değiştirebilirsin")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                    }

                    Item { Layout.preferredHeight: 24 }

                    // Settings card
                    GlassCard {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 440
                        Layout.preferredHeight: settingsCol.implicitHeight + 32
                        color: Theme.withAlpha(Theme.surface, 0.35)

                        ColumnLayout {
                            id: settingsCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 20
                            spacing: 0

                            SettingRow {
                                title: qsTr("Windows ile birlikte başlat")
                                description: qsTr("Bilgisayar açıldığında MakineAI sistem tepsisinde beklesin")
                                checked: SettingsManager.startWithWindows
                                onToggled: SettingsManager.startWithWindows = checked
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder }

                            SettingRow {
                                title: qsTr("Kapatınca tepside küçült")
                                description: qsTr("Pencere kapatıldığında uygulama arka planda çalışsın")
                                checked: SettingsManager.minimizeToTray
                                onToggled: SettingsManager.minimizeToTray = checked
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder }

                            SettingRow {
                                title: qsTr("Bildirimler")
                                description: qsTr("Yeni çeviri ve güncellemeler hakkında bildirim al")
                                checked: SettingsManager.showNotifications
                                onToggled: SettingsManager.showNotifications = checked
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder }

                            SettingRow {
                                title: qsTr("Otomatik oyun algılama")
                                description: qsTr("Yeni yüklenen oyunları otomatik olarak algıla")
                                checked: SettingsManager.autoDetectGames
                                onToggled: SettingsManager.autoDetectGames = checked
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== STEP 5: DONE =====
            AnimatedStep {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    StrokeIcon {
                        Layout.alignment: Qt.AlignHCenter
                        width: 64; height: 64
                        iconType: "check"
                        iconColor: Theme.success
                        strokeWidth: 2.8
                    }

                    Item { Layout.preferredHeight: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Her Şey Hazır!")
                        font.pixelSize: Dimensions.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        color: Theme.textPrimary
                    }

                    Item { Layout.preferredHeight: 8 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 420
                        text: qsTr("MakineAI kullanıma hazır.\nAna ekrandan bir oyun seç ve Türkçe çevirisini yükle!")
                        font.pixelSize: Dimensions.fontSubtitle
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    Item { Layout.preferredHeight: 28 }

                    GradientButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 46
                        text: qsTr("Başlayalım!")
                        font.pixelSize: Dimensions.fontSubtitle
                        font.weight: Font.DemiBold
                        useGradient: true
                        Accessible.name: qsTr("Start using MakineAI")

                        onClicked: root.nextStep()
                        Keys.onReturnPressed: root.nextStep()
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        // ===== BOTTOM NAVIGATION =====
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: Dimensions.spacingXL

            // Back button
            Button {
                visible: !root.isFirstStep && !root.isLastStep
                text: qsTr("Geri")
                flat: true
                font.pixelSize: Dimensions.fontBody
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.surfaceHover : "transparent"
                    radius: 6
                    implicitWidth: 72
                    implicitHeight: 34
                }
                onClicked: root.previousStep()
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Go back")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.previousStep()
            }

            Item { Layout.fillWidth: true }

            // Step indicators
            Row {
                spacing: 6
                Layout.alignment: Qt.AlignHCenter

                Repeater {
                    model: root.totalSteps
                    Rectangle {
                        required property int index
                        width: index === root.currentStep ? 28 : 8
                        height: 6
                        radius: 3
                        color: index === root.currentStep ? Theme.primary
                             : index < root.currentStep ? Theme.withAlpha(Theme.success, 0.7)
                             : Theme.withAlpha(Theme.surfaceActive, 0.6)

                        Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Next button
            Button {
                visible: !root.isLastStep
                text: root.currentStep === 0 ? qsTr("Başla") : qsTr("Devam")
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textOnColor
                    horizontalAlignment: Text.AlignHCenter
                }

                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? Theme.primaryHover : Theme.primary
                    implicitWidth: 88
                    implicitHeight: 34
                }

                scale: pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 80 } }

                onClicked: root.nextStep()
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Continue to next step")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.nextStep()
            }
        }
    }
}
