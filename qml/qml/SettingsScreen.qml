import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

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
                                textFormat: Text.PlainText
                                text: categories[selectedCategory].name
                                font.pixelSize: Dimensions.fontHero
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: categories[selectedCategory].description
                                font.pixelSize: Dimensions.fontMD
                                color: Theme.textMuted
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        // Settings content — cached pages (load once, keep alive)
                        Item {
                            id: pageContainer
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXL
                            Layout.rightMargin: Dimensions.marginXL
                            Layout.preferredWidth: Math.min(settingsScrollView.availableWidth - 64, 640)

                            // Height tracks the active page's content
                            implicitHeight: {
                                var loader = pageRepeater.itemAt(root.selectedCategory)
                                return (loader && loader.item) ? loader.item.implicitHeight : 200
                            }

                            // Entry animation (same visual as before)
                            opacity: 0
                            transform: Translate { id: contentTranslate; y: 12 }
                            ParallelAnimation {
                                id: contentEntryAnim
                                NumberAnimation {
                                    target: pageContainer; property: "opacity"
                                    from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic
                                }
                                NumberAnimation {
                                    target: contentTranslate; property: "y"
                                    from: 12; to: 0; duration: 220; easing.type: Easing.OutCubic
                                }
                            }

                            // Track visited categories
                            property var _visited: [true, false, false, false, false, false]

                            // Preload all pages shortly after Settings loads
                            Timer {
                                interval: 500; running: true
                                onTriggered: pageContainer._visited = [true, true, true, true, true, true]
                            }

                            // Category change handler
                            Connections {
                                target: root
                                function onSelectedCategoryChanged() {
                                    settingsScrollView.ScrollBar.vertical.position = 0
                                    pageContainer.opacity = 0
                                    contentTranslate.y = 12

                                    // Activate page on first visit
                                    if (!pageContainer._visited[root.selectedCategory]) {
                                        var v = pageContainer._visited.slice()
                                        v[root.selectedCategory] = true
                                        pageContainer._visited = v
                                    }

                                    // If already loaded — animate immediately
                                    var loader = pageRepeater.itemAt(root.selectedCategory)
                                    if (loader && loader.status === Loader.Ready) {
                                        contentEntryAnim.restart()
                                    } else if (typeof SceneProfiler !== "undefined") {
                                        SceneProfiler.beginInteraction("settingsPageSwitch")
                                    }
                                }
                            }

                            Repeater {
                                id: pageRepeater
                                model: root.panelSources

                                Loader {
                                    required property int index
                                    required property string modelData

                                    anchors.left: parent ? parent.left : undefined
                                    anchors.right: parent ? parent.right : undefined
                                    y: 0

                                    active: pageContainer._visited[index]
                                    visible: root.selectedCategory === index
                                    asynchronous: index > 0
                                    source: modelData

                                    onLoaded: {
                                        if (root.selectedCategory === index) {
                                            contentEntryAnim.restart()
                                            if (typeof SceneProfiler !== "undefined")
                                                SceneProfiler.endInteraction()
                                        }
                                    }
                                }
                            }

                            // Route GeneralSettings signals (page 0)
                            Connections {
                                target: {
                                    var loader = pageRepeater.itemAt(0)
                                    return (loader && loader.item) ? loader.item : null
                                }
                                ignoreUnknownSignals: true
                                function onClearCacheRequested() { clearCacheConfirm.open() }
                                function onResetSettingsRequested() { resetSettingsConfirm.open() }
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
