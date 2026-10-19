// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
        // Always reopen on the "Genel" tab — don't resume the last category
        root.selectedCategory = 0
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

    // Enabled plugins with settings or actions — delegates to PluginManager C++.
    // settingsCategories() has no change signal, so it is re-read on pluginsChanged
    // and replaced only when the entries differ (see _refreshPluginCategories).
    property var _pluginCategories: []

    // The "Eklentiler" page; plugin pages only exist when dev tools are on.
    readonly property int _pluginsPageIndex: 2

    function _refreshPluginCategories() {
        var next = (devToolsEnabled && PluginManager) ? PluginManager.settingsCategories() : []
        if (JSON.stringify(next) === JSON.stringify(_pluginCategories)) return

        var shown = categories[selectedCategory]
        _pluginCategories = next

        // The page on screen was a plugin's: follow it to its new index, or fall
        // back to the plugin list when the plugin was disabled or removed.
        if (shown && shown.isPlugin) {
            var index = _pluginsPageIndex
            for (var i = 0; i < categories.length; i++)
                if (categories[i].pluginId === shown.pluginId) index = i
            selectedCategory = index
        }
    }

    function _pluginIdAt(i) {
        var cat = categories[i]
        return cat && cat.isPlugin ? cat.pluginId : ""
    }

    Component.onCompleted: _refreshPluginCategories()

    Connections {
        target: PluginManager
        function onPluginsChanged() { root._refreshPluginCategories() }
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

    // The category on screen; a plugin's page can vanish a moment before the selection moves.
    readonly property var _current: categories[selectedCategory] || categories[0]

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
                                text: root._current.name
                                font.pixelSize: Dimensions.fontHero
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: root._current.description
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

                            // One slot per page. Plugin pages come and go at the end, and
                            // appending or removing slots leaves the other pages alone (a
                            // Repeater over a plain number would rebuild every page).
                            ListModel { id: pageSlots }

                            function syncPageSlots() {
                                var n = root.panelSources.length
                                while (pageSlots.count < n) pageSlots.append({ slot: pageSlots.count })
                                while (pageSlots.count > n) pageSlots.remove(pageSlots.count - 1)
                            }

                            Connections {
                                target: root
                                function onPanelSourcesChanged() { pageContainer.syncPageSlots() }
                            }

                            Component.onCompleted: syncPageSlots()

                            Repeater {
                                id: pageRepeater
                                model: pageSlots

                                Loader {
                                    id: pageLoader
                                    required property int index

                                    anchors.left: parent ? parent.left : undefined
                                    anchors.right: parent ? parent.right : undefined
                                    y: 0

                                    active: !!pageContainer._visited[index]
                                    visible: root.selectedCategory === index
                                    asynchronous: index > 0
                                    source: root.panelSources[index] || ""

                                    // Plugin pages share one source; which plugin they show follows the category.
                                    Binding {
                                        target: pageLoader.item
                                        property: "pluginId"
                                        value: root._pluginIdAt(pageLoader.index)
                                        when: pageLoader.item !== null && root._pluginIdAt(pageLoader.index) !== ""
                                    }

                                    onLoaded: {
                                        // A page's height follows its content (lists load, details open).
                                        if (item)
                                            item.implicitHeightChanged.connect(function() {
                                                if (root.selectedCategory === pageLoader.index)
                                                    pageContainer.updateHeight()
                                            })

                                        // Wire GeneralSettings (page 0) signals here.
                                        // itemAt() is not bindable, so the old
                                        // Connections target never resolved and the
                                        // "Ayarları Sıfırla" button was dead.
                                        if (index === 0 && item) {
                                            item.clearCacheRequested.connect(function() { clearCacheConfirm.open() })
                                            item.resetSettingsRequested.connect(function() { resetSettingsConfirm.open() })
                                        }

                                        if (root.selectedCategory === index) {
                                            pageContainer.updateHeight()
                                            contentEntryAnim.restart()
                                            if (typeof SceneProfiler !== "undefined")
                                                SceneProfiler.endInteraction()
                                        }
                                    }
                                }
                            }

                            // GeneralSettings (page 0) signals are connected in the
                            // Loader's onLoaded above — itemAt() is not bindable.
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
