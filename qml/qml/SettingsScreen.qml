import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SettingsScreen.qml
 *
 * Yapı:
 * - 280px sidebar
 * - Content area with 5 categories
 * - Animated toggle switches (44x24)
 * - Theme selector (Açık/Koyu)
 */
Item {
    id: root

    signal back()

    property int selectedCategory: 0

    property var categories: [
        { name: "Genel", description: "Uygulama genel ayarlarını yapılandırın" },
        { name: "Çeviri", description: "Çeviri tercihlerini ve dil ayarlarını düzenleyin" },
        { name: "Projeler", description: "Tamamlanan çeviriler ve yedekler" },
        { name: "Performans", description: "Performans ve kaynak kullanım ayarları" },
        { name: "Hakkında", description: "Uygulama hakkında bilgiler" },
        { name: "Geliştirici", description: "Geliştirici araçları ve test özellikleri" }
    ]

    // Settings state - bound to SettingsManager
    property bool autoDetectGames: SettingsManager.autoDetectGames
    property bool startWithWindows: SettingsManager.startWithWindows
    property bool minimizeToTray: SettingsManager.minimizeToTray
    property bool showNotifications: SettingsManager.showNotifications
    property bool hardwareAcceleration: SettingsManager.hardwareAcceleration
    property bool useGlobalCache: SettingsManager.useGlobalCache
    property bool disableAnimations: !SettingsManager.enableAnimations

    // Developer status
    property string devStatus: ""
    property bool isImporting: false

    // Update SettingsManager when properties change
    onAutoDetectGamesChanged: SettingsManager.autoDetectGames = autoDetectGames
    onStartWithWindowsChanged: SettingsManager.startWithWindows = startWithWindows
    onMinimizeToTrayChanged: SettingsManager.minimizeToTray = minimizeToTray
    onShowNotificationsChanged: SettingsManager.showNotifications = showNotifications
    onHardwareAccelerationChanged: SettingsManager.hardwareAcceleration = hardwareAcceleration
    onUseGlobalCacheChanged: SettingsManager.useGlobalCache = useGlobalCache
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations

    // Entry animation state
    property bool animationComplete: false

    Component.onCompleted: {
        entryAnimation.start()
    }

    // Smooth entry animation
    ParallelAnimation {
        id: entryAnimation

        NumberAnimation {
            target: sidebar
            property: "opacity"
            from: 0
            to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: sidebar
            property: "x"
            from: -40
            to: 0
            duration: 350
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea
            property: "opacity"
            from: 0
            to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea
            property: "x"
            from: 40
            to: 0
            duration: 350
            easing.type: Easing.OutCubic
        }

        onFinished: root.animationComplete = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        Row {
            anchors.fill: parent
            spacing: 0

            // ===== LEFT SIDEBAR (280px) =====
            Rectangle {
                id: sidebar
                width: 280
                height: parent.height
                opacity: 0  // Start invisible for animation
                x: 0
                color: Theme.withAlpha(Theme.surface, 0.5)

                // Right border
                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Header with back button
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Dimensions.navbarHeight

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 16

                            // Back button
                            Rectangle {
                                id: backBtn
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: Dimensions.radiusStandard
                                color: backMouse.containsMouse
                                    ? Theme.surfaceHover
                                    : Qt.rgba(1, 1, 1, 0.06)

                                scale: backMouse.pressed ? 0.92 : 1.0

                                // Hover border glow
                                border.color: backMouse.containsMouse
                                    ? Qt.rgba(1, 1, 1, 0.2)
                                    : "transparent"
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
                                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                                Label {
                                    anchors.centerIn: parent
                                    text: "\u2190"  // Left arrow
                                    font.pixelSize: 18
                                    color: backMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                MouseArea {
                                    id: backMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.back()
                                }
                            }

                            // Title
                            Label {
                                text: "Ayarlar"
                                font.pixelSize: 24
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Qt.rgba(1, 1, 1, 0.06)
                    }

                    Item { Layout.preferredHeight: 12 }

                    // Category list
                    Repeater {
                        model: categories

                        CategoryItem {
                            Layout.fillWidth: true
                            Layout.leftMargin: 12
                            Layout.rightMargin: 12
                            categoryIndex: index
                            name: modelData.name
                            isSelected: selectedCategory === index
                            onClicked: {
                                selectedCategory = index
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // Version info at bottom
                    Label {
                        Layout.leftMargin: 20
                        Layout.bottomMargin: 20
                        text: Dimensions.appName + " " + Dimensions.appVersionFull
                        font.pixelSize: 12
                        color: Theme.textMuted
                    }
                }
            }

            // ===== RIGHT CONTENT AREA =====
            Item {
                id: contentArea
                width: parent.width - sidebar.width
                height: parent.height
                opacity: 0  // Start invisible for animation
                x: 0

                ScrollView {
                    id: settingsScrollView
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        background: Rectangle { color: "transparent" }
                        contentItem: Rectangle {
                            implicitWidth: 8
                            radius: Dimensions.radiusStandard
                            color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                        }
                    }

                    ColumnLayout {
                        width: settingsScrollView.availableWidth
                        spacing: 16

                        Item { Layout.preferredHeight: 32 }

                        // Category title with fade animation
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 32
                            Layout.rightMargin: 32
                            spacing: 8

                            Label {
                                text: categories[selectedCategory].name
                                font.pixelSize: 28
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true

                                opacity: 1.0
                                Behavior on text {
                                    SequentialAnimation {
                                        NumberAnimation { target: parent; property: "opacity"; to: 0; duration: 100 }
                                        PropertyAction { }
                                        NumberAnimation { target: parent; property: "opacity"; to: 1; duration: 150 }
                                    }
                                }
                            }

                            Label {
                                text: categories[selectedCategory].description
                                font.pixelSize: 14
                                color: Theme.textMuted
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        // Settings content based on category
                        Loader {
                            id: contentLoader
                            Layout.fillWidth: true
                            Layout.leftMargin: 32
                            Layout.rightMargin: 32
                            Layout.preferredWidth: Math.min(settingsScrollView.availableWidth - 64, 640)

                            // Simple smooth opacity animation
                            opacity: 1.0
                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 200
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // Track pending category for smooth transition
                            property int pendingCategory: selectedCategory

                            sourceComponent: {
                                switch(pendingCategory) {
                                    case 0: return generalSettings
                                    case 1: return translationSettings
                                    case 2: return projectsSettings
                                    case 3: return performanceSettings
                                    case 4: return aboutSettings
                                    case 5: return developerSettings
                                    default: return null
                                }
                            }

                            // Smooth fade transition on category change
                            Connections {
                                target: root
                                function onSelectedCategoryChanged() {
                                    // Start fade out
                                    contentLoader.opacity = 0
                                    categoryChangeTimer.start()
                                }
                            }

                            Timer {
                                id: categoryChangeTimer
                                interval: 150  // Wait for fade out
                                onTriggered: {
                                    // Update content and fade in
                                    contentLoader.pendingCategory = selectedCategory
                                    contentLoader.opacity = 1
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 32 }
                    }
                }
            }
        }
    }

    // ===== GENERAL SETTINGS =====
    Component {
        id: generalSettings

        ColumnLayout {
            spacing: 16

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ThemeSetting {}
                }
            }

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ToggleSetting {
                        title: "Otomatik Oyun Tespiti"
                        description: "Oyunları otomatik olarak tespit et"
                        checked: autoDetectGames
                        onToggled: autoDetectGames = !autoDetectGames
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: "Windows ile Başlat"
                        description: "Bilgisayar açıldığında otomatik başlat"
                        checked: startWithWindows
                        onToggled: startWithWindows = !startWithWindows
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: "Sistem Tepsisine Küçült"
                        description: "Kapatıldığında arka planda çalışır"
                        checked: minimizeToTray
                        onToggled: minimizeToTray = !minimizeToTray
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: "Bildirimler"
                        description: "Oyun tespit edildiğinde bildirim göster"
                        checked: showNotifications
                        onToggled: showNotifications = !showNotifications
                    }
                }
            }
        }
    }

    // ===== TRANSLATION SETTINGS =====
    Component {
        id: translationSettings

        ColumnLayout {
            spacing: 16

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    InfoSettingWithBadge {
                        title: "Çeviri Dili"
                        description: "Oyunların çevrileceği dil"
                        badgeText: "Türkçe"
                    }
                }
            }

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DisabledSetting {
                        title: "Çeviri Kalitesi"
                        description: "Bu özellik gelecek güncellemelerde eklenecektir"
                    }
                }
            }
        }
    }

    // ===== PROJECTS SETTINGS - Tamamlanan çeviriler ve yedekler =====
    Component {
        id: projectsSettings

        ColumnLayout {
            spacing: 16

            // Backup list card
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // Section header
                    RowLayout {
                        spacing: 8

                        Text {
                            text: "Yedekler"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        Rectangle {
                            width: backupCountText.width + 12
                            height: 20
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.primary, 0.15)
                            visible: BackupManager.backups.length > 0

                            Text {
                                id: backupCountText
                                anchors.centerIn: parent
                                text: BackupManager.backups.length
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: Theme.primary
                            }
                        }
                    }

                    // Empty state
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        radius: Dimensions.radiusStandard
                        color: Qt.rgba(1, 1, 1, 0.03)
                        visible: BackupManager.backups.length === 0

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                text: "📁"
                                font.pixelSize: 24
                            }

                            Text {
                                text: "Henüz yedeklenmiş oyun yok"
                                font.pixelSize: 14
                                color: Theme.textMuted
                            }
                        }
                    }

                    // Backup list
                    Repeater {
                        model: BackupManager.backups

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            radius: Dimensions.radiusStandard
                            color: backupItemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                // Game icon placeholder
                                Rectangle {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48
                                    radius: Dimensions.radiusStandard
                                    color: Theme.surfaceActive

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.gameName ? modelData.gameName.substring(0, 2).toUpperCase() : "?"
                                        font.pixelSize: 16
                                        font.weight: Font.Bold
                                        color: Theme.textMuted
                                    }
                                }

                                // Backup info
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: modelData.gameName || "Bilinmeyen Oyun"
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: {
                                            var date = new Date(modelData.createdAt)
                                            return date.toLocaleDateString("tr-TR") + " - " +
                                                   (modelData.sizeBytes > 1048576
                                                    ? (modelData.sizeBytes / 1048576).toFixed(1) + " MB"
                                                    : (modelData.sizeBytes / 1024).toFixed(0) + " KB")
                                        }
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                    }
                                }

                                // Restore button
                                Rectangle {
                                    Layout.preferredWidth: restoreBtnContent.width + 20
                                    Layout.preferredHeight: 32
                                    radius: Dimensions.radiusStandard
                                    color: restoreBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary

                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Row {
                                        id: restoreBtnContent
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Text {
                                            text: "↩"
                                            font.pixelSize: 12
                                            color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: "Geri Al"
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: restoreBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            BackupManager.restoreBackup(modelData.id, modelData.originalPath)
                                        }
                                    }
                                }

                                // Delete button
                                Rectangle {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: Dimensions.radiusStandard
                                    color: deleteBtnMouse.containsMouse ? Theme.withAlpha(Theme.error, 0.15) : "transparent"

                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "🗑"
                                        font.pixelSize: 14
                                    }

                                    MouseArea {
                                        id: deleteBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            BackupManager.deleteBackup(modelData.id)
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: backupItemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }
                }
            }

            // Restore status indicator
            SettingsCard {
                Layout.fillWidth: true
                visible: BackupManager.isRestoring

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    BusyIndicator {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        running: BackupManager.isRestoring
                    }

                    Text {
                        text: BackupManager.restoreStatus || "Geri yükleniyor..."
                        font.pixelSize: 14
                        color: Theme.textPrimary
                    }
                }
            }
        }
    }

    // ===== PERFORMANCE SETTINGS =====
    Component {
        id: performanceSettings

        ColumnLayout {
            spacing: 16

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ToggleSetting {
                        title: "Donanım Hızlandırma"
                        description: "GPU kullanarak daha hızlı çeviri"
                        checked: hardwareAcceleration
                        onToggled: hardwareAcceleration = !hardwareAcceleration
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: "Global Önbellek"
                        description: "Çevirileri tüm oyunlar için paylaş"
                        checked: useGlobalCache
                        onToggled: useGlobalCache = !useGlobalCache
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: "Uygulama Animasyonları"
                        description: "Arayüz animasyonlarını etkinleştir"
                        checked: !disableAnimations
                        onToggled: disableAnimations = !disableAnimations
                    }
                }
            }
        }
    }

    // ===== ABOUT SETTINGS =====
    Component {
        id: aboutSettings

        ColumnLayout {
            spacing: 16

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    InfoRow { label: "Uygulama"; value: Dimensions.appName }
                    SettingsDivider {}
                    InfoRow { label: "Versiyon"; value: Dimensions.appVersionFull }
                    SettingsDivider {}
                    InfoRow { label: "Geliştirici"; value: "MakineAI Ekibi" }
                    SettingsDivider {}
                    InfoRow { label: "Lisans"; value: "Özel Lisans" }
                }
            }

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ClickableRow {
                        title: "Discord Desteği"
                        subtitle: "Topluluk ve yardım için Discord sunucumuza katılın"
                        icon: "\uD83D\uDCAC"  // 💬
                        onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                    }

                    SettingsDivider {}

                    ClickableRow {
                        title: "Geri Bildirim"
                        subtitle: "Hata bildirimi ve öneriler için web sitemizi ziyaret edin"
                        icon: "\uD83D\uDCE7"  // 📧
                        onClicked: Qt.openUrlExternally("https://makineai.com/feedback")
                    }
                }
            }
        }
    }

    // ===== DEVELOPER SETTINGS =====
    Component {
        id: developerSettings

        ColumnLayout {
            spacing: 16

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DevButton {
                        title: "Test Verisi Aktar"
                        subtitle: "Translation Memory'ye 30 test çevirisi ekle"
                        icon: "⬆"  // Upload icon
                        isLoading: isImporting
                        onClicked: {
                            isImporting = true
                            devStatus = "Test verisi aktarılıyor..."

                            // Add sample TM entries
                            var testData = [
                                ["New Game", "Yeni Oyun"],
                                ["Load Game", "Oyun Yükle"],
                                ["Save Game", "Oyunu Kaydet"],
                                ["Settings", "Ayarlar"],
                                ["Options", "Seçenekler"],
                                ["Exit", "Çıkış"],
                                ["Continue", "Devam Et"],
                                ["Start", "Başla"],
                                ["Quit", "Çık"],
                                ["Yes", "Evet"],
                                ["No", "Hayır"],
                                ["Cancel", "İptal"],
                                ["OK", "Tamam"],
                                ["Back", "Geri"],
                                ["Next", "İleri"],
                                ["Play", "Oyna"],
                                ["Pause", "Duraklat"],
                                ["Resume", "Devam Et"],
                                ["Restart", "Yeniden Başlat"],
                                ["Level", "Seviye"],
                                ["Score", "Puan"],
                                ["Health", "Sağlık"],
                                ["Mana", "Mana"],
                                ["Attack", "Saldırı"],
                                ["Defense", "Savunma"],
                                ["Inventory", "Envanter"],
                                ["Equipment", "Ekipman"],
                                ["Quest", "Görev"],
                                ["Map", "Harita"],
                                ["Skills", "Yetenekler"]
                            ]

                            var added = 0
                            for (var i = 0; i < testData.length; i++) {
                                var entry = testData[i]
                                if (CoreBridge.addTMEntry(entry[0], entry[1], "", "")) {
                                    added++
                                }
                            }

                            isImporting = false
                            devStatus = "✓ " + added + " çeviri eklendi!"
                        }
                    }

                    SettingsDivider {}

                    DevButton {
                        title: "TM'yi Temizle"
                        subtitle: "Tüm Translation Memory verilerini sil"
                        icon: "🗑"  // Delete icon
                        isDestructive: true
                        onClicked: {
                            // Note: This would need a clearTM function in CoreBridge
                            devStatus = "⚠ Bu özellik henüz aktif değil"
                        }
                    }

                    SettingsDivider {}

                    DevButton {
                        title: "TM İstatistikleri"
                        subtitle: "Translation Memory durumunu göster"
                        icon: "📊"  // Analytics icon
                        onClicked: {
                            // Get glossary terms count as proxy
                            var terms = CoreBridge.getAllGlossaryTerms()
                            devStatus = "📊 TM/Glossary İstatistikleri:\n" +
                                       "Glossary Terimleri: " + terms.length + "\n" +
                                       "Durum: Aktif"
                        }
                    }
                }
            }

            SettingsCard {
                Layout.fillWidth: true
                visible: devStatus !== ""

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: statusText.implicitHeight + 32

                        // Selectable status text
                        TextEdit {
                            id: statusText
                            anchors.fill: parent
                            anchors.margins: 16
                            text: devStatus
                            font.pixelSize: 12
                            font.family: Qt.platform.os === "windows" ? "Consolas" : "Courier New"
                            color: Qt.rgba(1, 1, 1, 0.7)
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                        }
                    }
                }
            }
        }
    }

    // ===== CATEGORY ITEM COMPONENT =====
    component CategoryItem: Rectangle {
        id: catItem
        property int categoryIndex: 0
        property string name: ""
        property bool isSelected: false
        signal clicked()

        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        Accessible.role: Accessible.Button
        Accessible.name: name
        Accessible.onPressAction: clicked()

        height: 40
        radius: Dimensions.radiusStandard
        color: isSelected
            ? Qt.rgba(1, 1, 1, 0.08)
            : ((catMouse.containsMouse || catItem.activeFocus) ? Qt.rgba(1, 1, 1, 0.06) : "transparent")

        scale: catMouse.pressed ? 0.97 : 1.0

        // Hover/focus glow border
        border.color: isSelected
            ? Theme.withAlpha(Theme.primary, 0.4)
            : ((catMouse.containsMouse || catItem.activeFocus) ? Qt.rgba(1, 1, 1, 0.15) : "transparent")
        border.width: isSelected ? 1.5 : 1

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            spacing: 12

            // Selection indicator
            Rectangle {
                Layout.preferredWidth: 3
                Layout.preferredHeight: isSelected ? 20 : 0
                radius: 2
                color: isSelected ? Theme.textPrimary : "transparent"

                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
            }

            Label {
                text: name
                font.pixelSize: 14
                font.weight: isSelected ? Font.DemiBold : Font.Medium
                color: isSelected ? Theme.textPrimary
                     : catMouse.containsMouse ? Theme.textPrimary
                     : Theme.textSecondary

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            Item { Layout.fillWidth: true }
        }

        MouseArea {
            id: catMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: catItem.clicked()
        }
    }

    // ===== SETTINGS CARD =====
    component SettingsCard: Rectangle {
        default property alias content: cardContent.data
        implicitHeight: cardContent.implicitHeight

        radius: Dimensions.radiusStandard
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1

        ColumnLayout {
            id: cardContent
            anchors.fill: parent
            spacing: 0
        }
    }

    // ===== DIVIDER =====
    component SettingsDivider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Qt.rgba(1, 1, 1, 0.04)
    }

    // ===== THEME SETTING =====
    component ThemeSetting: Item {
        // Connected to SettingsManager
        property bool isDarkTheme: SettingsManager.isDarkMode

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: "Tema"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    text: "Uygulama görünümünü seç"
                    font.pixelSize: 13
                    color: Theme.textMuted
                }
            }

            // Theme selector container
            Rectangle {
                Layout.preferredWidth: themeRow.width + 8
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: Qt.rgba(1, 1, 1, 0.06)

                Row {
                    id: themeRow
                    anchors.centerIn: parent
                    spacing: 4

                    // Light theme option
                    Rectangle {
                        id: lightThemeBtn
                        width: lightRow.width + 28
                        height: 32
                        radius: Dimensions.radiusStandard
                        color: !isDarkTheme ? Qt.rgba(1, 1, 1, 0.12) : (lightMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        border.color: !isDarkTheme ? Qt.rgba(1, 1, 1, 0.2) : (lightMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent")
                        border.width: 1
                        scale: lightMouse.pressed ? 0.95 : 1.0

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        Row {
                            id: lightRow
                            anchors.centerIn: parent
                            spacing: 6

                            Label {
                                text: "☀"  // Sun icon
                                font.pixelSize: 14
                                color: !isDarkTheme ? Theme.textPrimary : (lightMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: "Açık"
                                font.pixelSize: 13
                                font.weight: !isDarkTheme ? Font.DemiBold : Font.Medium
                                color: !isDarkTheme ? Theme.textPrimary : (lightMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: lightMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: SettingsManager.isDarkMode = false
                        }
                    }

                    // Dark theme option
                    Rectangle {
                        id: darkThemeBtn
                        width: darkRow.width + 28
                        height: 32
                        radius: Dimensions.radiusStandard
                        color: isDarkTheme ? Qt.rgba(1, 1, 1, 0.12) : (darkMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        border.color: isDarkTheme ? Qt.rgba(1, 1, 1, 0.2) : (darkMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent")
                        border.width: 1
                        scale: darkMouse.pressed ? 0.95 : 1.0

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        Row {
                            id: darkRow
                            anchors.centerIn: parent
                            spacing: 6

                            Label {
                                text: "🌙"  // Moon icon
                                font.pixelSize: 14
                                color: isDarkTheme ? Theme.textPrimary : (darkMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: "Koyu"
                                font.pixelSize: 13
                                font.weight: isDarkTheme ? Font.DemiBold : Font.Medium
                                color: isDarkTheme ? Theme.textPrimary : Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: darkMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: SettingsManager.isDarkMode = true
                        }
                    }
                }
            }
        }
    }

    // ===== TOGGLE SETTING =====
    component ToggleSetting: Item {
        id: toggleRoot
        property string title: ""
        property string description: ""
        property bool checked: false
        signal toggled()

        activeFocusOnTab: true
        Keys.onReturnPressed: toggled()
        Keys.onSpacePressed: toggled()

        Accessible.role: Accessible.CheckBox
        Accessible.name: title
        Accessible.description: description
        Accessible.checked: checked
        Accessible.onToggleAction: toggled()

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        Rectangle {
            anchors.fill: parent
            color: toggleMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.02) : "transparent"
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: title
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    text: description
                    font.pixelSize: 13
                    color: Theme.textMuted
                }
            }

            // Animated Toggle Switch
            Rectangle {
                id: toggleTrack
                Layout.preferredWidth: Dimensions.toggleWidth
                Layout.preferredHeight: Dimensions.toggleHeight
                radius: Dimensions.toggleRadius
                color: checked ? Theme.primary : Qt.rgba(1, 1, 1, 0.1)

                // Hover/focus glow border
                property bool showGlow: toggleMouse.containsMouse || toggleRoot.activeFocus
                border.color: showGlow
                    ? (checked ? Theme.withAlpha(Theme.primary, 0.6) : Qt.rgba(1, 1, 1, 0.3))
                    : "transparent"
                border.width: 1.5

                // Hover scale
                scale: toggleMouse.containsMouse ? 1.05 : 1.0

                Behavior on color {
                    ColorAnimation {
                        duration: disableAnimations ? 0 : 200
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                // Handle
                Rectangle {
                    id: toggleHandle
                    width: Dimensions.toggleKnobSize
                    height: Dimensions.toggleKnobSize
                    radius: Dimensions.toggleKnobRadius
                    color: "white"
                    x: checked ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter

                    // Handle shadow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: Dimensions.radiusStandard
                        color: "transparent"
                        border.color: Qt.rgba(0, 0, 0, 0.15)
                        border.width: 1
                        z: -1
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: disableAnimations ? 0 : 200
                            easing.type: Easing.OutCubic
                        }
                    }

                    scale: toggleMouse.pressed ? 0.85 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                }

                MouseArea {
                    id: toggleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: toggled()
                }
            }
        }
    }

    // ===== INFO SETTING WITH BADGE =====
    component InfoSettingWithBadge: Item {
        property string title: ""
        property string description: ""
        property string badgeText: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: title
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    text: description
                    font.pixelSize: 13
                    color: Theme.textMuted
                }
            }

            Rectangle {
                Layout.preferredWidth: Math.max(badgeLabel.width + 28, 70)
                Layout.preferredHeight: 32
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.primary, 0.15)

                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: badgeText
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: Theme.primary
                    elide: Text.ElideRight
                }
            }
        }
    }

    // ===== DISABLED SETTING =====
    component DisabledSetting: Item {
        property string title: ""
        property string description: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    spacing: 8

                    Label {
                        text: title
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: Theme.textMuted
                    }

                    Rectangle {
                        Layout.preferredWidth: yakindaLabel.width + 16
                        Layout.preferredHeight: 20
                        radius: Dimensions.radiusStandard
                        color: Qt.rgba(1, 1, 1, 0.08)

                        Label {
                            id: yakindaLabel
                            anchors.centerIn: parent
                            text: "Yakında"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: Theme.textMuted
                        }
                    }
                }

                Label {
                    text: description
                    font.pixelSize: 13
                    color: Theme.withAlpha(Theme.textMuted, 0.7)
                }
            }

            Label {
                text: "🔒"  // Lock icon
                font.pixelSize: 20
                color: Theme.textMuted
            }
        }
    }

    // ===== DEV BUTTON =====
    component DevButton: Item {
        property string title: ""
        property string subtitle: ""
        property string icon: ""
        property bool isLoading: false
        property bool isDestructive: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        Rectangle {
            anchors.fill: parent
            color: devMouse.containsMouse && !isLoading ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

            Behavior on color { ColorAnimation { duration: 150 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 16

                // Icon container
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Dimensions.radiusStandard
                    color: isDestructive
                         ? Theme.withAlpha(Theme.error, 0.1)
                         : Qt.rgba(1, 1, 1, 0.06)

                    scale: devMouse.pressed ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }

                    Label {
                        anchors.centerIn: parent
                        text: icon
                        font.pixelSize: 18
                        color: isDestructive ? Theme.error : Qt.rgba(1, 1, 1, 0.7)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: title
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: isDestructive ? Theme.error : Theme.textPrimary
                    }

                    Label {
                        text: subtitle
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                }

                Item {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24

                    // Loading indicator
                    BusyIndicator {
                        anchors.fill: parent
                        visible: isLoading
                        running: isLoading
                    }

                    // Chevron
                    Label {
                        anchors.centerIn: parent
                        visible: !isLoading
                        text: "›"
                        font.pixelSize: 20
                        color: Theme.textMuted
                    }
                }
            }

            MouseArea {
                id: devMouse
                anchors.fill: parent
                enabled: !isLoading
                hoverEnabled: true
                cursorShape: isLoading ? Qt.ArrowCursor : Qt.PointingHandCursor
                onClicked: parent.parent.clicked()
            }
        }
    }

    // ===== INFO ROW =====
    component InfoRow: Item {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 60

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            Label {
                text: label
                font.pixelSize: 14
                color: Theme.textMuted
            }

            Item { Layout.fillWidth: true }

            Label {
                text: value
                font.pixelSize: 14
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

    // ===== CLICKABLE ROW =====
    component ClickableRow: Item {
        property string title: ""
        property string subtitle: ""
        property string icon: ""
        property bool isDestructive: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 80

        Rectangle {
            anchors.fill: parent
            color: rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

            Behavior on color { ColorAnimation { duration: 150 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 16

                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Dimensions.radiusStandard
                    color: isDestructive
                         ? Theme.withAlpha(Theme.error, 0.1)
                         : Qt.rgba(1, 1, 1, 0.06)

                    scale: rowMouse.pressed ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }

                    Label {
                        anchors.centerIn: parent
                        text: icon
                        font.pixelSize: 20
                        color: isDestructive ? Theme.error : Theme.textMuted
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: title
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: isDestructive ? Theme.error : Theme.textPrimary
                    }

                    Label {
                        text: subtitle
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                }

                Label {
                    text: "›"
                    font.pixelSize: 24
                    color: Theme.textMuted
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.parent.clicked()
            }
        }
    }
}
