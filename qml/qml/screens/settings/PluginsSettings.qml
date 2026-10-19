// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PluginsSettings.qml - Plugin management panel (plugin system v2)
 *
 * "Yüklü": installed plugins with state, enable switch, consent and update.
 * "Keşfet": the signed official catalogue; installing asks for the permissions first.
 * Developer mode adds installing an unverified .mkplugin from disk.
 * Details of one plugin replace the list in place (PluginSettingsPage).
 * Backend: PluginManager C++ service (null in builds without dev tools).
 * Everything a plugin or the registry supplies is untrusted text: PlainText only.
 */
ColumnLayout {
    id: pluginsRoot
    spacing: Dimensions.spacingXL

    // Ids of the installed plugins. A card reads its own plugin and follows
    // pluginChanged(id) itself, so a state tick never rebuilds the list.
    ListModel { id: _idModel }
    // Replaced only when the content changes, so cards do not rebuild on no-op signals.
    property var catalog: []
    // Id of the plugin whose details are open; "" shows the list.
    property string detailId: ""
    // Catalogue entry waiting for the user's confirmation.
    property var pendingInstall: null

    readonly property string busyId: PluginManager ? PluginManager.busyPluginId : ""
    readonly property real busyProgress: PluginManager ? PluginManager.busyProgress : 0
    readonly property bool catalogLoading: PluginManager ? PluginManager.catalogLoading : false
    readonly property string catalogError: PluginManager ? PluginManager.catalogError : ""
    readonly property bool developerMode: PluginManager ? PluginManager.developerMode : false
    readonly property int updateCount: PluginManager ? PluginManager.updateCount : 0

    // Brings the id model in line with the installed set, touching only what
    // differs: installing or removing one plugin leaves the other cards alone.
    function syncPlugins() {
        var list = PluginManager ? PluginManager.plugins : []
        var ids = []
        for (var i = 0; i < list.length; i++) ids.push(list[i].id)

        for (var n = 0; n < ids.length; n++) {
            if (n < _idModel.count && _idModel.get(n).pluginId === ids[n]) continue
            var from = -1
            for (var k = n + 1; k < _idModel.count; k++) {
                if (_idModel.get(k).pluginId === ids[n]) { from = k; break }
            }
            if (from >= 0) _idModel.move(from, n, 1)
            else _idModel.insert(n, { pluginId: ids[n] })
        }
        while (_idModel.count > ids.length) _idModel.remove(_idModel.count - 1)

        // The plugin whose details are open was uninstalled.
        if (detailId !== "" && ids.indexOf(detailId) < 0) detailId = ""
    }

    function syncCatalog() {
        var next = PluginManager ? PluginManager.catalog : []
        if (JSON.stringify(next) !== JSON.stringify(catalog)) catalog = next
    }

    function catalogEntry(id) {
        for (var i = 0; i < catalog.length; i++)
            if (catalog[i].id === id) return catalog[i]
        return null
    }

    function openDetail(id) {
        detailId = id
        // The page scrolls inside the settings ScrollView's Flickable.
        var f = pluginsRoot
        while (f && !("contentY" in f)) f = f.parent
        if (f) f["contentY"] = 0
    }

    function askInstall(id) {
        var entry = catalogEntry(id)
        if (!entry) return
        pendingInstall = entry
        _installConfirm.open()
    }

    function formatSize(bytes) {
        if (!(bytes > 0)) return "?"
        if (bytes < 1024 * 1024) return Math.max(1, Math.round(bytes / 1024)) + " KB"
        return (bytes / 1048576).toFixed(1) + " MB"
    }

    function installMessage(entry) {
        var caps = entry.capabilities || []
        var text = ""
        if (caps.length > 0) {
            var lines = []
            for (var i = 0; i < caps.length; i++)
                lines.push("• " + caps[i].label + (caps[i].reason ? " — " + caps[i].reason : ""))
            text = qsTr("Bu eklenti Makine'den ayrı bir işlem olarak çalışır ve şu izinleri ister:")
                   + "\n\n" + lines.join("\n")
        } else {
            text = qsTr("Bu eklenti özel bir izin istemiyor.")
        }
        text += "\n\n" + qsTr("Boyut: %1").arg(formatSize(entry.size))
        if (entry.publisher) text += "   ·   " + qsTr("Yayıncı: %1").arg(entry.publisher)
        return text
    }

    function stateText(s) {
        switch (s) {
        case "disabled": return qsTr("Devre dışı")
        case "starting": return qsTr("Başlatılıyor")
        case "running": return qsTr("Çalışıyor")
        case "stopping": return qsTr("Durduruluyor")
        case "error": return qsTr("Hata")
        case "crashed": return qsTr("Beklenmedik şekilde durdu")
        case "needs-consent": return qsTr("İzin bekliyor")
        case "incompatible": return qsTr("Uyumsuz")
        case "invalid": return qsTr("Geçersiz")
        default: return s
        }
    }

    function stateColor(s) {
        switch (s) {
        case "running": return Theme.success
        case "starting":
        case "stopping":
        case "needs-consent":
        case "incompatible": return Theme.warning
        case "error":
        case "crashed":
        case "invalid": return Theme.error
        default: return Theme.textMuted
        }
    }

    Component.onCompleted: {
        syncPlugins()
        syncCatalog()
    }

    Connections {
        target: PluginManager
        // List-level changes only; one plugin's state ticks arrive as pluginChanged(id).
        function onPluginsChanged() { pluginsRoot.syncPlugins() }
        function onCatalogChanged() { pluginsRoot.syncCatalog() }
    }

    // ── Small building blocks (the same look as PluginSettingsPage) ──

    component PluginBadge: Rectangle {
        id: badge
        property string text: ""
        property color fg: Theme.textMuted
        property color bg: Theme.textPrimary06
        implicitWidth: _badgeLabel.implicitWidth + 12
        implicitHeight: 20
        radius: Dimensions.radiusFull
        color: badge.bg

        Label {
            id: _badgeLabel
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: badge.text
            font.pixelSize: Dimensions.fontMini
            font.weight: Font.Medium
            color: badge.fg
        }
    }

    component PluginButton: Rectangle {
        id: btn
        property string text: ""
        property string kind: "ghost"   // primary | ghost | danger
        signal clicked()

        implicitWidth: _btnLabel.implicitWidth + 28
        implicitHeight: 34
        radius: Dimensions.radiusMD
        color: btn.kind === "primary" ? Theme.primary
             : btn.kind === "danger" ? (_btnMouse.containsMouse ? Theme.error15 : Theme.error10)
             : (_btnMouse.containsMouse ? Theme.primary12 : Theme.primary08)
        border.color: btn.kind === "danger" ? Theme.error25 : "transparent"
        border.width: 1
        opacity: !btn.enabled ? 0.4 : (btn.kind === "primary" && _btnMouse.containsMouse ? 0.9 : 1.0)
        scale: _btnMouse.pressed ? Dimensions.pressScale : 1.0
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: btn.text
        Keys.onReturnPressed: if (btn.enabled) btn.clicked()
        Keys.onSpacePressed: if (btn.enabled) btn.clicked()

        Label {
            id: _btnLabel
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: btn.text
            font.pixelSize: Dimensions.fontSM
            font.weight: Font.Medium
            color: btn.kind === "primary" ? Theme.textOnColor
                 : btn.kind === "danger" ? Theme.error
                 : Theme.textPrimary
        }

        FocusRing { offset: -1 }

        MouseArea {
            id: _btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    component PluginSwitch: Rectangle {
        id: sw
        property bool checked: false
        property string label: ""
        signal toggled()

        implicitWidth: Dimensions.toggleWidth
        implicitHeight: Dimensions.toggleHeight
        radius: Dimensions.toggleRadius
        color: sw.checked ? Theme.primary : Theme.primary12
        opacity: sw.enabled ? 1.0 : 0.4
        Behavior on color { ColorAnimation { duration: Dimensions.animMedium; easing.type: Easing.OutCubic } }

        activeFocusOnTab: true
        Accessible.role: Accessible.CheckBox
        Accessible.name: sw.label
        Accessible.checked: sw.checked
        Keys.onSpacePressed: if (sw.enabled) sw.toggled()
        Keys.onReturnPressed: if (sw.enabled) sw.toggled()

        Rectangle {
            width: Dimensions.toggleKnobSize
            height: Dimensions.toggleKnobSize
            radius: Dimensions.toggleKnobRadius
            color: Theme.textOnColor
            x: sw.checked ? sw.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            Behavior on x { NumberAnimation { duration: Dimensions.animMedium; easing.type: Easing.OutCubic } }
        }

        FocusRing { offset: -1 }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: sw.toggled()
        }
    }

    component SectionTitle: Label {
        textFormat: Text.PlainText
        font.pixelSize: Dimensions.fontLG
        font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    // ── Details of one plugin ──
    ColumnLayout {
        Layout.fillWidth: true
        visible: pluginsRoot.detailId !== ""
        spacing: Dimensions.spacingXL

        RowLayout {
            Layout.fillWidth: true

            PluginButton {
                text: "← " + qsTr("Eklentilere dön")
                onClicked: pluginsRoot.detailId = ""
            }

            Item { Layout.fillWidth: true }
        }

        Loader {
            Layout.fillWidth: true
            active: pluginsRoot.detailId !== ""
            sourceComponent: Component {
                PluginSettingsPage { pluginId: pluginsRoot.detailId }
            }
        }
    }

    // ── Installed ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginsRoot.detailId === ""

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

                    SectionTitle { text: qsTr("Yüklü") }

                    Item { Layout.fillWidth: true }

                    PluginBadge {
                        visible: pluginsRoot.updateCount > 0
                        text: qsTr("%1 güncelleme var").arg(pluginsRoot.updateCount)
                        fg: Theme.warningBadgeText
                        bg: Theme.warningBadgeBg
                    }
                }
            }

            SettingsDivider {}

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 88
                visible: _idModel.count === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingSM

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        textFormat: Text.PlainText
                        text: qsTr("Henüz eklenti yüklenmedi")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textMuted
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        textFormat: Text.PlainText
                        text: qsTr("Aşağıdaki Keşfet bölümünden bir eklenti kurabilirsiniz.")
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                    }
                }
            }

            Repeater {
                model: _idModel

                Rectangle {
                    id: card
                    required property int index
                    required property string pluginId
                    // Install-level data (re-read on pluginsChanged) and the three
                    // runtime fields (re-read on this plugin's pluginChanged).
                    property var info: ({})
                    property string pluginState: ""
                    property string statusText: ""
                    property string lastError: ""
                    readonly property bool busy: pluginsRoot.busyId === pluginId

                    function reloadRuntime() {
                        var m = PluginManager ? PluginManager.plugin(pluginId) : ({})
                        pluginState = m.state || ""
                        statusText = m.statusText || ""
                        lastError = m.lastError || ""
                    }

                    function reload() {
                        info = PluginManager ? PluginManager.plugin(pluginId) : ({})
                        pluginState = info.state || ""
                        statusText = info.statusText || ""
                        lastError = info.lastError || ""
                    }

                    Component.onCompleted: reload()

                    Connections {
                        target: PluginManager
                        function onPluginsChanged() { card.reload() }
                        function onPluginChanged(pid) { if (pid === card.pluginId) card.reloadRuntime() }
                    }

                    Layout.fillWidth: true
                    Layout.preferredHeight: _cardCol.implicitHeight + Dimensions.marginML * 2
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: index === 0 ? Dimensions.spacingMD : Dimensions.spacingSM
                    Layout.bottomMargin: index === _idModel.count - 1 ? Dimensions.spacingMD : 0
                    radius: Dimensions.radiusMD
                    color: _cardHover.hovered ? Theme.primary06 : Theme.primary04
                    border.color: Theme.primary08
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    HoverHandler { id: _cardHover }

                    ColumnLayout {
                        id: _cardCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingMD

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXS

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingSM

                                    Label {
                                        Layout.maximumWidth: 260
                                        textFormat: Text.PlainText
                                        text: card.info.name
                                        font.pixelSize: Dimensions.fontMD
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    PluginBadge {
                                        visible: !!card.info.version
                                        text: "v" + card.info.version
                                    }

                                    PluginBadge {
                                        visible: card.info.verified === true
                                        text: "✓ " + qsTr("Doğrulanmış")
                                        fg: Theme.verifiedText
                                        bg: Theme.verifiedBg
                                    }

                                    PluginBadge {
                                        visible: card.info.verified !== true && card.pluginState !== "invalid"
                                        text: "⚠ " + qsTr("Doğrulanmamış")
                                        fg: Theme.warningBadgeText
                                        bg: Theme.warningBadgeBg
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !!card.info.description
                                    textFormat: Text.PlainText
                                    text: card.info.description || ""
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !!card.info.publisher
                                    textFormat: Text.PlainText
                                    text: qsTr("Yayıncı: %1").arg(card.info.publisher || "")
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }

                            PluginSwitch {
                                Layout.alignment: Qt.AlignTop
                                label: qsTr("Eklentiyi etkinleştir")
                                checked: card.info.enabled === true
                                enabled: card.pluginState !== "incompatible" && card.pluginState !== "invalid"
                                         && !card.busy
                                onToggled: if (PluginManager) PluginManager.setEnabled(card.pluginId, !checked)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingSM

                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: pluginsRoot.stateColor(card.pluginState)
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: pluginsRoot.stateText(card.pluginState)
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.Medium
                                color: pluginsRoot.stateColor(card.pluginState)
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: !!card.statusText
                                textFormat: Text.PlainText
                                text: "· " + (card.statusText || "")
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textMuted
                                elide: Text.ElideRight
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !!card.lastError
                            textFormat: Text.PlainText
                            text: "⚠ " + (card.lastError || "")
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.error
                            wrapMode: Text.WordWrap
                        }

                        // The plugin does not start before the user has granted what its manifest asks for.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: card.pluginState === "needs-consent"
                            implicitHeight: _consentCol.implicitHeight + Dimensions.marginMD * 2
                            radius: Dimensions.radiusMD
                            color: Theme.warning08
                            border.color: Theme.warning25
                            border.width: 1

                            ColumnLayout {
                                id: _consentCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Dimensions.marginMD
                                spacing: Dimensions.spacingMD

                                Label {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: qsTr("Bu eklentinin çalışabilmesi için aşağıdaki izinleri vermeniz gerekiyor:")
                                    font.pixelSize: Dimensions.fontSM
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    wrapMode: Text.WordWrap
                                }

                                Repeater {
                                    model: card.info.missingCapabilities || []

                                    ColumnLayout {
                                        id: missingItem
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: Dimensions.spacingXXS

                                        Label {
                                            Layout.fillWidth: true
                                            textFormat: Text.PlainText
                                            text: missingItem.modelData.label
                                            font.pixelSize: Dimensions.fontBody
                                            font.weight: Font.Medium
                                            color: Theme.textPrimary
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: !!missingItem.modelData.reason
                                            textFormat: Text.PlainText
                                            text: missingItem.modelData.reason || ""
                                            font.pixelSize: Dimensions.fontSM
                                            color: Theme.textSecondary
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    PluginButton {
                                        kind: "primary"
                                        text: qsTr("İzinleri ver")
                                        onClicked: if (PluginManager) PluginManager.grantCapabilities(card.pluginId)
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: card.busy
                            spacing: Dimensions.spacingXS

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                                radius: 2
                                color: Theme.primary12

                                Rectangle {
                                    width: parent.width * pluginsRoot.busyProgress
                                    height: parent.height
                                    radius: 2
                                    color: Theme.primary
                                }
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: qsTr("Güncelleniyor… %1%").arg(Math.round(pluginsRoot.busyProgress * 100))
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textMuted
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingMD

                            PluginButton {
                                visible: card.info.updateAvailable === true
                                kind: "primary"
                                text: qsTr("Güncelle") + " · v" + card.info.availableVersion
                                enabled: pluginsRoot.busyId === ""
                                onClicked: pluginsRoot.askInstall(card.pluginId)
                            }

                            Item { Layout.fillWidth: true }

                            PluginButton {
                                text: qsTr("Ayrıntılar")
                                onClicked: pluginsRoot.openDetail(card.pluginId)
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Discover (official catalogue) ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginsRoot.detailId === ""

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
                    spacing: Dimensions.spacingMD

                    SectionTitle { text: qsTr("Keşfet") }

                    BusyIndicator {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        visible: pluginsRoot.catalogLoading
                        running: visible
                        palette.dark: Theme.primary
                    }

                    Item { Layout.fillWidth: true }

                    PluginButton {
                        text: qsTr("Yenile")
                        enabled: !pluginsRoot.catalogLoading
                        onClicked: if (PluginManager) PluginManager.refreshCatalog()
                    }
                }
            }

            SettingsDivider {}

            Rectangle {
                Layout.fillWidth: true
                Layout.margins: Dimensions.marginML
                visible: pluginsRoot.catalogError !== ""
                implicitHeight: _catalogErrorCol.implicitHeight + Dimensions.marginMD * 2
                radius: Dimensions.radiusMD
                color: Theme.error10
                border.color: Theme.error20
                border.width: 1

                ColumnLayout {
                    id: _catalogErrorCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Dimensions.marginMD
                    spacing: Dimensions.spacingXS

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: qsTr("Eklenti kataloğu alınamadı")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.error
                    }

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: pluginsRoot.catalogError
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 88
                visible: pluginsRoot.catalog.length === 0 && pluginsRoot.catalogError === ""

                Label {
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: pluginsRoot.catalogLoading ? qsTr("Katalog yükleniyor…")
                                                     : qsTr("Kurulabilir eklenti bulunamadı.")
                    font.pixelSize: Dimensions.fontMD
                    color: Theme.textMuted
                }
            }

            Repeater {
                model: pluginsRoot.catalog

                Rectangle {
                    id: entryCard
                    required property int index
                    required property var modelData
                    readonly property bool busy: pluginsRoot.busyId === modelData.id
                    readonly property bool installed: modelData.installed === true

                    Layout.fillWidth: true
                    Layout.preferredHeight: _entryCol.implicitHeight + Dimensions.marginML * 2
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: index === 0 ? Dimensions.spacingMD : Dimensions.spacingSM
                    Layout.bottomMargin: index === pluginsRoot.catalog.length - 1 ? Dimensions.spacingMD : 0
                    radius: Dimensions.radiusMD
                    color: _entryHover.hovered ? Theme.primary06 : Theme.primary04
                    border.color: Theme.primary08
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    HoverHandler { id: _entryHover }

                    ColumnLayout {
                        id: _entryCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingMD

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXS

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingSM

                                    Label {
                                        Layout.maximumWidth: 260
                                        textFormat: Text.PlainText
                                        text: entryCard.modelData.name
                                        font.pixelSize: Dimensions.fontMD
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    PluginBadge {
                                        text: "v" + entryCard.modelData.version
                                    }

                                    PluginBadge {
                                        visible: entryCard.installed && entryCard.modelData.updateAvailable !== true
                                        text: "✓ " + qsTr("Yüklü")
                                        fg: Theme.verifiedText
                                        bg: Theme.verifiedBg
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !!entryCard.modelData.description
                                    textFormat: Text.PlainText
                                    text: entryCard.modelData.description || ""
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: {
                                        var parts = []
                                        if (entryCard.modelData.publisher)
                                            parts.push(qsTr("Yayıncı: %1").arg(entryCard.modelData.publisher))
                                        parts.push(qsTr("Boyut: %1").arg(pluginsRoot.formatSize(entryCard.modelData.size)))
                                        return parts.join("  ·  ")
                                    }
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }

                            PluginButton {
                                Layout.alignment: Qt.AlignTop
                                visible: !entryCard.installed || entryCard.modelData.updateAvailable === true
                                kind: "primary"
                                text: entryCard.installed ? qsTr("Güncelle") : qsTr("Kur")
                                enabled: entryCard.modelData.compatible === true && pluginsRoot.busyId === ""
                                onClicked: pluginsRoot.askInstall(entryCard.modelData.id)
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            visible: (entryCard.modelData.capabilities || []).length > 0
                            spacing: Dimensions.spacingSM

                            Repeater {
                                model: entryCard.modelData.capabilities || []

                                PluginBadge {
                                    required property var modelData
                                    text: modelData.label
                                    fg: Theme.textSecondary
                                    bg: Theme.textPrimary04
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: entryCard.modelData.compatible === false
                            textFormat: Text.PlainText
                            text: "⚠ " + (entryCard.modelData.incompatibleReason || qsTr("Bu eklenti bu Makine sürümüyle uyumlu değil."))
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.warning
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: entryCard.busy
                            spacing: Dimensions.spacingXS

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                                radius: 2
                                color: Theme.primary12

                                Rectangle {
                                    width: parent.width * pluginsRoot.busyProgress
                                    height: parent.height
                                    radius: 2
                                    color: Theme.primary
                                }
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: entryCard.installed
                                      ? qsTr("Güncelleniyor… %1%").arg(Math.round(pluginsRoot.busyProgress * 100))
                                      : qsTr("Kuruluyor… %1%").arg(Math.round(pluginsRoot.busyProgress * 100))
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textMuted
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Developer mode ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginsRoot.detailId === ""

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Geliştirici modu")
                description: qsTr("Dosyadan, doğrulanmamış eklenti kurmaya izin verir")
                checked: pluginsRoot.developerMode
                onToggled: if (PluginManager) PluginManager.developerMode = !pluginsRoot.developerMode
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: pluginsRoot.developerMode
                spacing: 0

                SettingsDivider {}

                Rectangle {
                    Layout.fillWidth: true
                    Layout.margins: Dimensions.marginML
                    implicitHeight: _devWarning.implicitHeight + Dimensions.marginMD * 2
                    radius: Dimensions.radiusMD
                    color: Theme.warning08
                    border.color: Theme.warning25
                    border.width: 1

                    Label {
                        id: _devWarning
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginMD
                        textFormat: Text.PlainText
                        text: "⚠ " + qsTr("Dosyadan kurulan eklentiler Makine tarafından doğrulanmaz. Eklentiler bilgisayarınızda yerel kod olarak çalışır: bir eklenti dosyalarınıza ve hesaplarınıza erişebilir, kullandığınız oyunlara zarar verebilir. Yalnızca kaynağına güvendiğiniz eklentileri kurun.")
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.bottomMargin: Dimensions.marginML
                    spacing: Dimensions.spacingMD

                    PluginButton {
                        kind: "primary"
                        text: qsTr("Dosyadan yükle (.mkplugin)")
                        enabled: pluginsRoot.busyId === ""
                        onClicked: _fileDialog.open()
                    }

                    PluginButton {
                        text: qsTr("Geliştirici belgeleri") + " →"
                        onClicked: Qt.openUrlExternally("https://makineceviri.org/docs/plugin-api")
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    FileDialog {
        id: _fileDialog
        title: qsTr("Eklenti dosyası seç")
        nameFilters: [qsTr("Makine eklentisi (*.mkplugin)"), qsTr("Tüm dosyalar (*)")]
        onAccepted: if (PluginManager) PluginManager.installFromFile(selectedFile)
    }

    // The registry entry's permissions count as granted once the user confirms.
    ConfirmDialog {
        id: _installConfirm
        parent: Overlay.overlay
        accentColor: Theme.warning
        title: pluginsRoot.pendingInstall
               ? (pluginsRoot.pendingInstall.installed ? qsTr("%1 güncellensin mi?") : qsTr("%1 kurulsun mu?"))
                     .arg(pluginsRoot.pendingInstall.name)
               : ""
        message: pluginsRoot.pendingInstall ? pluginsRoot.installMessage(pluginsRoot.pendingInstall) : ""
        confirmText: pluginsRoot.pendingInstall && pluginsRoot.pendingInstall.installed
                     ? qsTr("Güncelle") : qsTr("Kur")
        onConfirmed: {
            if (PluginManager && pluginsRoot.pendingInstall)
                PluginManager.install(pluginsRoot.pendingInstall.id)
        }
    }
}
