import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SettingsScreen.qml
 *
 * Structure:
 * - 280px sidebar (SettingsSidebar)
 * - Content area with 6 categories loaded via Loader
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

    readonly property var panelSources: [
        "screens/settings/GeneralSettings.qml",
        "screens/settings/TranslationSettings.qml",
        "screens/settings/BackupsSettings.qml",
        "screens/settings/PerformanceSettings.qml",
        "screens/settings/AboutSettings.qml",
        "screens/settings/DeveloperSettings.qml"
    ]

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        Row {
            anchors.fill: parent
            spacing: 0

            // ===== LEFT SIDEBAR (280px) =====
            SettingsSidebar {
                id: sidebar
                height: parent.height
                selectedCategory: root.selectedCategory
                categories: root.categories
                onCategorySelected: function(index) {
                    root.selectedCategory = index
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

                    ScrollBar.vertical: StyledScrollBar {}

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

                            source: panelSources[selectedCategory] || ""

                            // Entry animation on load
                            opacity: 0
                            transform: Translate { id: contentTranslate; y: 12 }
                            onLoaded: contentEntryAnim.restart()

                            ParallelAnimation {
                                id: contentEntryAnim
                                NumberAnimation {
                                    target: contentLoader
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: 220
                                    easing.type: Easing.OutCubic
                                }
                                NumberAnimation {
                                    target: contentTranslate
                                    property: "y"
                                    from: 12; to: 0
                                    duration: 220
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // Reset scroll on category change
                            Connections {
                                target: root
                                function onSelectedCategoryChanged() {
                                    settingsScrollView.ScrollBar.vertical.position = 0
                                    contentLoader.opacity = 0
                                    contentTranslate.y = 12
                                }
                            }

                            // Connect GeneralSettings signals
                            Connections {
                                target: contentLoader.item
                                ignoreUnknownSignals: true
                                function onClearCacheRequested() {
                                    clearCacheConfirm.open()
                                }
                                function onResetSettingsRequested() {
                                    resetSettingsConfirm.open()
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 32 }
                    }
                }
            }
        }
    }

    // ===== CONFIRM DIALOGS =====
    ConfirmDialog {
        id: clearCacheConfirm
        parent: Overlay.overlay
        title: qsTr("Önbellek Temizle")
        message: qsTr("Uygulama önbellek dosyaları silinecek. İndirilen veriler etkilenmez.")
        confirmText: qsTr("Temizle")
        accentColor: Theme.warning
        onConfirmed: SettingsManager.clearCache()
    }

    ConfirmDialog {
        id: resetSettingsConfirm
        parent: Overlay.overlay
        title: qsTr("Ayarları Sıfırla")
        message: qsTr("Tüm ayarlar varsayılan değerlere döndürülecek. Bu işlem geri alınamaz.")
        confirmText: qsTr("Sıfırla")
        onConfirmed: SettingsManager.resetToDefaults()
    }
}
