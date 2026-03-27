import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsScreen.qml
 *
 * Structure:
 * - 280px sidebar (SettingsSidebar)
 * - Content area with 5 categories loaded via Loader
 */
Item {
    id: root

    signal back()

    property int selectedCategory: 0

    // Called from Main.qml when Settings becomes visible
    function resetScroll() {
        pageContainer.updateHeight()
        _playEntryAnim()
    }

    function _playEntryAnim() {
        settingsEntryAnim.stop()
        contentEntryAnim.stop()
        sidebar.opacity = 0; sidebarTranslate.x = -40
        contentArea.opacity = 0; contentAreaTranslate.y = 20
        pageContainer.opacity = 1; contentTranslate.y = 0
        settingsEntryAnim.start()
    }

    // Static categories + dynamic plugin categories
    readonly property var _staticCategories: [
        { name: qsTr("Genel"), description: qsTr("Uygulama genel ayarlarını yapılandırın"), isPlugin: false },
        { name: qsTr("Ekran"), description: qsTr("Tema, animasyon ve görüntü ayarları"), isPlugin: false },
        { name: qsTr("Eklentiler"), description: qsTr("Eklenti yönetimi ve mağaza"), isPlugin: false }
    ]
    readonly property var _staticCategoriesEnd: [
        { name: qsTr("Hakkında"), description: qsTr("Uygulama hakkında bilgiler"), isPlugin: false }
    ]

    // Discover loaded plugins with settings — delegates to PluginManager C++
    readonly property var _pluginCategories: {
        if (!devToolsEnabled || !PluginManager) return []
        return PluginManager.settingsCategories()
    }

    // Plugin pages go AFTER Hakkında (at the very bottom) — Eklentiler hidden in release
    property var categories: {
        var base = devToolsEnabled ? _staticCategories : _staticCategories.filter(function(c) { return c.name !== qsTr("Eklentiler") })
        return base.concat(_staticCategoriesEnd).concat(_pluginCategories)
    }

    readonly property var _staticSources: [
        "screens/settings/GeneralSettings.qml",
        "screens/settings/DisplaySettings.qml",
        "screens/settings/PluginsSettings.qml"
    ]
    readonly property var _staticSourcesEnd: [
        "screens/settings/AboutSettings.qml"
    ]

    readonly property var panelSources: {
        var base = devToolsEnabled ? _staticSources : _staticSources.filter(function(s) { return s.indexOf("Plugins") === -1 })
        var sources = base.concat(_staticSourcesEnd)
        for (var i = 0; i < _pluginCategories.length; i++)
            sources.push("screens/settings/PluginSettingsPage.qml")
        return sources
    }

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
                opacity: 0
                transform: Translate { id: sidebarTranslate; x: -40 }
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
                opacity: 0
                transform: Translate { id: contentAreaTranslate; y: 20 }

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

                            // Height tracks the active page's content.
                            // Use a Timer to re-evaluate after layout settles (fixes
                            // first-visit scroll — ColumnLayout needs a frame to compute).
                            implicitHeight: _measuredHeight
                            property real _measuredHeight: 200

                            Timer {
                                id: heightFixTimer
                                interval: 50
                                onTriggered: {
                                    var loader = pageRepeater.itemAt(root.selectedCategory)
                                    if (loader && loader.item)
                                        pageContainer._measuredHeight = loader.item.implicitHeight
                                }
                            }

                            function updateHeight() {
                                var loader = pageRepeater.itemAt(root.selectedCategory)
                                if (loader && loader.item) {
                                    _measuredHeight = loader.item.implicitHeight
                                    // Also schedule a delayed re-measure for layout settling
                                    heightFixTimer.restart()
                                }
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

                            // Track visited categories (dynamic length)
                            property var _visited: {
                                var v = [true] // first page always visited
                                for (var i = 1; i < root.panelSources.length; i++) v.push(false)
                                return v
                            }

                            // Pages load on first visit — no eager preload needed

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

                                    pageContainer.updateHeight()

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
                                        // Set pluginId for plugin settings pages
                                        var cat = root.categories[index]
                                        if (cat && cat.isPlugin && item && "pluginId" in item)
                                            item.pluginId = cat.pluginId

                                        if (root.selectedCategory === index) {
                                            pageContainer.updateHeight()
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

    // ===== ENTRY ANIMATION =====
    ParallelAnimation {
        id: settingsEntryAnim

        // Sidebar — slide from left
        NumberAnimation { target: sidebar; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        NumberAnimation { target: sidebarTranslate; property: "x"; from: -40; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }

        // Content area — delay 100ms, fade + slide up
        SequentialAnimation {
            PauseAnimation { duration: 100 }
            NumberAnimation { target: contentArea; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 100 }
            NumberAnimation { target: contentAreaTranslate; property: "y"; from: 20; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
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
        onConfirmed: {
            SettingsManager.clearCache()
            ManifestSync.syncCatalog()
        }
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
