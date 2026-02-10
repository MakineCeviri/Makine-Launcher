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
        { name: qsTr("Yedekler"), description: qsTr("Tamamlanan çeviriler ve yedekler") },
        { name: qsTr("Performans"), description: qsTr("Performans ve kaynak kullanım ayarları") },
        { name: qsTr("Hakkında"), description: qsTr("Uygulama hakkında bilgiler") },
        { name: qsTr("Geliştirici"), description: qsTr("Geliştirici araçları ve test özellikleri") }
    ]

    // Settings state - bound to SettingsManager
    property bool autoDetectGames: SettingsManager.autoDetectGames
    property bool startWithWindows: SettingsManager.startWithWindows
    property bool minimizeToTray: SettingsManager.minimizeToTray
    property bool disableAnimations: !SettingsManager.enableAnimations

    // Update SettingsManager when properties change
    onAutoDetectGamesChanged: SettingsManager.autoDetectGames = autoDetectGames
    onStartWithWindowsChanged: SettingsManager.startWithWindows = startWithWindows
    onMinimizeToTrayChanged: SettingsManager.minimizeToTray = minimizeToTray
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations


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
                x: 0
                color: Theme.withAlpha(Theme.surface, 0.5)

                // Right border
                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Header
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Dimensions.navbarHeight

                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Dimensions.marginML
                            text: qsTr("Ayarlar")
                            font.pixelSize: Dimensions.headlineLarge
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.withAlpha(Theme.textPrimary, 0.06)
                    }

                    Item { Layout.preferredHeight: 4 }

                    // Category list
                    Repeater {
                        model: categories

                        CategoryItem {
                            Layout.fillWidth: true
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
                        Layout.leftMargin: Dimensions.marginML
                        Layout.bottomMargin: Dimensions.marginML
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
                            color: parent.pressed ? Theme.withAlpha(Theme.textPrimary, 0.25) : Theme.withAlpha(Theme.textPrimary, 0.12)
                        }
                    }

                    ColumnLayout {
                        width: settingsScrollView.availableWidth
                        spacing: Dimensions.spacingXL

                        Item { Layout.preferredHeight: 32 }

                        // Category title
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXL
                            Layout.rightMargin: Dimensions.marginXL
                            spacing: Dimensions.spacingMD

                            Label {
                                text: categories[selectedCategory].name
                                font.pixelSize: Dimensions.fontHero
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
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
                            Layout.leftMargin: Dimensions.marginXL
                            Layout.rightMargin: Dimensions.marginXL
                            Layout.preferredWidth: Math.min(settingsScrollView.availableWidth - 64, 640)

                            sourceComponent: {
                                switch(selectedCategory) {
                                    case 0: return generalSettings
                                    case 1: return translationSettings
                                    case 2: return projectsSettings
                                    case 3: return performanceSettings
                                    case 4: return aboutSettings
                                    case 5: return developerSettings
                                    default: return null
                                }
                            }

                            // Reset scroll on category change
                            Connections {
                                target: root
                                function onSelectedCategoryChanged() {
                                    settingsScrollView.ScrollBar.vertical.position = 0
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

                }
            }

            // Planned features
            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DisabledSetting {
                        title: qsTr("Bildirimler")
                        description: qsTr("Oyun tespit edildiğinde bildirim göster")
                    }

                    SettingsDivider {}

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
                        color: Theme.withAlpha(Theme.textPrimary, 0.03)
                        visible: BackupManager.backups.length === 0

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingLG

                            Text {
                                text: "📁"
                                font.pixelSize: Dimensions.headlineLarge
                            }

                            Text {
                                text: qsTr("Henüz yedeklenmiş çeviri yok")
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
                            color: backupItemMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06) : Theme.withAlpha(Theme.textPrimary, 0.03)
                            border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Dimensions.marginMS
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
                        title: qsTr("Uygulama Animasyonları")
                        description: qsTr("Arayüz animasyonlarını etkinleştir")
                        checked: !disableAnimations
                        onToggled: disableAnimations = !disableAnimations
                    }
                }
            }

            SettingsCard {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DisabledSetting {
                        title: qsTr("Donanım Hızlandırma")
                        description: qsTr("GPU kullanarak daha hızlı çeviri işleme")
                    }

                    SettingsDivider {}

                    DisabledSetting {
                        title: qsTr("Global Önbellek")
                        description: qsTr("Çevirileri tüm oyunlar için paylaş")
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

                    InfoRow { label: qsTr("Versiyon"); value: Dimensions.appVersionFull }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Geliştirici"); value: qsTr("MakineAI Ekibi") }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Lisans"); value: qsTr("Ücretsiz Lisans") }
                    SettingsDivider {}
                    InfoRow { label: qsTr("Platform"); value: Qt.platform.os }
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
                    id: licensesSection
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
                            anchors.leftMargin: Dimensions.marginML
                            anchors.rightMargin: Dimensions.marginML

                            Label {
                                text: qsTr("Açık Kaynak Lisanslar")
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: licensesSection.licenseModel.length.toString()
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.Medium
                                color: Theme.textMuted
                            }
                        }
                    }

                    SettingsDivider {}

                    Repeater {
                        model: licensesSection.licenseModel

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Dimensions.marginML
                                    anchors.rightMargin: Dimensions.marginML

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        font.pixelSize: Dimensions.fontMD
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: modelData.license
                                        font.pixelSize: Dimensions.fontSM
                                        font.weight: Font.Medium
                                        color: Theme.textMuted
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally(modelData.url)
                                }
                            }

                            SettingsDivider {
                                visible: index < licensesSection.licenseModel.length - 1
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

                    DisabledSetting {
                        title: qsTr("Translation Memory")
                        description: qsTr("Çeviri belleği test verisi aktarma ve yönetim araçları")
                    }

                    SettingsDivider {}

                    DisabledSetting {
                        title: qsTr("Glossary Yönetimi")
                        description: qsTr("Terim sözlüğü görüntüleme ve düzenleme")
                    }

                    SettingsDivider {}

                    DisabledSetting {
                        title: qsTr("Adaptasyon Motoru")
                        description: qsTr("Güncelleme tespiti ve otomatik uyarlama araçları")
                    }
                }
            }
        }
    }

    // ===== CATEGORY ITEM COMPONENT (W10 sharp flat) =====
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

        height: 36
        radius: 0
        color: isSelected
            ? Theme.withAlpha(Theme.textPrimary, 0.08)
            : catMouse.pressed
                ? Theme.withAlpha(Theme.textPrimary, 0.06)
                : catMouse.containsMouse
                    ? Theme.withAlpha(Theme.textPrimary, 0.04)
                    : "transparent"

        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Dimensions.marginML
            text: name
            font.pixelSize: Dimensions.fontMD
            font.weight: isSelected ? Font.DemiBold : Font.Normal
            color: isSelected ? Theme.textPrimary
                 : catMouse.containsMouse ? Theme.textPrimary
                 : Theme.textSecondary
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
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
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
        color: Theme.withAlpha(Theme.textPrimary, 0.04)
    }

    // ===== THEME SETTING =====
    component ThemeSetting: Item {
        property bool isDarkTheme: SettingsManager.isDarkMode

        Layout.fillWidth: true
        Layout.preferredHeight: 72

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Tema")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Uygulama görünümünü seç")
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }

            // Theme selector
            Rectangle {
                Layout.preferredWidth: themeRow.width + 8
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.textPrimary, 0.06)

                Row {
                    id: themeRow
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingXS

                    // Light theme — disabled (Yakında)
                    Rectangle {
                        width: lightRow.width + 28
                        height: 32
                        radius: Dimensions.radiusStandard
                        color: "transparent"
                        opacity: 0.4

                        Row {
                            id: lightRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM

                            Label {
                                text: qsTr("Açık")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: Font.Medium
                                color: Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: yakindaLbl.width + 10
                                height: 16
                                radius: 8
                                color: Theme.withAlpha(Theme.textPrimary, 0.1)
                                anchors.verticalCenter: parent.verticalCenter

                                Label {
                                    id: yakindaLbl
                                    anchors.centerIn: parent
                                    text: qsTr("Yakında")
                                    font.pixelSize: Dimensions.fontCaption
                                    font.weight: Font.DemiBold
                                    color: Theme.textMuted
                                }
                            }
                        }
                    }

                    // Dark theme — active
                    Rectangle {
                        id: darkThemeBtn
                        width: darkRow.width + 28
                        height: 32
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(Theme.textPrimary, 0.12)
                        border.color: Theme.withAlpha(Theme.textPrimary, 0.2)
                        border.width: 1

                        Row {
                            id: darkRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM

                            Label {
                                text: qsTr("Koyu")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
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
            color: toggleMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.02) : "transparent"
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
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
                color: checked ? Theme.primary : Theme.withAlpha(Theme.textPrimary, 0.1)

                // Hover/focus glow border
                property bool showGlow: toggleMouse.containsMouse || toggleRoot.activeFocus
                border.color: showGlow
                    ? (checked ? Theme.withAlpha(Theme.primary, 0.6) : Theme.withAlpha(Theme.textPrimary, 0.3))
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
                        border.color: Theme.withAlpha(Theme.bgPrimary, 0.15)
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
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
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

            Rectangle {
                Layout.preferredWidth: Math.max(badgeLabel.width + 28, 70)
                Layout.preferredHeight: 28
                radius: 14
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
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
            spacing: Dimensions.spacingXL

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS

                Label {
                    text: title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textMuted
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Label {
                    text: description
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.withAlpha(Theme.textMuted, 0.7)
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            // Right-aligned "Yakında" badge
            Rectangle {
                Layout.preferredWidth: _yakindaLbl.width + 24
                Layout.preferredHeight: 28
                radius: 14
                color: Theme.withAlpha(Theme.textPrimary, 0.08)

                Label {
                    id: _yakindaLbl
                    anchors.centerIn: parent
                    text: qsTr("Yakında")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }
        }
    }

    // ===== INFO ROW =====
    component InfoRow: Item {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML

            Label {
                Layout.fillWidth: true
                text: label
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted
                elide: Text.ElideRight
            }

            Label {
                text: value
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

}
