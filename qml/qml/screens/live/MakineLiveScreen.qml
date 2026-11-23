// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * MakineLiveScreen.qml - "Makine Live": the screen of the bundled Live plugin
 * (org.makineceviri.live, docs/plugins/README.md "plugin screen contract").
 *
 * The plugin owns the logic; this page only shows its latest `ui.state`
 * (PluginManager.pluginState) and calls its methods (PluginManager.invoke):
 * sign-in by code, PC or console mode, the console's video source, the
 * subtitle region, start/stop and the last line read. When the plugin is
 * off, broken or switched off remotely (launcher.json), the page says so
 * instead.
 *
 * State (Live's Engine::stateLocked): {login:{state,code,url,user,message},
 *         mode: pc|console, sources:[{id,key,label,…,pid}], selectedSource:{key,pid}|null,
 *         region:{pc:{x,y,width,height}|null, console:{…}}, running, status,
 *         lastLine:{source,text}, counters:{lines,poolHits}, server:{maintenance,message},
 *         notice, style}
 */
Item {
    id: live

    property real contentMargin: 16

    readonly property string pluginId: "org.makineceviri.live"

    // pluginState()/plugin() are invokables: re-read on their change signals
    property var _info: ({})
    property var _state: ({})
    property string _error: ""
    property string _busyMethod: ""

    readonly property bool _exists: _info.id !== undefined
    readonly property string _pluginState: _info.state || ""
    readonly property bool _running: _pluginState === "running"
    readonly property var _login: _state.login || ({})
    readonly property bool _signedIn: !!_login.user
    readonly property bool _consoleMode: _state.mode === "console"
    readonly property var _sources: _state.sources || []
    readonly property var _region: _state.region ? _state.region[_consoleMode ? "console" : "pc"] || null : null
    readonly property var _selected: _state.selectedSource || null
    readonly property var _lastLine: _state.lastLine || ({})
    readonly property var _counters: _state.counters || ({})
    readonly property var _server: _state.server || ({})

    function _refreshInfo() { _info = PluginManager ? PluginManager.plugin(pluginId) : ({}) }
    function _refreshState() { _state = PluginManager ? PluginManager.pluginState(pluginId) : ({}) }
    function _call(method, params) {
        if (!PluginManager) return
        _error = ""
        _busyMethod = method
        PluginManager.invoke(pluginId, method, params || {})
    }

    Component.onCompleted: { _refreshInfo(); _refreshState(); if (_running) _call("state") }
    onVisibleChanged: if (visible && _running) _call("state")
    on_RunningChanged: if (_running && visible) _call("state")

    Connections {
        target: PluginManager
        function onPluginsChanged() { live._refreshInfo() }
        function onPluginChanged(id) { if (id === live.pluginId) live._refreshInfo() }
        function onPluginStateChanged(id) { if (id === live.pluginId) live._refreshState() }
        function onInvokeFinished(id, method, ok, result) {
            if (id !== live.pluginId) return
            if (live._busyMethod === method) live._busyMethod = ""
            if (!ok) live._error = String(result || "")
            else if (method === "state" && result && typeof result === "object") live._state = result
        }
    }

    // ── Small parts ──────────────────────────────────────────────────────────
    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        font.pixelSize: Dimensions.body
        color: Theme.textSecondary
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: StyledScrollBar {
            parent: flick.parent
            anchors.top: flick.top; anchors.right: flick.right; anchors.bottom: flick.bottom
        }

        ColumnLayout {
            id: page
            width: flick.width
            spacing: 22

            PageHeader {
                Layout.topMargin: 52
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                title: live._info.screen && live._info.screen.title ? live._info.screen.title : "Makine Live"
                subtitle: qsTr("Oyundaki altyazıyı okur ve Türkçesini ekranın üzerine yazar.")
            }

            // ── The plugin is not there, off, or switched off remotely ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: !live._running
                title: qsTr("Durum")
                footer: live._pluginState === "remote-off"
                        ? ""
                        : qsTr("Eklentiyi Ayarlar > Eklentiler bölümünden de yönetebilirsin.")

                ListRow {
                    icon: Icons.info
                    iconTint: live._pluginState === "remote-off" || live._pluginState === "error"
                              || live._pluginState === "crashed" ? Theme.warning : Theme.textMuted
                    title: !live._exists ? qsTr("Makine Live bu sürümde yok")
                         : live._pluginState === "remote-off" ? qsTr("Makine Live şu an kapalı")
                         : !live._info.enabled ? qsTr("Makine Live kapalı")
                         : live._pluginState === "starting" ? qsTr("Makine Live açılıyor…")
                         : qsTr("Makine Live çalışmıyor")
                    subtitle: live._pluginState === "remote-off"
                              ? (live._info.lastError || qsTr("Kısa bir bakım var; birazdan yeniden açılacak."))
                              : (live._info.lastError || "")

                    AppButton {
                        visible: live._exists && !live._info.enabled
                        style: "filled"
                        text: qsTr("Aç")
                        onClicked: PluginManager.setEnabled(live.pluginId, true)
                    }
                    AppButton {
                        visible: live._exists && live._info.enabled
                                 && (live._pluginState === "error" || live._pluginState === "crashed")
                        style: "gray"
                        text: qsTr("Yeniden başlat")
                        onClicked: PluginManager.restart(live.pluginId)
                    }
                }
            }

            // ── Server notice ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && live._server.maintenance === true

                ListRow {
                    icon: Icons.info
                    iconTint: Theme.warning
                    title: qsTr("Sunucu bakımda")
                    subtitle: live._server.message || qsTr("Çeviri havuzu bir süre kullanılamıyor.")
                }
            }

            // ── Account ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Hesap")
                footer: live._login.code
                        ? qsTr("Tarayıcıda açılan sayfaya bu kodu gir; giriş kendiliğinden tamamlanır.")
                        : ""

                ListRow {
                    icon: Icons.user
                    title: live._signedIn ? live._login.user
                         : live._login.code ? qsTr("Kod: %1").arg(live._login.code)
                         : qsTr("Giriş yapılmadı")
                    subtitle: live._login.message ? live._login.message
                            : live._signedIn ? qsTr("makineceviri.org hesabınla giriş yaptın.")
                            : live._login.code ? qsTr("Giriş bekleniyor")
                            : qsTr("Çeviri havuzunu kullanmak için makineceviri.org hesabınla giriş yap.")

                    AppButton {
                        visible: !live._signedIn && !!live._login.code && !!live._login.url
                        style: "tinted"
                        text: qsTr("Tarayıcıda aç")
                        icon: Icons.externalLink
                        onClicked: PluginManager.openUrl(live._login.url)
                    }
                    AppButton {
                        visible: !live._signedIn && !!live._login.code
                        style: "gray"
                        text: qsTr("Vazgeç")
                        onClicked: live._call("login.cancel")
                    }
                    AppButton {
                        visible: !live._signedIn && !live._login.code
                        style: "filled"
                        text: qsTr("Giriş yap")
                        busy: live._busyMethod === "login.start"
                        onClicked: live._call("login.start")
                    }
                    AppButton {
                        visible: live._signedIn
                        style: "gray"
                        text: qsTr("Çıkış yap")
                        onClicked: live._call("logout")
                    }
                }
            }

            // ── Where the subtitle comes from ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Kaynak")
                footer: live._consoleMode
                        ? qsTr("Konsolun görüntüsü bir yakalama kartından gelir; listeden onu seç.")
                        : qsTr("Bilgisayarda oynadığın oyunun altyazısı ekrandan okunur.")

                ListRow {
                    icon: Icons.monitor
                    title: qsTr("Oynadığın yer")
                    SegmentedControl {
                        small: true
                        model: [{ value: "pc", text: qsTr("PC") }, { value: "console", text: qsTr("Konsol (GTA VI)") }]
                        currentValue: live._consoleMode ? "console" : "pc"
                        onActivated: (value) => live._call("mode.set", { mode: value })
                    }
                }
                ListRow {
                    visible: live._consoleMode
                    icon: Icons.refreshCw
                    title: qsTr("Görüntü kaynakları")
                    subtitle: live._sources.length === 0 ? qsTr("Henüz kaynak bulunmadı.") : ""
                    AppButton {
                        style: "gray"
                        text: qsTr("Yenile")
                        busy: live._busyMethod === "console.sources"
                        onClicked: live._call("console.sources")
                    }
                }
                Repeater {
                    model: live._consoleMode ? live._sources : []
                    ListRow {
                        required property var modelData
                        readonly property string sourceId: String(modelData.id !== undefined ? modelData.id : modelData)
                        title: modelData.label || modelData.title || sourceId
                        subtitle: live._selected && live._selected.key === modelData.key
                                  && live._selected.pid === modelData.pid ? qsTr("Seçili") : ""
                        clickable: true
                        onClicked: live._call("console.select", { sourceId: sourceId })
                    }
                }
                ListRow {
                    icon: Icons.scan
                    title: qsTr("Altyazı bölgesi")
                    subtitle: live._region && live._region.width > 0
                              ? qsTr("%1 × %2, ekranın %3, %4 noktasında")
                                    .arg(live._region.width).arg(live._region.height).arg(live._region.x).arg(live._region.y)
                              : qsTr("Altyazının çıktığı yeri seç.")
                    AppButton {
                        style: "gray"
                        text: live._region && live._region.width > 0 ? qsTr("Değiştir") : qsTr("Bölge seç")
                        busy: live._busyMethod === "region.pick"
                        onClicked: live._call("region.pick")
                    }
                }
                ListRow {
                    icon: Icons.languages
                    title: qsTr("Altyazı görünümü")
                    SegmentedControl {
                        small: true
                        model: [{ value: "classic", text: qsTr("Klasik") }, { value: "contrast", text: qsTr("Kontrast") },
                                { value: "band", text: qsTr("Bant") }, { value: "minimal", text: qsTr("Sade") }]
                        currentValue: (live._state.style && live._state.style.preset) || "classic"
                        onActivated: (value) => live._call("style.set", { preset: value })
                    }
                }
            }

            // ── Run ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Çeviri")
                footer: live._error || live._state.notice || ""

                ListRow {
                    icon: live._state.running ? Icons.squareStop : Icons.play
                    title: live._state.running ? qsTr("Çalışıyor") : qsTr("Durduruldu")
                    subtitle: live._state.status || ""
                    AppButton {
                        style: "gray"
                        text: qsTr("Bir kez oku")
                        visible: !live._state.running
                        busy: live._busyMethod === "readOnce"
                        onClicked: live._call("readOnce")
                    }
                    AppButton {
                        style: live._state.running ? "gray" : "filled"
                        text: live._state.running ? qsTr("Durdur") : qsTr("Başlat")
                        busy: live._busyMethod === "start" || live._busyMethod === "stop"
                        onClicked: live._call(live._state.running ? "stop" : "start")
                    }
                }
                ListRow {
                    icon: Icons.messageSquareText
                    title: live._lastLine.text || qsTr("Henüz satır yok")
                    subtitle: live._counters.lines !== undefined
                              ? qsTr("%1 satır · %2 tanesi havuzdan").arg(live._counters.lines).arg(live._counters.poolHits || 0)
                              : ""
                }
            }
        }
    }

    TopDissolve {
        flick: flick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
