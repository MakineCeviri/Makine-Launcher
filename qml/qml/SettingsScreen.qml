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
        { name: qsTr("Genel"), description: qsTr("Uygulama genel ayarlarını yapılandırın") },
        { name: qsTr("Çeviri"), description: qsTr("Çeviri tercihlerini ve dil ayarlarını düzenleyin") },
        { name: qsTr("Projeler"), description: qsTr("Tamamlanan çeviriler ve yedekler") },
        { name: qsTr("Performans"), description: qsTr("Performans ve kaynak kullanım ayarları") },
        { name: qsTr("Hakkında"), description: qsTr("Uygulama hakkında bilgiler") },
        { name: qsTr("Geliştirici"), description: qsTr("Geliştirici araçları ve test özellikleri") }
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
            duration: Dimensions.fadeTransitionDuration
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
            duration: Dimensions.fadeTransitionDuration
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
                            spacing: Dimensions.spacingXL

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

                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast; easing.type: Easing.OutCubic } }

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Back")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: root.back()
                                Keys.onSpacePressed: root.back()

                                Label {
                                    anchors.centerIn: parent
                                    text: "\u2190"  // Left arrow
                                    font.pixelSize: Dimensions.fontTitle
                                    color: backMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
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
                                text: qsTr("Ayarlar")
                                font.pixelSize: Dimensions.headlineLarge
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
                        font.pixelSize: Dimensions.fontSM
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
                        spacing: Dimensions.spacingXL

                        Item { Layout.preferredHeight: 32 }

                        // Category title with fade animation
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 32
                            Layout.rightMargin: 32
                            spacing: Dimensions.spacingMD

                            Label {
                                text: categories[selectedCategory].name
                                font.pixelSize: Dimensions.fontHero
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true

                                opacity: 1.0
                                Behavior on text {
                                    SequentialAnimation {
                                        NumberAnimation { target: parent; property: "opacity"; to: 0; duration: Dimensions.animVeryFast }
                                        PropertyAction { }
                                        NumberAnimation { target: parent; property: "opacity"; to: 1; duration: Dimensions.animFast }
                                    }
                                }
                            }

                            Label {
                                text: categories[selectedCategory].description
                                font.pixelSize: Dimensions.fontMD
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
                                    duration: Dimensions.transitionDuration
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
            spacing: Dimensions.spacingXL

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
                        title: qsTr("Otomatik Oyun Tespiti")
                        description: qsTr("Oyunları otomatik olarak tespit et")
                        checked: autoDetectGames
                        onToggled: autoDetectGames = !autoDetectGames
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: qsTr("Windows ile Başlat")
                        description: qsTr("Bilgisayar açıldığında otomatik başlat")
                        checked: startWithWindows
                        onToggled: startWithWindows = !startWithWindows
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: qsTr("Sistem Tepsisine Küçült")
                        description: qsTr("Kapatıldığında arka planda çalışır")
                        checked: minimizeToTray
                        onToggled: minimizeToTray = !minimizeToTray
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: qsTr("Bildirimler")
                        description: qsTr("Oyun tespit edildiğinde bildirim göster")
                        checked: showNotifications
                        onToggled: showNotifications = !showNotifications
                    }
                }
            }

            // Planned features
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DisabledSetting {
                        title: qsTr("İndirme Dizini")
                        description: qsTr("Çeviri paketlerinin indirileceği konum")
                    }

                    SettingsDivider {}

                    DisabledSetting {
                        title: qsTr("Önbellek Yönetimi")
                        description: qsTr("Önbellek boyutunu görüntüle ve temizle")
                    }

                    SettingsDivider {}

                    DisabledSetting {
                        title: qsTr("Proxy Ayarları")
                        description: qsTr("Ağ bağlantısı için proxy yapılandırması")
                    }
                }
            }
        }
    }

    // ===== TRANSLATION SETTINGS =====
    Component {
        id: translationSettings

        ColumnLayout {
            spacing: Dimensions.spacingXL

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    InfoSettingWithBadge {
                        title: qsTr("Çeviri Dili")
                        description: qsTr("Oyunların çevrileceği dil")
                        badgeText: qsTr("Türkçe")
                    }
                }
            }

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DisabledSetting {
                        title: qsTr("Çeviri Kalitesi")
                        description: qsTr("Bu özellik gelecek güncellemelerde eklenecektir")
                    }
                }
            }
        }
    }

    // ===== PROJECTS SETTINGS - Tamamlanan çeviriler ve yedekler =====
    Component {
        id: projectsSettings

        ColumnLayout {
            spacing: Dimensions.spacingXL

            // Backup list card
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingLG

                    // Section header
                    RowLayout {
                        spacing: Dimensions.spacingMD

                        Text {
                            text: qsTr("Yedekler")
                            font.pixelSize: Dimensions.fontLG
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
                                font.pixelSize: Dimensions.fontXS
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
                            spacing: Dimensions.spacingLG

                            Text {
                                text: "📁"
                                font.pixelSize: Dimensions.headlineLarge
                            }

                            Text {
                                text: qsTr("Henüz yedeklenmiş oyun yok")
                                font.pixelSize: Dimensions.fontMD
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

                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: Dimensions.spacingLG

                                // Game icon placeholder
                                Rectangle {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48
                                    radius: Dimensions.radiusStandard
                                    color: Theme.surfaceActive

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.gameName ? modelData.gameName.substring(0, 2).toUpperCase() : "?"
                                        font.pixelSize: Dimensions.fontLG
                                        font.weight: Font.Bold
                                        color: Theme.textMuted
                                    }
                                }

                                // Backup info
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingXXS

                                    Text {
                                        text: modelData.gameName || qsTr("Bilinmeyen Oyun")
                                        font.pixelSize: Dimensions.fontMD
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
                                        font.pixelSize: Dimensions.fontSM
                                        color: Theme.textMuted
                                    }
                                }

                                // Restore button
                                Rectangle {
                                    Layout.preferredWidth: restoreBtnContent.width + 20
                                    Layout.preferredHeight: 32
                                    radius: Dimensions.radiusStandard
                                    color: restoreBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Restore backup")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: BackupManager.restoreBackup(modelData.id, modelData.originalPath)
                                    Keys.onSpacePressed: BackupManager.restoreBackup(modelData.id, modelData.originalPath)

                                    Row {
                                        id: restoreBtnContent
                                        anchors.centerIn: parent
                                        spacing: Dimensions.spacingSM

                                        Text {
                                            text: "↩"
                                            font.pixelSize: Dimensions.fontSM
                                            color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: qsTr("Geri Al")
                                            font.pixelSize: Dimensions.fontSM
                                            font.weight: Font.Medium
                                            color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
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

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Delete backup")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: BackupManager.deleteBackup(modelData.id)
                                    Keys.onSpacePressed: BackupManager.deleteBackup(modelData.id)

                                    Text {
                                        anchors.centerIn: parent
                                        text: "🗑"
                                        font.pixelSize: Dimensions.fontMD
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
                                        id: deleteBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            BackupManager.deleteBackup(modelData.id)
                                        }
                                    }

                                    ToolTip {
                                        visible: deleteBtnMouse.containsMouse
                                        text: qsTr("Yedeği sil")
                                        delay: 500
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
                    spacing: Dimensions.spacingLG

                    BusyIndicator {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        running: BackupManager.isRestoring
                    }

                    Text {
                        Layout.fillWidth: true
                        text: BackupManager.restoreStatus || qsTr("Geri yükleniyor...")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // ===== PERFORMANCE SETTINGS =====
    Component {
        id: performanceSettings

        ColumnLayout {
            spacing: Dimensions.spacingXL

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ToggleSetting {
                        title: qsTr("Donanım Hızlandırma")
                        description: qsTr("GPU kullanarak daha hızlı çeviri")
                        checked: hardwareAcceleration
                        onToggled: hardwareAcceleration = !hardwareAcceleration
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: qsTr("Global Önbellek")
                        description: qsTr("Çevirileri tüm oyunlar için paylaş")
                        checked: useGlobalCache
                        onToggled: useGlobalCache = !useGlobalCache
                    }

                    SettingsDivider {}

                    ToggleSetting {
                        title: qsTr("Uygulama Animasyonları")
                        description: qsTr("Arayüz animasyonlarını etkinleştir")
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
            spacing: Dimensions.spacingXL

            // App info card
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    InfoRow { label: qsTr("Uygulama"); value: Dimensions.appName }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Versiyon"); value: Dimensions.appVersionFull }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Geliştirici"); value: qsTr("MakineAI Ekibi") }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Lisans"); value: qsTr("Özel Lisans") }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Platform"); value: Qt.platform.os }
                    SettingsDivider {}
                    InfoRow { label: "Qt"; value: "6.10.1" }
                }
            }

            // Community links card
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ClickableRow {
                        title: qsTr("Discord Desteği")
                        subtitle: qsTr("Topluluk ve yardım için Discord sunucumuza katılın")
                        icon: "\uD83D\uDCAC"  // 💬
                        onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                    }

                    SettingsDivider {}

                    ClickableRow {
                        title: "GitHub"
                        subtitle: qsTr("Kaynak kodu, hata bildirimi ve katkılar")
                        icon: "\uD83D\uDD17"  // 🔗
                        onClicked: Qt.openUrlExternally("https://github.com/jlceaser/MakineAI")
                    }

                    SettingsDivider {}

                    ClickableRow {
                        title: qsTr("Geri Bildirim")
                        subtitle: qsTr("Hata bildirimi ve öneriler için web sitemizi ziyaret edin")
                        icon: "\uD83D\uDCE7"  // 📧
                        onClicked: Qt.openUrlExternally("https://makineai.com/feedback")
                    }
                }
            }

            // Open source licenses card
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    readonly property var licenseModel: [
                        { name: "Qt Framework", license: "LGPL v3", url: "https://www.qt.io/licensing" },
                        { name: "Boost", license: "BSL-1.0", url: "https://www.boost.org/LICENSE_1_0.txt" },
                        { name: "OpenSSL", license: "Apache-2.0", url: "https://www.openssl.org/source/license.html" },
                        { name: "spdlog", license: "MIT", url: "https://github.com/gabime/spdlog/blob/v1.x/LICENSE" },
                        { name: "nlohmann/json", license: "MIT", url: "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT" },
                        { name: "Inter Font", license: "OFL-1.1", url: "https://github.com/rsms/inter/blob/master/LICENSE.txt" }
                    ]

                    // Section header
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20

                            Label {
                                text: qsTr("Açık Kaynak Lisanslar")
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: licenseModel.length.toString()
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.Medium
                                color: Theme.textMuted
                            }
                        }
                    }

                    SettingsDivider {}

                    Repeater {
                        model: licenseModel

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            ClickableRow {
                                title: modelData.name
                                subtitle: modelData.license
                                icon: "\uD83D\uDCC4"  // 📄
                                onClicked: Qt.openUrlExternally(modelData.url)
                            }

                            SettingsDivider {
                                visible: index < licenseModel.length - 1
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== DEVELOPER SETTINGS =====
    Component {
        id: developerSettings

        ColumnLayout {
            spacing: Dimensions.spacingXL

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DevButton {
                        title: qsTr("Test Verisi Aktar")
                        subtitle: qsTr("Translation Memory'ye 30 test çevirisi ekle")
                        icon: "⬆"  // Upload icon
                        isLoading: isImporting
                        onClicked: {
                            isImporting = true
                            devStatus = qsTr("Test verisi aktarılıyor...")

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
                            devStatus = qsTr("%1 çeviri eklendi!").arg(added)
                        }
                    }

                    SettingsDivider {}

                    DevButton {
                        title: qsTr("TM'yi Temizle")
                        subtitle: qsTr("Tüm Translation Memory verilerini sil")
                        icon: "🗑"  // Delete icon
                        isDestructive: true
                        onClicked: {
                            devStatus = qsTr("Bu özellik henüz aktif değil")
                        }
                    }

                    SettingsDivider {}

                    DevButton {
                        title: qsTr("TM İstatistikleri")
                        subtitle: qsTr("Translation Memory durumunu göster")
                        icon: "📊"  // Analytics icon
                        onClicked: {
                            var terms = CoreBridge.getAllGlossaryTerms()
                            devStatus = qsTr("TM/Glossary İstatistikleri:") + "\n" +
                                       qsTr("Glossary Terimleri: %1").arg(terms.length) + "\n" +
                                       qsTr("Durum: Aktif")
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
                            font.pixelSize: Dimensions.fontSM
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

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast; easing.type: Easing.OutCubic } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            spacing: Dimensions.spacingLG

            // Selection indicator
            Rectangle {
                Layout.preferredWidth: 3
                Layout.preferredHeight: isSelected ? 20 : 0
                radius: 2
                color: isSelected ? Theme.textPrimary : "transparent"

                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                }
            }

            Label {
                text: name
                font.pixelSize: Dimensions.fontMD
                font.weight: isSelected ? Font.DemiBold : Font.Medium
                color: isSelected ? Theme.textPrimary
                     : catMouse.containsMouse ? Theme.textPrimary
                     : Theme.textSecondary

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
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
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Label {
                    text: qsTr("Tema")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    text: qsTr("Uygulama görünümünü seç")
                    font.pixelSize: Dimensions.fontBody
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
                    spacing: Dimensions.spacingXS

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

                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                        Accessible.role: Accessible.RadioButton
                        Accessible.name: qsTr("Light theme")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: SettingsManager.isDarkMode = false
                        Keys.onSpacePressed: SettingsManager.isDarkMode = false

                        Row {
                            id: lightRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM

                            Label {
                                text: "☀"  // Sun icon
                                font.pixelSize: Dimensions.fontMD
                                color: !isDarkTheme ? Theme.textPrimary : (lightMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: qsTr("Açık")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: !isDarkTheme ? Font.DemiBold : Font.Medium
                                color: !isDarkTheme ? Theme.textPrimary : (lightMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: lightThemeBtn.activeFocus
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

                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                        Accessible.role: Accessible.RadioButton
                        Accessible.name: qsTr("Dark theme")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: SettingsManager.isDarkMode = true
                        Keys.onSpacePressed: SettingsManager.isDarkMode = true

                        Row {
                            id: darkRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM

                            Label {
                                text: "🌙"  // Moon icon
                                font.pixelSize: Dimensions.fontMD
                                color: isDarkTheme ? Theme.textPrimary : (darkMouse.containsMouse ? Theme.textSecondary : Theme.textMuted)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: qsTr("Koyu")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: isDarkTheme ? Font.DemiBold : Font.Medium
                                color: isDarkTheme ? Theme.textPrimary : Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: darkThemeBtn.activeFocus
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
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Label {
                    Layout.fillWidth: true
                    text: title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: description
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                    elide: Text.ElideRight
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
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

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
                    Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }
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
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Label {
                    text: title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Label {
                    text: description
                    font.pixelSize: Dimensions.fontBody
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
                    font.pixelSize: Dimensions.fontBody
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
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                RowLayout {
                    spacing: Dimensions.spacingMD

                    Label {
                        text: title
                        font.pixelSize: Dimensions.fontMD
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
                            text: qsTr("Yakında")
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.DemiBold
                            color: Theme.textMuted
                        }
                    }
                }

                Label {
                    text: description
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.withAlpha(Theme.textMuted, 0.7)
                }
            }

            Label {
                text: "🔒"  // Lock icon
                font.pixelSize: Dimensions.fontXL
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

        Accessible.role: Accessible.Button
        Accessible.name: title
        activeFocusOnTab: true
        Keys.onReturnPressed: { if (!isLoading) clicked() }
        Keys.onSpacePressed: { if (!isLoading) clicked() }

        Rectangle {
            anchors.fill: parent
            color: devMouse.containsMouse && !isLoading ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: Dimensions.spacingXL

                // Icon container
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Dimensions.radiusStandard
                    color: isDestructive
                         ? Theme.withAlpha(Theme.error, 0.1)
                         : Qt.rgba(1, 1, 1, 0.06)

                    scale: devMouse.pressed ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                    Label {
                        anchors.centerIn: parent
                        text: icon
                        font.pixelSize: Dimensions.fontTitle
                        color: isDestructive ? Theme.error : Qt.rgba(1, 1, 1, 0.7)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXS

                    Label {
                        text: title
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: isDestructive ? Theme.error : Theme.textPrimary
                    }

                    Label {
                        text: subtitle
                        font.pixelSize: Dimensions.fontBody
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
                        font.pixelSize: Dimensions.fontXL
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
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted
            }

            Item { Layout.fillWidth: true }

            Label {
                text: value
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

}
