import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * OnboardingWizard.qml - First-launch onboarding experience
 *
 * Multi-step wizard that guides new users through:
 * 1. Welcome - What MakineAI does
 * 2. Library scan - Auto-detect game libraries
 * 3. Scan results - Show found games
 * 4. Quick settings - Key preferences
 * 5. Done - Ready to go
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

    // Scan state
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

    // ===== BACKGROUND =====
    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        // Subtle gradient accent
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height * 0.4
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.03) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // ===== MAIN LAYOUT =====
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Dimensions.marginXXL
        spacing: 0

        // Skip button (top right)
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                visible: !root.isLastStep
                text: qsTr("Atla")
                flat: true
                font.pixelSize: Dimensions.fontBody
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.surfaceHover : "transparent"
                    radius: Dimensions.radiusSmall
                }
                onClicked: root.skip()

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Skip onboarding")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.skip()
                Keys.onSpacePressed: root.skip()
            }
        }

        Item { Layout.fillHeight: true; Layout.maximumHeight: 32 }

        // Content area
        StackLayout {
            id: stepStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentStep

            // ===== STEP 1: WELCOME =====
            ColumnLayout {
                spacing: Dimensions.spacingSection

                Item { Layout.fillHeight: true }

                // Logo
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                    sourceSize: Qt.size(80, 80)
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("MakineAI'a Hoş Geldin")
                    font.pixelSize: Dimensions.fontHero
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Oyunlarını Türkçe oynamanın en kolay yolu")
                    font.pixelSize: Dimensions.fontSubtitle
                    color: Theme.textSecondary
                }

                Item { Layout.preferredHeight: 16 }

                // Feature bullets
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Dimensions.spacingXL

                    FeatureBullet {
                        icon: "\uE8F1"  // Search
                        title: qsTr("Otomatik Oyun Algılama")
                        description: qsTr("Steam, Epic Games ve GOG kütüphanelerini otomatik tarar")
                    }
                    FeatureBullet {
                        icon: "\uE8E1"  // Translate
                        title: qsTr("Tek Tıkla Türkçe")
                        description: qsTr("Topluluk çevirilerini kolayca yükle ve uygula")
                    }
                    FeatureBullet {
                        icon: "\uE897"  // Shield
                        title: qsTr("Güvenli ve Geri Alınabilir")
                        description: qsTr("Orijinal dosyalar yedeklenir, her zaman geri dönebilirsin")
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // ===== STEP 2: LIBRARY SCAN =====
            ColumnLayout {
                spacing: Dimensions.spacingSection

                Item { Layout.fillHeight: true }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\uE8F1"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: Dimensions.displayLarge
                    color: Theme.primary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Oyun Kütüphaneni Tarayalım")
                    font.pixelSize: Dimensions.headlineLarge
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 420
                    text: qsTr("Bilgisayarındaki oyun kütüphanelerini otomatik olarak tarayıp\nçevirisi mevcut oyunları bulacağız.")
                    font.pixelSize: Dimensions.fontMD
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: 8 }

                // Platform checkboxes
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Dimensions.spacingLG

                    PlatformRow {
                        platformName: "Steam"
                        platformIcon: "\uE7FC"
                        checked: true
                        enabled: !root.isScanning
                    }
                    PlatformRow {
                        platformName: "Epic Games"
                        platformIcon: "\uE7FC"
                        checked: true
                        enabled: !root.isScanning
                    }
                    PlatformRow {
                        platformName: "GOG Galaxy"
                        platformIcon: "\uE7FC"
                        checked: true
                        enabled: !root.isScanning
                    }
                }

                Item { Layout.preferredHeight: 16 }

                // Scan button
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 42
                    text: root.isScanning ? qsTr("Taranıyor...") : qsTr("Taramayı Başlat")
                    enabled: !root.isScanning
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold

                    contentItem: RowLayout {
                        spacing: Dimensions.spacingMD
                        Item { Layout.fillWidth: true }
                        BusyIndicator {
                            visible: root.isScanning
                            running: root.isScanning
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                        }
                        Text {
                            text: root.isScanning ? qsTr("Taranıyor...") : qsTr("Taramayı Başlat")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.DemiBold
                            color: "white"
                        }
                        Item { Layout.fillWidth: true }
                    }

                    background: Rectangle {
                        radius: Dimensions.radiusStandard
                        color: parent.enabled ? (parent.hovered ? Theme.primaryHover : Theme.primary) : Theme.surfaceActive
                    }

                    onClicked: {
                        root.isScanning = true
                        GameService.scanAllLibraries()
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Start library scan")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { root.isScanning = true; GameService.scanAllLibraries() }
                    Keys.onSpacePressed: { root.isScanning = true; GameService.scanAllLibraries() }
                }

                Item { Layout.fillHeight: true }
            }

            // ===== STEP 3: SCAN RESULTS =====
            ColumnLayout {
                spacing: Dimensions.spacingSection

                Item { Layout.fillHeight: true }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.foundGames > 0 ? "\uE930" : "\uE783"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: Dimensions.displayLarge
                    color: root.foundGames > 0 ? Theme.success : Theme.textMuted
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.foundGames > 0
                          ? qsTr("%1 Oyun Bulundu!").arg(root.foundGames)
                          : qsTr("Henüz Oyun Bulunamadı")
                    font.pixelSize: Dimensions.headlineLarge
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

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
                }

                // Game count summary
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 60
                    visible: root.foundGames > 0
                    radius: Dimensions.radiusStandard
                    color: Theme.surface
                    border.color: Theme.surfaceActive
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingXL

                        Column {
                            spacing: Dimensions.spacingXXS
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

                        Rectangle {
                            width: 1
                            height: 32
                            color: Theme.surfaceActive
                        }

                        Column {
                            spacing: Dimensions.spacingXXS
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

            // ===== STEP 4: QUICK SETTINGS =====
            ColumnLayout {
                spacing: Dimensions.spacingSection

                Item { Layout.fillHeight: true }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\uE713"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: Dimensions.displayLarge
                    color: Theme.primary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Hızlı Ayarlar")
                    font.pixelSize: Dimensions.headlineLarge
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Temel tercihlerini ayarla — sonra istediğin zaman değiştirebilirsin")
                    font.pixelSize: Dimensions.fontMD
                    color: Theme.textSecondary
                }

                Item { Layout.preferredHeight: 8 }

                // Settings toggles
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 400
                    spacing: 0

                    SettingToggle {
                        title: qsTr("Windows ile birlikte başlat")
                        description: qsTr("Bilgisayar açıldığında MakineAI sistem tepsisinde beklesin")
                        checked: SettingsManager.startWithWindows
                        onToggled: SettingsManager.startWithWindows = checked
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceActive }

                    SettingToggle {
                        title: qsTr("Kapatınca tepside küçült")
                        description: qsTr("Pencere kapatıldığında uygulama arka planda çalışsın")
                        checked: SettingsManager.minimizeToTray
                        onToggled: SettingsManager.minimizeToTray = checked
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceActive }

                    SettingToggle {
                        title: qsTr("Bildirimler")
                        description: qsTr("Yeni çeviri ve güncellemeler hakkında bildirim al")
                        checked: SettingsManager.showNotifications
                        onToggled: SettingsManager.showNotifications = checked
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceActive }

                    SettingToggle {
                        title: qsTr("Otomatik oyun algılama")
                        description: qsTr("Yeni yüklenen oyunları otomatik olarak algıla")
                        checked: SettingsManager.autoDetectGames
                        onToggled: SettingsManager.autoDetectGames = checked
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // ===== STEP 5: DONE =====
            ColumnLayout {
                spacing: Dimensions.spacingSection

                Item { Layout.fillHeight: true }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\uE930"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: Dimensions.displayXL
                    color: Theme.success
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Her Şey Hazır!")
                    font.pixelSize: Dimensions.fontHero
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 420
                    text: qsTr("MakineAI kullanıma hazır.\nAna ekrandan bir oyun seç ve Türkçe çevirisini yükle!")
                    font.pixelSize: Dimensions.fontSubtitle
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: 16 }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 46
                    font.pixelSize: Dimensions.fontSubtitle
                    font.weight: Font.DemiBold

                    contentItem: Text {
                        text: qsTr("Başlayalım!")
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                    }

                    background: Rectangle {
                        radius: Dimensions.radiusStandard
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: parent.parent.hovered ? Theme.primaryHover : Theme.primary }
                            GradientStop { position: 1.0; color: parent.parent.hovered ? Theme.accentHover : Theme.accent }
                        }
                    }

                    onClicked: root.nextStep()

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Start using MakineAI")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: root.nextStep()
                    Keys.onSpacePressed: root.nextStep()
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true; Layout.maximumHeight: 16 }

        // ===== BOTTOM NAVIGATION =====
        RowLayout {
            Layout.fillWidth: true
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
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.surfaceHover : "transparent"
                    radius: Dimensions.radiusSmall
                }
                onClicked: root.previousStep()

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Go back")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.previousStep()
                Keys.onSpacePressed: root.previousStep()
            }

            Item { Layout.fillWidth: true }

            // Step indicators
            Row {
                spacing: Dimensions.spacingMD
                Layout.alignment: Qt.AlignHCenter

                Repeater {
                    model: root.totalSteps
                    Rectangle {
                        width: index === root.currentStep ? 24 : 8
                        height: 8
                        radius: 4
                        color: index === root.currentStep ? Theme.primary
                             : index < root.currentStep ? Theme.success
                             : Theme.surfaceActive

                        Behavior on width { NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: Dimensions.transitionDuration } }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Next button
            Button {
                visible: !root.isLastStep
                text: qsTr("Devam")
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                }

                background: Rectangle {
                    radius: Dimensions.radiusSmall
                    color: parent.hovered ? Theme.primaryHover : Theme.primary
                    implicitWidth: 80
                    implicitHeight: 34
                }

                onClicked: root.nextStep()

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Continue to next step")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.nextStep()
                Keys.onSpacePressed: root.nextStep()
            }
        }

        Item { Layout.preferredHeight: 8 }
    }

    // ===== INLINE COMPONENTS =====

    component FeatureBullet: RowLayout {
        property string icon: ""
        property string title: ""
        property string description: ""

        Layout.preferredWidth: 380
        spacing: Dimensions.spacingXL

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: 10
            color: Theme.surface

            Text {
                anchors.centerIn: parent
                text: parent.parent.icon
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: Dimensions.fontTitle
                color: Theme.primary
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXXS

            Text {
                text: parent.parent.title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                text: parent.parent.description
                font.pixelSize: Dimensions.fontSM
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
    }

    component PlatformRow: RowLayout {
        property string platformName: ""
        property string platformIcon: ""
        property alias checked: checkbox.checked

        spacing: Dimensions.spacingLG
        Layout.preferredWidth: 240

        CheckBox {
            id: checkbox
            checked: true
        }

        Text {
            text: parent.platformIcon
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: Dimensions.fontLG
            color: Theme.textSecondary
        }

        Text {
            text: parent.platformName
            font.pixelSize: Dimensions.fontMD
            color: Theme.textPrimary
        }
    }

    component SettingToggle: RowLayout {
        property string title: ""
        property string description: ""
        property alias checked: toggle.checked

        signal toggled()

        Layout.fillWidth: true
        Layout.preferredHeight: 64
        spacing: Dimensions.spacingLG

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXXS

            Text {
                text: parent.parent.title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                text: parent.parent.description
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }

        Switch {
            id: toggle
            onCheckedChanged: parent.toggled()
        }
    }
}
