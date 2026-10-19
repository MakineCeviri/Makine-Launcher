// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PluginSettingsPage.qml — Detail page of one installed plugin (plugin system v2)
 *
 * Header with state and switches, actions, settings drawn from the manifest,
 * permissions, log tail and uninstall. Shown as a sidebar category for enabled
 * plugins and as the drill-down of PluginsSettings.
 * Everything a plugin supplies is untrusted text, so every label is PlainText.
 */
ColumnLayout {
    id: pluginPage
    spacing: Dimensions.spacingXL

    property string pluginId: ""

    // Snapshot of PluginManager.plugin(pluginId). The lists below are replaced
    // only when their content changes, so Repeaters do not rebuild on every
    // state tick (a rebuild would drop the focus of a field being typed in).
    property var info: ({})
    // The three fields that change while the plugin runs. Kept apart from `info`
    // so a status tick re-evaluates only the bindings that show them.
    property string _state: ""
    property string _statusText: ""
    property string _lastError: ""
    property var actions: []
    property var capabilities: []
    property var settingDefs: []
    property string settingsError: ""
    property string pendingAction: ""
    property string actionNote: ""
    property bool actionNoteOk: true
    property string logText: ""
    property bool logCopied: false
    // True while this page's own setSetting call runs: the editor already shows
    // the value, so the settingsChanged it triggers needs no reload.
    property bool _savingSelf: false

    readonly property bool _exists: info.id !== undefined
    readonly property var _missingIds: {
        var ids = []
        var missing = info.missingCapabilities || []
        for (var i = 0; i < missing.length; i++)
            ids.push(missing[i].id)
        return ids
    }

    // ── Data ──

    function _same(a, b) {
        return JSON.stringify(a) === JSON.stringify(b)
    }

    // Hot path: only this plugin's state, status line and last error.
    function refreshRuntime() {
        var m = PluginManager && pluginId !== "" ? PluginManager.plugin(pluginId) : ({})
        _state = m.state || ""
        _statusText = m.statusText || ""
        _lastError = m.lastError || ""
    }

    // Everything else; runs on list-level changes (install, update, enable, consent).
    function refresh() {
        if (!PluginManager || pluginId === "") {
            info = ({})
            refreshRuntime()
            return
        }
        var next = PluginManager.plugin(pluginId)
        var versionChanged = next.version !== info.version
        info = next
        refreshRuntime()
        if (!_same(next.actions || [], actions)) actions = next.actions || []
        if (!_same(next.capabilities || [], capabilities)) capabilities = next.capabilities || []
        if (versionChanged) loadSettings()
    }

    function loadSettings() {
        var next = PluginManager && pluginId !== "" ? PluginManager.settings(pluginId) : []
        if (!_same(next, settingDefs)) settingDefs = next
    }

    function loadLog() {
        var lines = PluginManager && pluginId !== "" ? PluginManager.logLines(pluginId) : []
        logText = lines.join("\n")
    }

    function reloadAll() {
        info = ({})
        pendingAction = ""
        actionNote = ""
        settingsError = ""
        refresh()
        loadSettings()
        loadLog()
    }

    // A failed save leaves the editor showing a value the plugin never got:
    // say so and read the stored values back.
    function failSetting(def) {
        settingsError = qsTr("“%1” kaydedilemedi: değer geçerli değil.").arg(def.label || def.key)
        loadSettings()
    }

    // reload: the stored value may differ from what was passed (a path arrives
    // as a file:// URL and is stored as a native path), so show the stored one.
    function saveSetting(def, value, reload) {
        _savingSelf = true
        var ok = !!PluginManager && PluginManager.setSetting(pluginId, def.key, value)
        _savingSelf = false
        if (ok) {
            settingsError = ""
            if (reload) loadSettings()
            return true
        }
        failSetting(def)
        return false
    }

    function valueText(def) {
        return def.value === undefined || def.value === null ? "" : String(def.value)
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

    onPluginIdChanged: reloadAll()
    Component.onCompleted: reloadAll()

    Connections {
        target: PluginManager
        function onPluginsChanged() { pluginPage.refresh() }
        function onPluginChanged(pid) { if (pid === pluginPage.pluginId) pluginPage.refreshRuntime() }
        function onSettingsChanged(pid) {
            if (pid === pluginPage.pluginId && !pluginPage._savingSelf) pluginPage.loadSettings()
        }
        function onLogChanged(pid) {
            if (pid === pluginPage.pluginId && !_logTimer.running) _logTimer.start()
        }
        function onActionFinished(pid, actionId, ok, message) {
            if (pid !== pluginPage.pluginId) return
            pluginPage.pendingAction = ""
            pluginPage.actionNoteOk = ok
            pluginPage.actionNote = message
                || (ok ? qsTr("İşlem tamamlandı.") : qsTr("İşlem başarısız oldu."))
        }
    }

    // Coalesces bursts of log lines into one refresh.
    Timer {
        id: _logTimer
        interval: 250
        onTriggered: pluginPage.loadLog()
    }

    Timer {
        id: _copiedTimer
        interval: 1500
        onTriggered: pluginPage.logCopied = false
    }

    // ── Small building blocks (the same look as PluginsSettings) ──

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

    component PluginTextField: TextField {
        id: tf
        implicitHeight: 34
        leftPadding: 10
        rightPadding: 10
        font.pixelSize: Dimensions.fontSM
        color: Theme.textPrimary
        placeholderTextColor: Theme.textMuted
        selectByMouse: true
        background: Rectangle {
            radius: Dimensions.radiusMD
            color: Theme.primary06
            border.color: tf.activeFocus ? Theme.primary : Theme.primary12
            border.width: 1
        }
    }

    component PluginComboBox: ComboBox {
        id: combo
        implicitHeight: 34
        font.pixelSize: Dimensions.fontSM

        contentItem: Label {
            leftPadding: 10
            rightPadding: combo.indicator.width + 14
            textFormat: Text.PlainText
            text: combo.displayText
            font: combo.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Label {
            x: combo.width - width - 10
            y: (combo.height - height) / 2
            textFormat: Text.PlainText
            text: "\u25BE"
            font.pixelSize: Dimensions.fontSM
            color: Theme.textMuted
        }

        background: Rectangle {
            radius: Dimensions.radiusMD
            color: Theme.primary06
            border.color: combo.activeFocus || combo.popup.visible ? Theme.primary : Theme.primary12
            border.width: 1
        }

        delegate: ItemDelegate {
            id: comboItem
            required property var modelData
            required property int index
            width: combo.width
            height: 32
            highlighted: combo.highlightedIndex === comboItem.index
            contentItem: Label {
                textFormat: Text.PlainText
                text: comboItem.modelData[combo.textRole]
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                color: comboItem.highlighted ? Theme.primary12 : "transparent"
            }
        }

        popup: Popup {
            y: combo.height + 2
            width: combo.width
            implicitHeight: Math.min(_comboList.contentHeight, 220) + 2
            padding: 1
            contentItem: ListView {
                id: _comboList
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollBar.vertical: StyledScrollBar {}
            }
            background: Rectangle {
                radius: Dimensions.radiusMD
                color: Theme.surfaceElevated
                border.color: Theme.primary12
                border.width: 1
            }
        }
    }

    // ── Not found ──
    SettingsCard {
        Layout.fillWidth: true
        visible: !pluginPage._exists

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 100

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    textFormat: Text.PlainText
                    text: qsTr("Eklenti bulunamadı")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textMuted
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    textFormat: Text.PlainText
                    text: qsTr("Kaldırılmış olabilir. Eklentiler sayfasından kontrol edin.")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textSecondary
                }
            }
        }
    }

    // ── Header ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: _headerCol.implicitHeight + Dimensions.marginML * 2

            ColumnLayout {
                id: _headerCol
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
                                text: pluginPage.info.name || ""
                                font.pixelSize: Dimensions.fontLG
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }

                            PluginBadge {
                                visible: !!pluginPage.info.version
                                text: "v" + (pluginPage.info.version || "")
                            }

                            PluginBadge {
                                visible: pluginPage.info.verified === true
                                text: "\u2713 " + qsTr("Doğrulanmış")
                                fg: Theme.verifiedText
                                bg: Theme.verifiedBg
                            }

                            PluginBadge {
                                visible: pluginPage.info.verified !== true && pluginPage._state !== "invalid"
                                text: "\u26A0 " + qsTr("Doğrulanmamış")
                                fg: Theme.warningBadgeText
                                bg: Theme.warningBadgeBg
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: text !== ""
                            textFormat: Text.PlainText
                            text: {
                                var parts = []
                                if (pluginPage.info.publisher)
                                    parts.push(qsTr("Yayıncı: %1").arg(pluginPage.info.publisher))
                                if (pluginPage.info.license)
                                    parts.push(qsTr("Lisans: %1").arg(pluginPage.info.license))
                                return parts.join("  ·  ")
                            }
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                            elide: Text.ElideRight
                        }
                    }

                    PluginSwitch {
                        label: qsTr("Eklentiyi etkinleştir")
                        checked: pluginPage.info.enabled === true
                        enabled: pluginPage._state !== "incompatible" && pluginPage._state !== "invalid"
                        onToggled: if (PluginManager) PluginManager.setEnabled(pluginPage.pluginId, !checked)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    textFormat: Text.PlainText
                    text: pluginPage.info.description || ""
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingSM

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: pluginPage.stateColor(pluginPage._state)
                    }

                    Label {
                        textFormat: Text.PlainText
                        text: pluginPage.stateText(pluginPage._state)
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: pluginPage.stateColor(pluginPage._state)
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: !!pluginPage._statusText
                        textFormat: Text.PlainText
                        text: "· " + pluginPage._statusText
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textMuted
                        elide: Text.ElideRight
                    }

                    Item { Layout.fillWidth: true }
                }

                Label {
                    Layout.fillWidth: true
                    visible: !!pluginPage._lastError
                    textFormat: Text.PlainText
                    text: "\u26A0 " + pluginPage._lastError
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.error
                    wrapMode: Text.WordWrap
                }

                // Unverified plugins run as native code with the user's rights.
                Rectangle {
                    Layout.fillWidth: true
                    visible: pluginPage.info.verified !== true && pluginPage._state !== "invalid"
                    implicitHeight: _unverifiedText.implicitHeight + Dimensions.marginMD * 2
                    radius: Dimensions.radiusMD
                    color: Theme.warning08
                    border.color: Theme.warning25
                    border.width: 1

                    Label {
                        id: _unverifiedText
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginMD
                        textFormat: Text.PlainText
                        text: qsTr("Bu eklenti Makine tarafından doğrulanmadı. Eklentiler bilgisayarınızda yerel kod olarak çalışır; yalnızca güvendiğiniz kaynaklardan gelenleri kullanın.")
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingMD

                    PluginButton {
                        visible: pluginPage.info.enabled === true
                        text: qsTr("Yeniden başlat")
                        enabled: pluginPage._state !== "starting" && pluginPage._state !== "stopping"
                        onClicked: if (PluginManager) PluginManager.restart(pluginPage.pluginId)
                    }

                    PluginButton {
                        // Only web links: the homepage comes from the plugin's manifest.
                        visible: /^https?:\/\//.test(pluginPage.info.homepage || "")
                        text: qsTr("Web sitesi")
                        onClicked: Qt.openUrlExternally(pluginPage.info.homepage)
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    // ── Actions ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists && pluginPage.actions.length > 0

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                    textFormat: Text.PlainText
                    text: qsTr("İşlemler")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }

            SettingsDivider {}

            Repeater {
                model: pluginPage.actions

                RowLayout {
                    id: actionRow
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: Dimensions.marginMS
                    spacing: Dimensions.spacingLG

                    PluginButton {
                        Layout.preferredWidth: 160
                        text: pluginPage.pendingAction === actionRow.modelData.id
                              ? qsTr("Çalışıyor…") : actionRow.modelData.label
                        enabled: pluginPage._state === "running" && pluginPage.pendingAction === ""
                        onClicked: {
                            pluginPage.actionNote = ""
                            pluginPage.pendingAction = actionRow.modelData.id
                            PluginManager.runAction(pluginPage.pluginId, actionRow.modelData.id)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: actionRow.modelData.description || ""
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginML
                Layout.rightMargin: Dimensions.marginML
                Layout.topMargin: Dimensions.marginMS
                visible: pluginPage._state !== "running"
                textFormat: Text.PlainText
                text: qsTr("İşlemler yalnızca eklenti çalışırken kullanılabilir.")
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginML
                Layout.rightMargin: Dimensions.marginML
                Layout.topMargin: Dimensions.marginMS
                visible: pluginPage.actionNote !== ""
                textFormat: Text.PlainText
                text: (pluginPage.actionNoteOk ? "\u2713 " : "\u2715 ") + pluginPage.actionNote
                font.pixelSize: Dimensions.fontSM
                color: pluginPage.actionNoteOk ? Theme.success : Theme.error
                wrapMode: Text.WordWrap
            }

            Item { Layout.preferredHeight: Dimensions.marginML }
        }
    }

    // ── Settings ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists && pluginPage.settingDefs.length > 0

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                    textFormat: Text.PlainText
                    text: qsTr("Ayarlar")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }

            SettingsDivider {}

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginML
                Layout.rightMargin: Dimensions.marginML
                Layout.topMargin: Dimensions.marginMS
                visible: pluginPage.settingsError !== ""
                textFormat: Text.PlainText
                text: "\u26A0 " + pluginPage.settingsError
                font.pixelSize: Dimensions.fontSM
                color: Theme.error
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: pluginPage.settingDefs

                ColumnLayout {
                    id: settingItem
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0

                    // Whether a secret is stored; the value itself never reaches QML.
                    property bool secretSaved: modelData.hasValue === true
                    readonly property bool wide: modelData.type === "text"
                                                 || modelData.type === "secret"
                                                 || modelData.type === "path"

                    SettingsDivider { visible: settingItem.index > 0 }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Dimensions.marginML
                        Layout.rightMargin: Dimensions.marginML
                        Layout.topMargin: Dimensions.marginMS
                        Layout.bottomMargin: Dimensions.marginMS
                        spacing: Dimensions.spacingMD

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXL

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXS

                                RowLayout {
                                    // Fills even without a description, so every control sits at the right edge.
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingSM

                                    Label {
                                        textFormat: Text.PlainText
                                        text: settingItem.modelData.label || settingItem.modelData.key
                                        font.pixelSize: Dimensions.fontMD
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }

                                    PluginBadge {
                                        visible: settingItem.modelData.type === "secret" && settingItem.secretSaved
                                        text: qsTr("Kayıtlı")
                                        fg: Theme.verifiedText
                                        bg: Theme.verifiedBg
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !!settingItem.modelData.description
                                    textFormat: Text.PlainText
                                    text: settingItem.modelData.description || ""
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Loader {
                                Layout.alignment: Qt.AlignVCenter
                                visible: !settingItem.wide
                                sourceComponent: settingItem.modelData.type === "toggle" ? toggleControl
                                               : settingItem.modelData.type === "select" ? selectControl
                                               : settingItem.modelData.type === "number" ? numberControl
                                               : null
                            }
                        }

                        Loader {
                            Layout.fillWidth: true
                            visible: settingItem.wide
                            sourceComponent: settingItem.modelData.type === "text" ? textControl
                                           : settingItem.modelData.type === "secret" ? secretControl
                                           : settingItem.modelData.type === "path" ? pathControl
                                           : null
                        }
                    }

                    Component {
                        id: toggleControl

                        PluginSwitch {
                            label: settingItem.modelData.label || settingItem.modelData.key
                            checked: settingItem.modelData.value === true
                            onToggled: {
                                if (pluginPage.saveSetting(settingItem.modelData, !checked))
                                    checked = !checked
                            }
                        }
                    }

                    Component {
                        id: selectControl

                        PluginComboBox {
                            implicitWidth: 200
                            model: settingItem.modelData.options || []
                            textRole: "label"
                            valueRole: "value"
                            Component.onCompleted: currentIndex = indexOfValue(settingItem.modelData.value)
                            onActivated: pluginPage.saveSetting(settingItem.modelData, currentValue)
                        }
                    }

                    Component {
                        id: numberControl

                        PluginTextField {
                            implicitWidth: 120
                            horizontalAlignment: TextInput.AlignRight
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            text: pluginPage.valueText(settingItem.modelData)
                            placeholderText: {
                                var def = settingItem.modelData
                                if (def.min !== undefined && def.max !== undefined) return def.min + " – " + def.max
                                return ""
                            }
                            onEditingFinished: {
                                var raw = text.trim().replace(",", ".")
                                var n = Number(raw)
                                if (raw === "" || isNaN(n)) {
                                    pluginPage.failSetting(settingItem.modelData)
                                    return
                                }
                                if (pluginPage.saveSetting(settingItem.modelData, n))
                                    text = String(n)
                            }
                        }
                    }

                    Component {
                        id: textControl

                        PluginTextField {
                            text: pluginPage.valueText(settingItem.modelData)
                            onEditingFinished: {
                                if (text !== pluginPage.valueText(settingItem.modelData))
                                    pluginPage.saveSetting(settingItem.modelData, text)
                            }
                        }
                    }

                    // Empty input keeps the stored secret; only "Temizle" removes it.
                    Component {
                        id: secretControl

                        RowLayout {
                            spacing: Dimensions.spacingMD

                            PluginTextField {
                                Layout.fillWidth: true
                                echoMode: TextInput.Password
                                placeholderText: settingItem.secretSaved
                                                 ? qsTr("Kayıtlı — değiştirmek için yeni değeri yazın")
                                                 : qsTr("Değeri girin")
                                onEditingFinished: {
                                    if (text === "") return
                                    if (pluginPage.saveSetting(settingItem.modelData, text)) {
                                        settingItem.secretSaved = true
                                        text = ""
                                    }
                                }
                            }

                            PluginButton {
                                visible: settingItem.secretSaved
                                kind: "danger"
                                text: qsTr("Temizle")
                                onClicked: {
                                    if (pluginPage.saveSetting(settingItem.modelData, ""))
                                        settingItem.secretSaved = false
                                }
                            }
                        }
                    }

                    Component {
                        id: pathControl

                        RowLayout {
                            id: pathRow
                            spacing: Dimensions.spacingMD

                            function commit(path) {
                                if (path !== pluginPage.valueText(settingItem.modelData))
                                    pluginPage.saveSetting(settingItem.modelData, path)
                            }

                            PluginTextField {
                                id: pathField
                                Layout.fillWidth: true
                                text: pluginPage.valueText(settingItem.modelData)
                                placeholderText: settingItem.modelData.mode === "folder"
                                                 ? qsTr("Klasör yolu") : qsTr("Dosya yolu")
                                onEditingFinished: pathRow.commit(text)
                            }

                            PluginButton {
                                text: qsTr("Gözat…")
                                onClicked: settingItem.modelData.mode === "folder"
                                           ? folderDialog.open() : fileDialog.open()
                            }

                            FolderDialog {
                                id: folderDialog
                                title: settingItem.modelData.label || settingItem.modelData.key
                                onAccepted: pluginPage.saveSetting(settingItem.modelData, selectedFolder.toString(), true)
                            }

                            FileDialog {
                                id: fileDialog
                                title: settingItem.modelData.label || settingItem.modelData.key
                                onAccepted: pluginPage.saveSetting(settingItem.modelData, selectedFile.toString(), true)
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Permissions ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                    textFormat: Text.PlainText
                    text: qsTr("İzinler")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }

            SettingsDivider {}

            Label {
                Layout.fillWidth: true
                Layout.margins: Dimensions.marginML
                visible: pluginPage.capabilities.length === 0
                textFormat: Text.PlainText
                text: qsTr("Bu eklenti özel bir izin istemiyor.")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }

            Repeater {
                model: pluginPage.capabilities

                RowLayout {
                    id: capRow
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: Dimensions.marginMS
                    spacing: Dimensions.spacingLG

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingXXS

                        Label {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: capRow.modelData.label
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !!capRow.modelData.reason
                            textFormat: Text.PlainText
                            text: capRow.modelData.reason || ""
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                        }
                    }

                    PluginBadge {
                        readonly property bool missing: pluginPage._missingIds.indexOf(capRow.modelData.id) >= 0
                        text: missing ? qsTr("Bekliyor") : qsTr("Verildi")
                        fg: missing ? Theme.warningBadgeText : Theme.verifiedText
                        bg: missing ? Theme.warningBadgeBg : Theme.verifiedBg
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Dimensions.marginML
                visible: pluginPage._missingIds.length > 0

                PluginButton {
                    kind: "primary"
                    text: qsTr("İzinleri ver")
                    onClicked: if (PluginManager) PluginManager.grantCapabilities(pluginPage.pluginId)
                }

                Item { Layout.fillWidth: true }
            }

            Item { Layout.preferredHeight: Dimensions.marginSM }
        }
    }

    // ── Log ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists

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
                        textFormat: Text.PlainText
                        text: qsTr("Günlük")
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    PluginButton {
                        text: pluginPage.logCopied ? qsTr("Kopyalandı") : qsTr("Tümünü kopyala")
                        enabled: pluginPage.logText !== ""
                        onClicked: {
                            logArea.selectAll()
                            logArea.copy()
                            logArea.deselect()
                            pluginPage.logCopied = true
                            _copiedTimer.restart()
                        }
                    }
                }
            }

            SettingsDivider {}

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                Layout.margins: Dimensions.marginML
                radius: Dimensions.radiusMD
                color: Theme.bgPrimary
                border.color: Theme.primary08
                border.width: 1

                // Reading old lines: do not jump to the newest one under the cursor.
                HoverHandler { id: logHover }

                ScrollView {
                    id: logScroll
                    anchors.fill: parent
                    anchors.margins: 2
                    clip: true
                    ScrollBar.vertical: StyledScrollBar {}

                    TextArea {
                        id: logArea
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        text: pluginPage.logText
                        placeholderText: qsTr("Henüz günlük kaydı yok.")
                        placeholderTextColor: Theme.textMuted
                        font.family: "Consolas"
                        font.pixelSize: Dimensions.fontCaption
                        color: Theme.textSecondary
                        background: null

                        onTextChanged: Qt.callLater(function() {
                            if (logHover.hovered) return
                            var bar = logScroll.ScrollBar.vertical
                            bar.position = Math.max(0, 1.0 - bar.size)
                        })
                    }
                }
            }
        }
    }

    // ── Uninstall ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage._exists

        Item {
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
                        textFormat: Text.PlainText
                        text: qsTr("Eklentiyi kaldır")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: qsTr("Eklenti dosyaları silinir. Ayarlarını isteğe bağlı olarak saklayabilirsiniz.")
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                        elide: Text.ElideRight
                    }
                }

                PluginButton {
                    kind: "danger"
                    text: qsTr("Kaldır")
                    enabled: !PluginManager || PluginManager.busyPluginId !== pluginPage.pluginId
                    onClicked: _uninstallDialog.open()
                }
            }
        }
    }

    // ConfirmDialog keeps the header and the buttons; only the body is replaced
    // by one with the "delete data too" choice.
    ConfirmDialog {
        id: _uninstallDialog
        parent: Overlay.overlay
        title: qsTr("Eklenti kaldırılsın mı?")
        confirmText: qsTr("Kaldır")
        property bool removeData: false
        contentHeight: _uninstallBody.implicitHeight
        onAboutToShow: removeData = false
        onConfirmed: if (PluginManager) PluginManager.uninstall(pluginPage.pluginId, removeData)

        contentItem: ColumnLayout {
            id: _uninstallBody
            spacing: Dimensions.spacingMD

            Item { Layout.preferredHeight: Dimensions.spacingXS }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                textFormat: Text.PlainText
                text: qsTr("“%1” kaldırılacak. Eklenti çalışıyorsa önce durdurulur.").arg(pluginPage.info.name || "")
                font.pixelSize: Dimensions.fontSM
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.5
            }

            CheckBox {
                id: dataBox
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                text: qsTr("Eklentinin verilerini de sil")
                checked: _uninstallDialog.removeData
                onToggled: _uninstallDialog.removeData = checked

                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: dataBox.leftPadding
                    y: (dataBox.height - height) / 2
                    radius: Dimensions.radiusSM
                    color: dataBox.checked ? Theme.primary : "transparent"
                    border.color: dataBox.checked ? Theme.primary : Theme.textPrimary25
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        visible: dataBox.checked
                        textFormat: Text.PlainText
                        text: "\u2713"
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textOnColor
                    }
                }

                contentItem: Label {
                    leftPadding: dataBox.indicator.width + dataBox.spacing
                    textFormat: Text.PlainText
                    text: dataBox.text
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                textFormat: Text.PlainText
                text: qsTr("İşaretlemezseniz ayarlar ve eklentinin kaydettiği veriler korunur; yeniden kurarsanız kaldığınız yerden devam eder.")
                font.pixelSize: Dimensions.fontCaption
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            Item { Layout.preferredHeight: Dimensions.spacingXS }
        }
    }
}
