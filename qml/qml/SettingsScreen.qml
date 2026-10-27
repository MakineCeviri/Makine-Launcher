// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
import "components"
pragma ComponentBehavior: Bound

/**
 * SettingsScreen.qml — "Ayarlar", laid out like a macOS settings window:
 * the title, a row of toolbar tabs (Genel · Görünüm · Eklentiler · Hakkında,
 * then enabled plugins), and the chosen page's grouped lists, each page
 * loaded on its first visit and kept.
 */
Item {
    id: root

    signal back()

    property int selectedCategory: 0

    // Called from Main.qml when Settings becomes visible: on the tab asked for (Makine's settings ask
    // for Görünüm), else always "Genel" — don't resume the last category
    function resetScroll(category) {
        root.selectedCategory = category > 0 && category < root.categories.length ? category : 0
        pageContainer.updateHeight()
        _playEntryAnim()
    }

    function _playEntryAnim() {
        contentEntryAnim.stop()
        pageContainer.opacity = 0; contentTranslate.y = 8
        contentEntryAnim.start()
    }

    // Static categories + dynamic plugin categories
    readonly property var _staticCategories: [
        { name: qsTr("Genel"), description: qsTr("Başlangıç, oyun tanıma, raporlar, güncellemeler ve önbellek."), isPlugin: false, icon: "settings" },
        { name: qsTr("Görünüm"), description: qsTr("Açık ya da koyu görünüm, vurgu rengi, hareket ve Makine."), isPlugin: false, icon: "palette" },
        { name: qsTr("Eklentiler"), description: qsTr("Eklentileri kur, aç, kapat ve güncelle."), isPlugin: false, icon: "puzzle" }
    ]
    readonly property var _staticCategoriesEnd: [
        { name: qsTr("Hakkında"), description: qsTr("Sürüm, yardım, kısayollar ve açık kaynak bileşenler."), isPlugin: false, icon: "info" }
    ]
    function _icon(cat) {
        switch (cat && cat.icon) {
        case "settings": return Icons.settings
        case "palette": return Icons.palette
        case "info": return Icons.info
        default: return Icons.puzzle
        }
    }

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

    Item {
        anchors.fill: parent

        ScrollView {
            id: settingsScrollView
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical: StyledScrollBar {}

            ColumnLayout {
                width: Math.min(settingsScrollView.availableWidth - Dimensions.pageMargin * 2, 720)
                x: (settingsScrollView.availableWidth - width) / 2
                spacing: 22

                PageHeader {
                    Layout.topMargin: 52
                    title: qsTr("Ayarlar")
                    subtitle: root._current.description
                }

                // Toolbar tabs: icon over label, centred as System Settings
                // centres its toolbar. One raised plate slides to the chosen
                // tab (a little past, then back); a tab's icon answers the
                // pointer and the choice (MotionIcon).
                Item {
                    id: tabBar
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: tabRow.implicitWidth
                    implicitHeight: tabRow.implicitHeight
                    property Item _sel: null

                    RectangularShadow {
                        anchors.fill: plate
                        visible: plate.visible
                        radius: plate.radius
                        offset.y: 4
                        blur: 16
                        spread: -4
                        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.45 : 0.12)
                    }
                    Rectangle {
                        id: plate
                        visible: tabBar._sel !== null
                        x: tabBar._sel ? tabBar._sel.x : 0
                        width: tabBar._sel ? tabBar._sel.width : 0
                        height: tabRow.height
                        radius: Dimensions.radiusLG
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.fillSecondary }
                            GradientStop { position: 1.0; color: Theme.fillTertiary }
                        }
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.08 : 0)
                        Behavior on x { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
                        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

                        // Lit along the top
                        Rectangle {
                            x: plate.radius
                            width: plate.width - plate.radius * 2
                            height: 1
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.18 : 0.6) }
                                GradientStop { position: 1.0; color: "transparent" }
                            }
                        }
                        // The accent underline of the chosen tab
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 5
                            width: 18
                            height: 3
                            radius: 1.5
                            color: Theme.primary
                        }
                    }

                    Row {
                        id: tabRow
                        spacing: 6

                        Repeater {
                            model: root.categories

                            Item {
                                id: tab
                                required property var modelData
                                required property int index
                                readonly property bool selected: root.selectedCategory === tab.index

                                width: Math.max(80, tabLabel.implicitWidth + 28)
                                height: 62

                                activeFocusOnTab: true
                                Accessible.role: Accessible.PageTab
                                Accessible.name: tab.modelData.name
                                Accessible.checked: tab.selected
                                Accessible.onPressAction: root.selectedCategory = tab.index
                                Keys.onReturnPressed: root.selectedCategory = tab.index
                                Keys.onSpacePressed: root.selectedCategory = tab.index

                                onSelectedChanged: if (selected) {
                                    tabBar._sel = tab
                                    tabIcon.play()
                                }
                                Component.onCompleted: if (selected) tabBar._sel = tab

                                scale: tabTap.pressed ? 0.94 : 1
                                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Dimensions.radiusLG
                                    color: !tab.selected && tabHover.hovered ? Theme.fillQuaternary : "transparent"
                                    Behavior on color { ColorAnimation { duration: Dimensions.animVeryFast } }
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    radius: Dimensions.radiusLG + 2
                                    color: "transparent"
                                    border.width: 2
                                    border.color: Theme.withAlpha(Theme.primary, 0.5)
                                    visible: tab.activeFocus
                                }
                                Column {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: -2
                                    spacing: 5
                                    MotionIcon {
                                        id: tabIcon
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        glyph: root._icon(tab.modelData)
                                        size: 20
                                        color: tab.selected ? Theme.primaryText
                                             : tabHover.hovered ? Theme.textPrimary : Theme.textSecondary
                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    }
                                    Text {
                                        id: tabLabel
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        textFormat: Text.PlainText
                                        text: tab.modelData.name
                                        font.pixelSize: Dimensions.footnote
                                        font.weight: tab.selected ? Font.DemiBold : Font.Medium
                                        color: tab.selected ? Theme.textPrimary : Theme.textSecondary
                                    }
                                }
                                HoverHandler {
                                    id: tabHover
                                    cursorShape: Qt.PointingHandCursor
                                    onHoveredChanged: if (hovered && !tab.selected) tabIcon.play()
                                }
                                MouseArea {
                                    id: tabTap
                                    anchors.fill: parent
                                    onClicked: root.selectedCategory = tab.index
                                }
                            }
                        }
                    }
                }

                // Settings content — cached pages (load once, keep alive)
                Item {
                    id: pageContainer
                    Layout.fillWidth: true

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
                    // Entry: a fade, and a slide from the side of the tab it
                    // came from (from below on the first visit)
                    opacity: 0
                    transform: Translate { id: contentTranslate; y: 8 }
                    property int _shownCategory: 0
                    ParallelAnimation {
                        id: contentEntryAnim
                        NumberAnimation {
                            target: pageContainer; property: "opacity"
                            to: 1; duration: Dimensions.animNormal; easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: contentTranslate; property: "y"
                            to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutExpo
                        }
                        NumberAnimation {
                            target: contentTranslate; property: "x"
                            to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutExpo
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
                            const dir = root.selectedCategory > pageContainer._shownCategory ? 1
                                      : root.selectedCategory < pageContainer._shownCategory ? -1 : 0
                            pageContainer._shownCategory = root.selectedCategory
                            pageContainer.opacity = 0
                            contentTranslate.x = dir * 36
                            contentTranslate.y = dir === 0 ? 8 : 0

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

                Item { Layout.preferredHeight: 40 }
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

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: settingsScrollView.contentItem
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
