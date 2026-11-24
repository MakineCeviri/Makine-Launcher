// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import MakineLauncher 1.0
import "../../components"
import "../../dialogs"
pragma ComponentBehavior: Bound

/**
 * MakineLiveScreen.qml - "Makine Live": the screen of the bundled Live plugin
 * (org.makineceviri.live; the state and methods are Live's README "Launcher
 * screen API", the contract docs/plugins/README.md).
 *
 * The plugin owns the logic; this page shows its latest `ui.state`
 * (PluginManager.pluginState) and calls its methods (PluginManager.invoke).
 * Built step by step, top to bottom (rebuilt 2026-11-24 after the product owner
 * got lost in his own setup):
 *   - header and ONE status card: what Live does now and the one next action
 *     (sign in / pick the console picture / Başlat · Durdur); the problem strip
 *     (maintenance, errors), the team's log request and the sign-in sit under it
 *   - "Şu an" (while reading) and "Bu oturum"
 *   1. Ne okunacak: PC or console, the console picture, the subtitle region
 *   2. Altyazı nerede görünsün: this PC | Makine TV | phone / tablet, OBS; the TV
 *      install is a one-screen wizard (TvInstallWizard.qml)
 *   - Oyun havuzları (PoolsSection.qml; hidden without state.pools)
 *   3. Görünüm (AppearanceSection.qml, loaded on demand; rare choices under Gelişmiş)
 *   - Diğer, collapsed: account, hotkeys, auto start, usage report, Discord,
 *     announcements, log, donation
 * The GTA VI look (console mode, dark theme): backdrop, status card, previews.
 *
 * Cheap while a game runs: no running animation; the state is read only while
 * the page can be seen (minimised: never, behind the game: every 2 s).
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
    readonly property string _loginState: _login.state || (_login.user ? "signedIn" : "signedOut")
    readonly property bool _signedIn: _loginState === "signedIn" || (_loginState === "offline" && !!_login.user)
    readonly property bool _consoleMode: _state.mode === "console"
    // The GTA VI look (docs/design.md "GTA VI görünümü"): console mode is GTA VI's.
    // Its light is made for the dark page, so the light theme keeps the stock page.
    readonly property bool _gta6: _consoleMode && Theme.darkMode
    // The backdrop picture comes from our CDN at run time (never bundled); false = palette only.
    // launcher.json `live.gta6Art` switches it without a release; not said (or not
    // read yet, offline) = this built-in default.
    readonly property bool _gta6ArtDefault: true
    readonly property bool _gta6ArtOn: UpdateService.liveGta6Art < 0 ? _gta6ArtDefault : UpdateService.liveGta6Art === 1
    // Through the image cache: downloaded once, asked about again every few days,
    // "" while offline or before the download lands (the sun stands alone)
    property string _gta6ArtSrc: ""
    function _resolveArt() { _gta6ArtSrc = _gta6 && _gta6ArtOn ? ImageCache.resolveUrl(_gta6Art[_gta6ArtIdx].url) : "" }
    on_Gta6Changed: _resolveArt()
    on_Gta6ArtOnChanged: _resolveArt()
    Connections {
        target: ImageCache
        enabled: live._gta6 && live._gta6ArtOn && live._gta6ArtSrc === ""
        function onImageReady() { live._resolveArt() }
    }
    // Official screenshots, sky in the upper half (the part the backdrop shows): a
    // sunset and the night skyline. One per visit; the next is picked while the page
    // is hidden, so nothing changes in front of the player. `gain` scales
    // Theme.gta6BackdropOpacity: the hazy sunset needs more than the lit skyline.
    readonly property var _gta6Art: [
        { url: "https://cdn.makineceviri.org/site/gta6/shots/port_gellhorn_06-1920.webp", gain: 1.6 },
        { url: "https://cdn.makineceviri.org/site/gta6/shots/vice_city_27-1920.webp", gain: 1.0 }
    ]
    property int _gta6ArtIdx: Math.floor(Math.random() * _gta6Art.length)
    onVisibleChanged: if (!visible) { _gta6ArtIdx = (_gta6ArtIdx + 1) % _gta6Art.length; _resolveArt() }
    readonly property var _region: _state.region ? _state.region[_consoleMode ? "console" : "pc"] || null : null
    readonly property bool _hasRegion: !!_region && _region.width > 0 && _region.height > 0
    readonly property var _selected: _state.selectedSource || null
    readonly property bool _hasSource: !!_selected
    readonly property var _selectedInfo: {
        for (let i = 0; i < _sources.length; ++i)
            if (_isSelected(_sources[i])) return _sources[i]
        return null
    }
    readonly property var _lastLine: _state.lastLine || ({})
    readonly property var _counters: _state.counters || ({})
    readonly property var _server: _state.server || ({})
    readonly property var _style: _state.style || ({})
    readonly property bool _reading: _state.running === true
    // PC: no region needed any more, Live finds the subtitle band itself (autoRegion)
    readonly property bool _sourceOk: _consoleMode ? _hasSource : true
    readonly property var _auto: _state.autoRegion || ({ state: "off" })
    readonly property string _autoText: _auto.state === "searching" ? qsTr("Altyazı aranıyor…")
        : _auto.state === "found" ? qsTr("Bulundu")
        : _auto.state === "locked" ? qsTr("Kilitlendi")
        : _auto.state === "manual" ? qsTr("Elle seçildi") : ""
    readonly property bool _ready: _loginState === "signedIn" && _sourceOk && _server.maintenance !== true
    readonly property var _announcements: Array.isArray(_state.announcements) ? _state.announcements : []
    // This session's lines, newest first (plugin keeps ~50): [{source, text, kind}]
    readonly property var _history: Array.isArray(_state.history) ? _state.history : []
    // Hotkeys: {armed, keys: {start|region|move: {key: "F9"|…|"off", active, error}}}
    readonly property var _hotkeys: (_state.hotkeys && _state.hotkeys.keys) || ({})
    function _hotkey(action, label) {
        const k = _hotkeys[action] || {}
        const key = k.key || ({ start: "F9", region: "F10", move: "F8" })[action]
        return key === "off" ? qsTr("%1: kapalı").arg(label) : qsTr("%1 %2").arg(key).arg(label)
    }
    readonly property string _hotkeyError: {
        const names = ["start", "region", "move"]
        for (let i = 0; i < names.length; ++i)
            if (_hotkeys[names[i]] && _hotkeys[names[i]].error) return _hotkeys[names[i]].error
        return ""
    }
    readonly property string _output: _consoleMode ? (_state.output || "pc") : "pc"
    readonly property var _web: _state.web || ({})
    readonly property var _tv: _state.tv || ({})
    readonly property var _install: _state.tvInstall || ({})

    // Kept apart from _state: every subtitle line publishes a new state, the source list
    // (and its rows) only changes when its content does
    property var _sources: []
    property string _sourcesKey: "[]"

    function _refreshInfo() { _info = PluginManager ? PluginManager.plugin(pluginId) : ({}) }
    function _applyState(st) {
        _state = st || ({})
        const key = JSON.stringify(_state.sources || [])
        if (key !== _sourcesKey) { _sourcesKey = key; _sources = _state.sources || [] }
        // Console is where most players are: the first visit starts there
        if (!seen.modeChosen && _state.mode !== undefined) {
            seen.modeChosen = true
            if (_state.mode !== "console") _call("mode.set", { mode: "console" })
        }
    }
    function _refreshState() { _applyState(PluginManager ? PluginManager.pluginState(pluginId) : ({})) }
    function _call(method, params) {
        if (!PluginManager) return
        _error = ""
        _busyMethod = method
        PluginManager.invoke(pluginId, method, params || {})
    }
    function _setStyle(changes) { _call("style.set", changes) }
    // Android TV / Fire TV install over ADB: a scanned host ("ip" or {host, port}) or a typed "ip[:port]"
    function _tvInstallAt(h) {
        if (typeof h !== "string") { _call("tv.install", { platform: "android", host: h.host, port: h.port }); return }
        const m = h.trim().match(/^(.+?)(?::(\d{1,5}))?$/)
        if (!m) return
        _call("tv.install", m[2] ? { platform: "android", host: m[1], port: Number(m[2]) } : { platform: "android", host: m[1] })
    }
    // LG webOS over the TV's Developer Mode: a scanned or typed address and the Passphrase on the TV
    function _tvInstallLg(host, passphrase) {
        _call("tv.install", { platform: "lg", host: String(host).trim(), passphrase: String(passphrase).trim() })
    }
    property string _copied: ""
    function _copy(text) { copyHelper.text = text; copyHelper.selectAll(); copyHelper.copy(); _copied = text; copiedTimer.restart() }
    Timer { id: copiedTimer; interval: 1500; onTriggered: live._copied = "" }

    // Counters' running time ticks only while reading and on screen
    property real _now: Date.now()
    Timer { interval: 1000; repeat: true; running: live._reading && live._onScreen; onTriggered: live._now = Date.now() }
    function _elapsed(startedAt) {
        if (!startedAt) return ""
        const ms = startedAt > 1e12 ? startedAt : startedAt * 1000
        const t = Math.max(0, Math.floor((_now - ms) / 1000))
        const two = (n) => (n < 10 ? "0" : "") + n
        return two(Math.floor(t / 3600)) + ":" + two(Math.floor(t / 60) % 60) + ":" + two(t % 60)
    }
    readonly property string _countersText: {
        const c = _counters
        const parts = [qsTr("%1 satır").arg(c.lines || 0)]
        const pct = c.hitPct !== undefined ? Math.round(c.hitPct) : (c.lines ? Math.round(100 * (c.poolHits || 0) / c.lines) : 0)
        parts.push(qsTr("%%1 havuzdan").arg(pct))
        if (c.avgMs) parts.push(qsTr("ort. %1 ms").arg(Math.round(c.avgMs)))
        if (c.startedAt && _reading) parts.push(_elapsed(c.startedAt))
        return parts.join(" · ")
    }
    readonly property var _newestAnnouncement: _announcements.length > 0 ? _announcements[0] : null
    function _isSelected(src) {
        return !!_selected && _selected.key === src.key && _selected.pid === src.pid
    }
    function _start() {
        if (_consoleMode && !seen.consoleWarned) { consoleConfirm.open(); return }
        _call("start")
    }
    function _scrollTo(item) {
        if (!item) return
        const y = item.mapToItem(page, 0, 0).y - 60
        flick.contentY = Math.max(0, Math.min(y, flick.contentHeight - flick.height))
    }

    // Opening the page is the plugin's first use: asking for its state starts its
    // host (PluginManager.invoke). Nothing runs before the page is opened.
    readonly property bool _wanted: visible && _exists && _info.enabled === true && _pluginState !== "remote-off"
    Component.onCompleted: { _refreshInfo(); _refreshState(); _resolveArt(); if (_wanted) _call("state") }
    on_WantedChanged: if (_wanted) _call("state")

    // Every subtitle line publishes a new state: read it only while the page can be seen.
    readonly property bool _onScreen: visible && Window.visibility !== Window.Minimized
                                      && Window.visibility !== Window.Hidden
    on_OnScreenChanged: if (_onScreen) { _refreshState(); _maybeShowBlock() }
    Window.onActiveChanged: if (Window.active && _onScreen) _refreshState()
    function _stateArrived() {
        if (!_onScreen) return
        if (Window.active) _refreshState()
        else if (!lazyRefresh.running) lazyRefresh.start()
    }
    Timer { id: lazyRefresh; interval: 2000; onTriggered: live._refreshState() }

    // Console mode with no source list yet: look once, the user should not have to
    property bool _askedSources: false
    readonly property bool _wantSources: _running && _consoleMode && _sources.length === 0 && !_askedSources
    on_WantSourcesChanged: if (_wantSources) { _askedSources = true; _call("console.sources") }

    Connections {
        target: PluginManager
        function onPluginsChanged() { live._refreshInfo() }
        function onPluginChanged(id) { if (id === live.pluginId) live._refreshInfo() }
        function onPluginStateChanged(id) { if (id === live.pluginId) live._stateArrived() }
        function onInvokeFinished(id, method, ok, result) {
            if (id !== live.pluginId) return
            if (live._busyMethod === method) live._busyMethod = ""
            // Console start needs the one-time GTA VI consent first
            if (!ok && method === "start" && String(result || "").indexOf("consent_required") >= 0) consoleConfirm.open()
            else if (!ok) live._error = String(result || "")
            else if (method === "state" && result && typeof result === "object") live._applyState(result)
        }
    }

    Settings {
        id: seen
        category: "makineLive"
        // The note on what Live reads, until the user closes it
        property bool introDone: false
        // The one-time "only while playing GTA VI" confirmation
        property bool consoleWarned: false
        // Console was offered as the first mode once
        property bool modeChosen: false
        // The firewall note was read before the first TV search / phone page
        property bool firewallSeen: false
    }

    // ── The one blocking problem, if any: {text, fix, fixText} ───────────────
    readonly property var _problem: {
        if (!_exists) return { text: qsTr("Makine Live bu sürümde yok."), fixText: "" }
        if (_pluginState === "crashed")
            return { text: qsTr("Makine Live beklenmedik şekilde kapandı."), fixText: qsTr("Yeniden başlat"), fix: "restart" }
        if (_pluginState === "error")
            return { text: _info.lastError || qsTr("Makine Live açılamadı."), fixText: qsTr("Yeniden başlat"), fix: "restart" }
        if (_pluginState === "remote-off")
            return { text: _info.lastError || qsTr("Makine Live kısa bir bakımda; birazdan yeniden açılacak."), fixText: "" }
        if (_info.enabled === false)
            return { text: qsTr("Makine Live kapalı."), fixText: qsTr("Aç"), fix: "enable" }
        if (_running && _server.maintenance === true)
            return { text: _server.message || qsTr("Çeviri havuzu bakımda; bakım bitince kendiliğinden devam eder."),
                     fixText: qsTr("Tekrar dene"), fix: "invoke:status.refresh" }
        const reasons = {
            source_missing: { text: qsTr("Konsol görüntüsü bulunamadı: Remote Play ya da yakalama kartının penceresi açık değil."),
                              fixText: qsTr("Yeniden ara"), fix: "sources" },
            black_frame:    { text: qsTr("Görüntü siyah geliyor: pencere küçültülmüş ya da başka bir pencerenin altında olabilir."),
                              fixText: qsTr("Yeniden ara"), fix: "sources" },
            stream_stalled: { text: qsTr("Görüntü dondu: Remote Play bağlantısı kopmuş olabilir."), fixText: qsTr("Yeniden ara"), fix: "sources" },
            stream_poor:    { text: qsTr("Görüntü kalitesi düşük; altyazı okunamayabilir. Remote Play'de kaliteyi yükselt."), fixText: "" },
            hdr_washed:     { text: qsTr("Renkler soluk geliyor: konsolda HDR açık. Okuma için HDR'yi kapat."), fixText: "" },
            tv_offline:     { text: qsTr("Makine TV'ye ulaşılamıyor: TV açık ve aynı ağda mı?"), fixText: "" },
            quota:          { text: qsTr("Bugünkü çeviri hakkın doldu; yarın yeniden açılır."), fixText: "" },
            consent_required: { text: qsTr("Konsol modunda okunan her satır GTA VI havuzuna yazılır; başlatmadan önce onayla."),
                                fixText: qsTr("Onayla") },
            game_unidentified: { text: qsTr("Çalışan oyun tanınamadı; oyunu launcher'dan başlat ya da Kütüphanem'e ekle."), fixText: "" }
        }
        // {code, text, action}: the plugin's own sentence; action is the method that fixes it
        const r = _state.reason
        if (_running && r && typeof r === "object" && r.code) {
            const known = reasons[r.code] || {}
            // No action, no button. Some actions are the launcher's own, not plugin methods.
            // consent_required comes without an action (the screen asks itself): its button opens that question
            const a = r.action || (r.code === "consent_required" ? "consent" : "")
            const local = { consent: "consent", "launcher.update": "update", update_launcher: "update",
                            "tv.pairForm": "tvForm", tv_pair: "tvForm", login: "login", retry: "invoke:status.refresh" }
            const labels = { consent: qsTr("Onayla"), update: qsTr("Güncelle"), tvForm: qsTr("TV'ye bağlan"),
                             login: qsTr("Giriş yap"), "invoke:status.refresh": qsTr("Tekrar dene") }
            const fix = a === "" ? "" : (local[a] || "invoke:" + a)
            return { text: r.text || known.text || r.code,
                     fixText: fix === "" ? "" : (labels[fix] || known.fixText || qsTr("Düzelt")),
                     fix: fix }
        }
        if (_running && typeof r === "string" && reasons[r]) return reasons[r]
        if (_error !== "") return { text: _error, fixText: "" }
        return null
    }
    // Is the place the subtitle goes to ready (the TV paired and reachable, the phone page up)?
    readonly property bool _whereOk: _output === "tv" ? _tv.state === "connected"
                                   : _output === "phone" ? !!_web.phoneUrl : true

    // The status card: what Live is doing and the ONE next action.
    // kind: start | stop | login | source | "" (no action)
    readonly property var _next: {
        const fixable = !!_problem && _problem.fixText !== ""
        if (!_exists)
            return { head: qsTr("Makine Live bu sürümde yok"), text: "", glyph: Icons.circleAlert, tint: Theme.textMuted,
                     actionText: "", kind: "", enabled: false }
        if (!_running) {
            const opening = _pluginState === "starting" || _pluginState === "idle"
            return { head: opening ? qsTr("Makine Live açılıyor…") : qsTr("Makine Live şu an çalışmıyor"),
                     text: !opening && fixable ? qsTr("Aşağıdaki uyarıdan düzeltebilirsin.") : "",
                     glyph: opening ? Icons.languages : Icons.circleAlert, tint: opening ? Theme.primary : Theme.warning,
                     actionText: "", kind: "", enabled: false }
        }
        if (_reading)
            return { head: qsTr("Okuyor"), text: _countersText, glyph: Icons.languages, tint: Theme.success,
                     actionText: qsTr("Durdur"), kind: "stop", enabled: true }
        if (_server.maintenance === true)
            return { head: qsTr("Çeviri havuzu bakımda"), text: _server.message || qsTr("Bakım bitince kendiliğinden devam eder."),
                     glyph: Icons.wrench, tint: Theme.warning, actionText: "", kind: "", enabled: false }
        if (!_signedIn) {
            const waiting = _loginState === "waiting" && !!_login.code
            return { head: waiting ? qsTr("Tarayıcıda kodu onayla") : qsTr("Önce giriş yap"),
                     text: waiting ? qsTr("Kod aşağıda yazıyor; onaylayınca Live devam eder.")
                                   : qsTr("Çeviri havuzunu kullanmak için Makine hesabı gerekir."),
                     glyph: Icons.user, tint: Theme.primary,
                     actionText: waiting ? "" : qsTr("Giriş yap"), kind: "login", enabled: true }
        }
        if (_consoleMode && !_hasSource)
            return { head: qsTr("Konsol görüntüsünü seç"),
                     text: qsTr("Remote Play ya da yakalama kartının penceresini aç, sonra aşağıdan seç."),
                     glyph: Icons.gamepad2, tint: Theme.primary, actionText: qsTr("Görüntüyü seç"), kind: "source", enabled: true }
        return { head: qsTr("Hazır"),
                 text: _whereOk ? qsTr("Başlat'a basınca altyazıyı okumaya başlar; Türkçesi oyunun üstünde çıkar.")
                                : qsTr("Makine TV'ye bağlı değil; altyazı TV'de çıkmaz. Aşağıdan TV'yi bağla ya da yine de başlat."),
                 glyph: Icons.play, tint: Theme.primary, actionText: qsTr("Başlat"), kind: "start", enabled: true }
    }
    function _runNext() {
        const k = _next.kind
        if (k === "start") _start()
        else if (k === "stop") _call("stop")
        else if (k === "login") _call("login.start")
        else if (k === "source") _scrollTo(readStep)
    }

    function _fix(kind) {
        if (kind === "restart") PluginManager.restart(pluginId)
        else if (kind === "enable") PluginManager.setEnabled(pluginId, true)
        else if (kind === "sources") _call("console.sources")
        else if (kind === "consent") consoleConfirm.open()
        else if (kind === "update") { if (typeof UpdateService !== "undefined" && UpdateService) UpdateService.check() }
        else if (kind === "tvForm") _scrollTo(whereSection)
        else if (kind === "login") _scrollTo(signCard)
        else if (kind && kind.indexOf("invoke:") === 0) _call(kind.slice(7))
    }

    // Clipboard: QML has none of its own
    TextEdit { id: copyHelper; visible: false }

    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        font.pixelSize: Dimensions.body
        color: Theme.textSecondary
        lineHeight: 1.2
    }

    // A ticked (or open) line: what is done, or what still has to be done
    component Need: RowLayout {
        id: need
        property bool done: false
        property string text: ""
        Layout.fillWidth: true
        spacing: 10
        Icon {
            glyph: need.done ? Icons.circleCheck : Icons.circle
            size: 16
            color: need.done ? Theme.success : Theme.textMuted
        }
        Text {
            Layout.fillWidth: true
            text: need.text
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            font.pixelSize: Dimensions.callout
            color: need.done ? Theme.textSecondary : Theme.textPrimary
        }
    }

    // A small status chip of the status card: tick / circle and a name
    component Chip: RowLayout {
        id: chip
        property bool done: false
        property string text: ""
        spacing: 6
        Icon {
            glyph: chip.done ? Icons.circleCheck : Icons.circle
            size: 14
            color: chip.done ? Theme.success : Theme.textMuted
        }
        Text {
            text: chip.text
            textFormat: Text.PlainText
            font.pixelSize: Dimensions.footnote
            font.weight: Font.DemiBold
            color: chip.done ? Theme.textSecondary : Theme.textMuted
        }
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

        // GTA VI look: a quiet backdrop behind the header and the status card, scrolling
        // with them. A low sun in the corner (ours); behind it, when the CDN answers, a
        // picture at Theme.gta6BackdropOpacity. Both dissolve into the page (EdgeFade),
        // so the text above keeps the page's own contrast.
        Item {
            id: backdrop
            width: flick.width
            height: Math.min(480, Math.round(flick.width * 0.5))
            opacity: live._gta6 ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

            Image {
                anchors.fill: parent
                source: live._gta6ArtSrc
                sourceSize.width: 1920
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                // Offline or removed from the CDN: the sun stands alone
                opacity: status === Image.Ready ? Theme.gta6BackdropOpacity * live._gta6Art[live._gta6ArtIdx].gain : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
            }
            Shape {
                anchors.fill: parent
                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: backdrop.width * 0.86; centerY: 0
                        centerRadius: backdrop.width * 0.55
                        focalX: centerX; focalY: centerY
                        GradientStop { position: 0.0; color: Theme.withAlpha(Theme.gta6Orange, 0.14) }
                        GradientStop { position: 0.45; color: Theme.withAlpha(Theme.gta6Pink, 0.06) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                    PathRectangle { width: backdrop.width; height: backdrop.height }
                }
            }
        }
        EdgeFade { target: backdrop; topFade: 40; bottomFade: Math.round(backdrop.height * 0.6) }

        ColumnLayout {
            id: page
            width: flick.width
            spacing: 24

            // ── Header: the name and one sentence ──
            PageHeader {
                Layout.topMargin: 52
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                title: "Makine Live"
                subtitle: qsTr("Oyundaki İngilizce altyazıyı okur, Türkçesini oyunun üstüne altyazı olarak yazar.")

                StatusBadge {
                    visible: live._exists
                    text: live._reading ? qsTr("Okuyor")
                        : live._running ? (live._ready ? qsTr("Hazır") : qsTr("Kurulum bekliyor"))
                        : live._pluginState === "starting" || live._pluginState === "idle" ? qsTr("Açılıyor")
                        : qsTr("Kapalı")
                    tint: live._reading ? Theme.success : live._running && live._ready ? Theme.primary : Theme.textMuted
                }
            }

            // ── The newest announcement, as a strip (the list stays under "Diğer") ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._newestAnnouncement !== null
                ListRow {
                    icon: Icons.bell
                    title: live._newestAnnouncement ? (live._newestAnnouncement.title || live._newestAnnouncement.kindLabel || "") : ""
                    subtitle: live._newestAnnouncement ? (live._newestAnnouncement.body || "") : ""
                    AppButton {
                        style: "plain"
                        text: qsTr("Kapat")
                        onClicked: live._call("announcement.read", { "id": live._newestAnnouncement["id"] })
                    }
                }
            }

            // ── Status card: what Live is doing, and the one next thing ──
            Card {
                id: statusCard
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._exists
                glow: true
                glowColor: live._gta6 ? Theme.gta6Pink : Theme.accentBase
                radius: Dimensions.radiusHero
                implicitHeight: statusCol.implicitHeight + 48

                // GTA VI look: the top edge catches the sun instead of white light
                Rectangle {
                    visible: live._gta6
                    x: Dimensions.radiusHero * 0.7
                    width: parent.width - Dimensions.radiusHero * 1.4
                    height: 1
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.25; color: Theme.gta6Sun[0] }
                        GradientStop { position: 0.5; color: Theme.gta6Sun[1] }
                        GradientStop { position: 0.75; color: Theme.gta6Sun[2] }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                ColumnLayout {
                    id: statusCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 26
                    anchors.rightMargin: 26
                    spacing: 18

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 18

                        // Our own mark in the sunset's colours (GTA VI look), else a tile for the state
                        Image {
                            visible: live._gta6
                            Layout.alignment: Qt.AlignTop
                            Layout.preferredWidth: 56
                            Layout.preferredHeight: 56
                            source: "qrc:/qt/qml/MakineLauncher/resources/images/logo_gta6.png"
                            sourceSize: Qt.size(256, 256)
                            mipmap: true
                            asynchronous: true
                        }
                        IconTile {
                            visible: !live._gta6
                            Layout.alignment: Qt.AlignTop
                            size: 48
                            glyph: live._next.glyph
                            tint: live._next.tint
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                visible: live._gta6
                                text: qsTr("GTA VI · PS5 / XBOX")
                                textFormat: Text.PlainText
                                font.family: Dimensions.fontDisplay
                                font.pixelSize: Dimensions.caption1
                                font.weight: Font.Bold
                                font.letterSpacing: 1.4
                                color: Theme.gta6Orange
                            }
                            Text {
                                Layout.fillWidth: true
                                text: live._next.head
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                                font.family: Dimensions.fontDisplay
                                font.pixelSize: Dimensions.title2
                                font.weight: Font.Bold
                                color: Theme.textPrimary
                            }
                            Body {
                                visible: live._next.text !== ""
                                text: live._next.text
                            }
                        }
                        AppButton {
                            visible: live._next.actionText !== ""
                            Layout.alignment: Qt.AlignVCenter
                            style: live._next.kind === "start" || live._next.kind === "stop" ? "filled" : "tinted"
                            size: "large"
                            text: live._next.actionText
                            icon: live._next.kind === "stop" ? Icons.squareStop : live._next.kind === "start" ? Icons.play : ""
                            enabled: live._next.enabled
                            busy: live._busyMethod === "start" || live._busyMethod === "stop" || live._busyMethod === "login.start"
                            toolTip: live._next.kind === "stop" ? qsTr("Okumayı durdurur; oyunun üstündeki altyazı kalkar.")
                                   : live._next.kind === "start" ? qsTr("Altyazıyı okumaya başlar; Türkçesi oyunun üstünde çıkar.") : ""
                            onClicked: live._runNext()
                        }
                    }

                    // The three things that make it work, ticked as they are done
                    RowLayout {
                        visible: live._running
                        Layout.fillWidth: true
                        spacing: 22
                        Chip { done: live._signedIn; text: qsTr("Giriş") }
                        Chip { done: live._sourceOk; text: qsTr("Ne okunacak") }
                        Chip { done: live._whereOk; text: qsTr("Altyazı yeri") }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ── Problem / maintenance strip: the most blocking one, one fix ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._problem !== null

                ListRow {
                    icon: Icons.triangleAlert
                    iconTint: Theme.warning
                    title: live._problem ? live._problem.text : ""
                    AppButton {
                        visible: !!live._problem && live._problem.fixText !== ""
                        style: "tinted"
                        text: live._problem ? live._problem.fixText : ""
                        onClicked: live._fix(live._problem.fix)
                    }
                }
            }

            // ── The team's log request: stays visible while it is there ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: !!live._state.support
                ListRow {
                    icon: Icons.scrollText
                    iconTint: Theme.success
                    title: qsTr("Kayıt ekibe gönderildi")
                    subtitle: live._state.support ? qsTr("%1 KB; sorunu inceleyip sana döneceğiz.").arg(live._state.support.sizeKb || 0) : ""
                    AppButton {
                        style: "plain"
                        text: qsTr("Kapat")
                        onClicked: live._call("support.dismiss")
                    }
                }
            }

            // ── What Live reads: once, the first time ──
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._exists && !seen.introDone
                implicitHeight: introCol.implicitHeight + 40
                radius: Dimensions.radiusSection

                ColumnLayout {
                    id: introCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        IconTile { glyph: Icons.shieldCheck; tint: Theme.success; size: 32 }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Makine Live neyi okur?")
                            textFormat: Text.PlainText
                            font.pixelSize: Dimensions.headline
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        AppButton {
                            style: "tinted"
                            size: "small"
                            text: qsTr("Anladım")
                            icon: Icons.check
                            onClicked: seen.introDone = true
                        }
                    }
                    Need { done: true; text: qsTr("Sen Başlat'a basmadan hiçbir şey okunmaz.") }
                    Need { done: true; text: qsTr("Yalnız seçtiğin altyazı bölgesini ya da konsol görüntüsünün penceresini okur; ekranın geri kalanına bakmaz.") }
                    Need { done: true; text: qsTr("Görüntü bilgisayarından çıkmaz. Okunan metin bilgisayarında çıkarılır.") }
                    Need { done: true; text: qsTr("Yalnız okunan İngilizce satır, Makine hesabınla makineceviri.org'daki ortak çeviri havuzuna gider; Türkçesi oradan gelir.") }
                    Need { done: true; text: qsTr("Kullanım bildirimi: oyun, mod, okuma süresi ve çevrilen satır sayısı gider; ekran görüntüsü ya da okunan metin gitmez. Sayfanın altından kapatabilirsin.") }
                }
            }

            // ── Sign-in (only until signed in; afterwards the account sits under "Diğer") ──
            // Device-code sign-in: a big code to type in the browser
            Card {
                id: signCard
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && !live._signedIn
                radius: Dimensions.radiusSection
                implicitHeight: signCol.implicitHeight + 44

                ColumnLayout {
                    id: signCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 14

                    readonly property bool waiting: live._loginState === "waiting" && !!live._login.code
                    readonly property bool preparing: live._loginState === "starting" || live._loginState === "checking"
                                                      || live._busyMethod === "login.start"

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        IconTile { glyph: Icons.user; size: 32 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: signCol.waiting ? qsTr("Tarayıcıda bu kodu onayla") : qsTr("Makine hesabınla giriş yap")
                                textFormat: Text.PlainText
                                font.pixelSize: Dimensions.headline
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Body {
                                font.pixelSize: Dimensions.callout
                                text: live._login.message ? live._login.message
                                    : live._loginState === "offline" ? qsTr("makineceviri.org'a ulaşılamıyor. Bağlantı gelince giriş kendiliğinden yenilenir.")
                                    : signCol.waiting ? qsTr("Açılan sayfada kod zaten yazılı gelir; onaylaman yeter. Giriş kendiliğinden tamamlanır.")
                                    : signCol.preparing ? qsTr("Giriş hazırlanıyor…")
                                    : qsTr("Çeviri havuzunu kullanmak için gerekir. Şifre yazmazsın: tarayıcıda bir kodu onaylarsın.")
                            }
                        }
                        AppButton {
                            visible: !signCol.waiting
                            style: "tinted"
                            text: qsTr("Giriş yap")
                            busy: signCol.preparing
                            enabled: !signCol.preparing
                            onClicked: live._call("login.start")
                        }
                    }

                    // The code, big enough to read across the room
                    Row {
                        visible: signCol.waiting
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 4
                        spacing: 8
                        Repeater {
                            model: String(live._login.code || "").split("")
                            Rectangle {
                                required property string modelData
                                width: modelData === "-" ? 18 : 44
                                height: 56
                                radius: Dimensions.radiusControl
                                color: modelData === "-" ? "transparent" : Theme.fill
                                border.width: modelData === "-" ? 0 : 1
                                border.color: Theme.containerBorder
                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    textFormat: Text.PlainText
                                    font.family: Dimensions.fontDisplay
                                    font.pixelSize: Dimensions.largeTitle
                                    font.weight: Font.Bold
                                    color: parent.modelData === "-" ? Theme.textMuted : Theme.textPrimary
                                }
                            }
                        }
                    }

                    RowLayout {
                        visible: signCol.waiting
                        Layout.fillWidth: true
                        spacing: 10
                        Spinner { size: 16 }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Tarayıcıda onaylamanı bekliyoruz…")
                            textFormat: Text.PlainText
                            font.pixelSize: Dimensions.footnote
                            color: Theme.textMuted
                        }
                        AppButton {
                            style: "gray"
                            text: qsTr("Vazgeç")
                            onClicked: live._call("login.cancel")
                        }
                        AppButton {
                            style: "tinted"
                            text: qsTr("Tarayıcıda aç")
                            icon: Icons.externalLink
                            visible: !!live._login.url
                            onClicked: PluginManager.openUrl(live._login.url)
                        }
                    }
                }
            }

            // ── Şu an: the subtitle as the game shows it (while reading) ──
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && (live._reading || !!live._lastLine.text)
                radius: Dimensions.radiusSection
                implicitHeight: nowCol.implicitHeight + 36

                ColumnLayout {
                    id: nowCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 10

                    Text {
                        text: qsTr("Şu an")
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.headline
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    // A neutral 16:9 scene (no game imagery): light to dark, so readability shows on both
                    Rectangle {
                        id: scene
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(nowCol.width, 720)
                        Layout.preferredHeight: Math.round(Layout.preferredWidth * 9 / 16)
                        radius: Dimensions.radiusLG
                        clip: true
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.textPrimary }
                            GradientStop { position: 0.5; color: Theme.textMuted }
                            GradientStop { position: 1.0; color: Theme.bgPrimary }
                        }
                        SunsetScene { active: live._gta6 }

                        // The subtitle band Live found (fraction of the game window / console video)
                        Rectangle {
                            readonly property var r: live._auto.rect || null
                            visible: !!r && (live._auto.state === "found" || live._auto.state === "locked")
                            x: r ? Math.round(r.x * scene.width) : 0
                            y: r ? Math.round(r.y * scene.height) : 0
                            width: r ? Math.round(r.width * scene.width) : 0
                            height: r ? Math.round(r.height * scene.height) : 0
                            color: "transparent"
                            radius: 4
                            border.width: 1
                            border.color: live._auto.state === "locked" ? Theme.success : Theme.primary
                        }
                        SubtitleBlock {
                            id: nowSubtitle
                            x: Math.round((scene.width - width) / 2)
                            y: (live._style.position || "region") === "top" ? 6
                             : (live._style.position || "region") === "bottom" ? scene.height - height - 6
                             : Math.round(scene.height * 0.78 - height / 2)
                            // The screen's proportions at a size that still reads on the page
                            textScale: 0.62
                            maxWidth: scene.width * ((live._style.widthPct || 62) / 100)
                            stretchWidth: scene.width * 0.92
                            animate: Dimensions.animFast > 0
                            style: live._style
                            lines: live._lastLine.text
                                   ? [{ text: live._lastLine.text, sourceText: live._lastLine.source || "", kind: live._lastLine.kind || "" }]
                                   : [{ text: qsTr("Arabayı arkaya çek, birazdan dönerim."), sourceText: "Pull the car around back, I'll be right out.", kind: "verified" }]
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: !live._lastLine.text ? qsTr("Örnek satır; oyunda bir altyazı çıkınca burada da görünür.")
                            : live._countersText + " · "
                              + (live._lastLine.kind === "verified" ? qsTr("onaylı çeviri") : qsTr("makine çevirisi, onay bekliyor"))
                        textFormat: Text.PlainText
                        font.family: Dimensions.fontText
                        font.pixelSize: Dimensions.caption1
                        color: Theme.textMuted
                    }
                }
            }

            // ── Bu oturum: the lines read so far ──
            ListSection {
                id: historySection
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && live._history.length > 0
                title: qsTr("Bu oturum")
                footer: qsTr("Bu oturumda okunan son satırlar; en yenisi üstte.")

                property bool all: false

                Repeater {
                    model: historySection.all ? live._history : live._history.slice(0, 5)
                    ListRow {
                        required property var modelData
                        icon: modelData.kind === "verified" ? Icons.circleCheck : Icons.languages
                        iconTint: modelData.kind === "verified" ? Theme.success : Theme.textMuted
                        title: modelData.text || ""
                        subtitle: (modelData.source || "")
                                  + "  ·  " + (modelData.kind === "verified" ? qsTr("Onaylı") : qsTr("Makine çevirisi"))
                    }
                }
                ListRow {
                    visible: live._history.length > 5
                    icon: Icons.history
                    title: historySection.all ? qsTr("Daha az göster") : qsTr("Hepsini göster (%1)").arg(live._history.length)
                    clickable: true
                    onClicked: historySection.all = !historySection.all
                }
            }

            // ═══ Step 1: Ne okunacak ═══
            LiveStep {
                id: readStep
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                number: 1
                title: qsTr("Ne okunacak")
                hint: live._consoleMode ? qsTr("PS5 ya da Xbox görüntüsünü bilgisayarda açtığın pencereden okur.")
                                        : qsTr("Bilgisayardaki oyunun altyazısını oyun öndeyken okur.")
                done: live._sourceOk

                ListSection {
                    Layout.fillWidth: true
                    footer: live._consoleMode
                            ? qsTr("Konsol modunda okunan her satır GTA VI havuzuna yazılır; bu yüzden yalnız GTA VI oynarken başlat.")
                            : ""

                    ListRow {
                        icon: live._consoleMode ? Icons.gamepad2 : Icons.laptop
                        title: qsTr("Oyun nerede çalışıyor?")
                        subtitle: live._reading ? qsTr("Değiştirmek için önce Durdur'a bas.")
                                : live._consoleMode ? qsTr("PS5 / Xbox görüntüsü bu bilgisayardaki bir pencerede.")
                                : (live._state.game && live._state.game.name)
                                  ? qsTr("%1 çalışıyor.").arg(live._state.game.name)
                                  : qsTr("Oyunu başlat; Live tanır.")
                        SegmentedControl {
                            small: true
                            model: [{ value: "console", text: qsTr("Konsolda") }, { value: "pc", text: qsTr("Bu bilgisayarda") }]
                            currentValue: live._consoleMode ? "console" : "pc"
                            // Locked while reading: switching mid-read would mix two sources
                            enabled: !live._reading
                            opacity: enabled ? 1 : 0.5
                            onActivated: (value) => { live._askedSources = false; live._call("mode.set", { mode: value }) }
                        }
                    }
                }

                // Console: step by step until one picture is chosen
                ListSection {
                    id: sourceSection
                    Layout.fillWidth: true
                    visible: live._consoleMode
                    title: qsTr("Konsol görüntüsü")
                    footer: live._hasSource && !sourceSection.open ? ""
                            : qsTr("Live bu pencereyi okur. Pencere küçültülmüş olmasın; başka bir pencerenin altında kalabilir.")

                    property bool open: false

                    // Done: one ticked row
                    ListRow {
                        visible: live._hasSource && !sourceSection.open
                        icon: Icons.circleCheck
                        iconTint: Theme.success
                        title: live._selectedInfo ? (live._selectedInfo.label || live._selectedInfo.title) : qsTr("Konsol görüntüsü seçili")
                        subtitle: live._selectedInfo && live._selectedInfo.title ? live._selectedInfo.title : ""
                        AppButton {
                            style: "gray"
                            text: qsTr("Değiştir")
                            onClicked: { sourceSection.open = true; live._call("console.sources") }
                        }
                    }

                    // Not yet: how to bring the console onto this PC
                    ListRow {
                        visible: !live._hasSource || sourceSection.open
                        icon: Icons.gamepad2
                        title: qsTr("Konsolu bilgisayara getir")
                        subtitle: qsTr("PS5: PS Remote Play, chiaki ya da Asobi. Xbox: Xbox uygulaması ya da Greenlight. Yakalama kartı: kartın programı (Elgato, OBS önizlemesi, Windows Kamera). Konsoluna bağlan, GTA VI'yı orada başlat.")
                    }
                    Repeater {
                        model: live._consoleMode && (!live._hasSource || sourceSection.open) ? live._sources : []
                        ListRow {
                            id: srcRow
                            required property var modelData
                            readonly property bool chosen: live._isSelected(modelData)
                            icon: modelData.console === "capture" ? Icons.monitor
                                : modelData.console === "custom" ? Icons.appWindow : Icons.gamepad2
                            iconTint: chosen ? Theme.success : Theme.primary
                            title: modelData.label || modelData.title || String(modelData.id)
                            subtitle: modelData.console === "ps5" ? qsTr("PS5 · %1").arg(modelData.title || modelData.exe || "")
                                    : modelData.console === "xbox" ? qsTr("Xbox · %1").arg(modelData.title || modelData.exe || "")
                                    : modelData.console === "capture" ? qsTr("Yakalama kartı · %1").arg(modelData.title || modelData.exe || "")
                                    : (modelData.title || modelData.exe || "")
                            clickable: !chosen
                            showChevron: false
                            onClicked: { live._call("console.select", { sourceId: String(modelData.id) }); sourceSection.open = false }
                            Text {
                                text: srcRow.chosen ? qsTr("Seçili") : qsTr("Seç")
                                textFormat: Text.PlainText
                                font.pixelSize: Dimensions.footnote
                                font.weight: Font.DemiBold
                                color: srcRow.chosen ? Theme.success : Theme.primaryText
                            }
                        }
                    }
                    ListRow {
                        visible: !live._hasSource || sourceSection.open
                        icon: live._sources.length === 0 ? Icons.circleAlert : Icons.refreshCw
                        iconTint: live._sources.length === 0 ? Theme.warning : Theme.primary
                        title: live._sources.length === 0 ? qsTr("Henüz bir konsol görüntüsü bulunamadı") : qsTr("Listede yok mu?")
                        subtitle: qsTr("Programı açtıktan sonra Yenile'ye bas; başka bir programdaysa Başka pencere'den seç.")
                        AppButton {
                            style: "plain"
                            text: qsTr("Başka pencere")
                            toolTip: qsTr("Açık bütün büyük pencereleri listeler.")
                            onClicked: live._call("console.sources", { all: true })
                        }
                        AppButton {
                            style: "gray"
                            text: qsTr("Yenile")
                            busy: live._busyMethod === "console.sources"
                            onClicked: live._call("console.sources")
                        }
                    }
                }

                // Altyazı bölgesi
                ListSection {
                    Layout.fillWidth: true
                    title: qsTr("Altyazı bölgesi")
                    footer: qsTr("Seçmezsen Live altyazının çıktığı bandı okurken kendisi bulur. Elle çizeceksen satırlara sıkı çiz: ne kadar dar, o kadar hızlı ve doğru.")

                    // Live finds the subtitle band itself
                    ListRow {
                        visible: live._auto.state !== "off"
                        icon: live._auto.state === "locked" || live._auto.state === "manual" ? Icons.circleCheck : Icons.scanSearch
                        iconTint: live._auto.state === "locked" || live._auto.state === "manual" ? Theme.success : Theme.primary
                        title: live._autoText
                        subtitle: live._auto.state === "searching" ? qsTr("Okurken altyazının çıktığı bant aranıyor; birkaç satır yeter.")
                                : live._auto.state === "found" ? qsTr("Altyazı bandı bulundu ve okunuyor; Kilitle dersen bu oyun için saklanır.")
                                : live._auto.state === "locked" ? qsTr("Bu oyunun altyazı bandı saklandı; Live artık aramıyor.")
                                : qsTr("Senin çizdiğin bölge okunuyor.")
                        AppButton {
                            visible: live._auto.state === "found"
                            style: "tinted"
                            text: qsTr("Kilitle")
                            busy: live._busyMethod === "autoRegion.accept"
                            onClicked: live._call("autoRegion.accept")
                        }
                        AppButton {
                            visible: live._auto.state === "found" || live._auto.state === "locked" || live._auto.state === "manual"
                            style: "gray"
                            text: qsTr("Yeniden ara")
                            onClicked: live._auto.state === "manual" || live._hasRegion ? resetConfirm.open()
                                                                                         : live._call("autoRegion.reset")
                        }
                    }

                    // Lines were read clipped, so this session's region grew: keep it?
                    ListRow {
                        readonly property var grow: live._state.regionGrow || ({})
                        visible: (grow.left || 0) > 0 || (grow.right || 0) > 0 || (grow.top || 0) > 0 || (grow.bottom || 0) > 0
                        icon: Icons.maximize2
                        iconTint: Theme.warning
                        title: qsTr("Satırlar kesik okunduğu için bölge genişletildi")
                        subtitle: qsTr("Bu oturumda geçerli; her seferinde böyle kalsın istiyorsan kalıcı yap.")
                        AppButton {
                            style: "tinted"
                            text: qsTr("Kalıcı yap")
                            busy: live._busyMethod === "region.keepGrown"
                            onClicked: live._call("region.keepGrown")
                        }
                    }
                    ListRow {
                        icon: live._hasRegion ? Icons.circleCheck : Icons.scan
                        iconTint: live._hasRegion ? Theme.success : Theme.primary
                        title: live._state.picking ? qsTr("Bölge seçiliyor…")
                             : live._hasRegion ? (live._consoleMode ? qsTr("Görüntüdeki altyazı yeri seçili")
                                                                    : qsTr("%1 × %2 piksel seçili").arg(live._region.width).arg(live._region.height))
                             : qsTr("Bölge seçilmedi")
                        subtitle: live._state.picking ? qsTr("Ekranda altyazının çıktığı yeri sürükleyerek çiz; Esc vazgeçer.")
                                : live._hasRegion ? qsTr("Live yalnız bu dikdörtgenin içini okur.")
                                : qsTr("Oyunda altyazının çıktığı yeri seç.")
                        AppButton {
                            style: live._hasRegion ? "gray" : "tinted"
                            text: live._hasRegion ? qsTr("Değiştir") : qsTr("Bölge seç")
                            enabled: !live._state.picking && (!live._consoleMode || live._hasSource)
                            busy: live._busyMethod === "region.pick"
                            onClicked: live._call("region.pick")
                        }
                    }
                    ListRow {
                        visible: live._signedIn && !live._reading
                        icon: Icons.scanSearch
                        title: qsTr("Bir kez oku")
                        subtitle: qsTr("Başlatmadan, ekrandaki altyazıyı bir kez okuyup çevirir; bölgeyi denemek için.")
                        AppButton {
                            style: "gray"
                            text: qsTr("Oku")
                            enabled: live._sourceOk
                            busy: live._busyMethod === "readOnce"
                            onClicked: live._call("readOnce")
                        }
                    }
                }
            }

            // ═══ Step 2: Altyazı nerede görünsün ═══
            LiveStep {
                id: whereSection
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                number: 2
                title: qsTr("Altyazı nerede görünsün")
                hint: live._consoleMode ? qsTr("Bir yer seç; yalnız onun kurulumu aşağıda görünür.")
                                        : qsTr("Bilgisayardaki oyunda altyazı oyunun üstünde çıkar.")
                done: live._whereOk

                // Where: this PC | Makine TV | phone / tablet (console); OBS on top, any mode
                ListSection {
                    Layout.fillWidth: true
                    visible: live._consoleMode

                    ListRow {
                        icon: live._output === "tv" ? Icons.monitor : live._output === "phone" ? Icons.smartphone : Icons.laptop
                        title: qsTr("Altyazı nerede çıksın?")
                        subtitle: live._output === "tv" ? qsTr("Makine TV uygulamasında, TV'nin üstünde.")
                                : live._output === "phone" ? qsTr("Telefonun ya da tabletin tarayıcısında.")
                                : qsTr("Bu ekranda, Remote Play penceresinin üstünde.")
                        SegmentedControl {
                            small: true
                            model: [{ value: "pc", text: qsTr("Bu bilgisayar") }, { value: "tv", text: qsTr("Makine TV") },
                                    { value: "phone", text: qsTr("Telefon / tablet") }]
                            currentValue: live._output
                            onActivated: (value) => live._call("output.set", { output: value })
                        }
                    }
                }

                // Firewall: said once, before the first TV search or phone page
                ListSection {
                    Layout.fillWidth: true
                    visible: live._consoleMode && (live._output === "tv" || live._output === "phone") && !seen.firewallSeen
                             && !!(live._web.firewallHeadsUp || live._web.firewallNote || live._tv.firewallNote)
                    ListRow {
                        icon: Icons.shield
                        iconTint: Theme.warning
                        title: qsTr("Windows bir izin sorabilir")
                        subtitle: live._web.firewallHeadsUp || live._web.firewallNote || live._tv.firewallNote || ""
                        AppButton {
                            style: "tinted"
                            text: qsTr("Anladım")
                            onClicked: seen.firewallSeen = true
                        }
                    }
                }

                // ── Makine TV: paired, or "TV'yi bağla" ──
                ListSection {
                    Layout.fillWidth: true
                    visible: live._consoleMode && live._output === "tv" && !!live._tv.paired
                    title: qsTr("Makine TV")
                    ListRow {
                        icon: live._tv.state === "connected" ? Icons.monitorCheck : Icons.wifiOff
                        iconTint: live._tv.state === "connected" ? Theme.success : Theme.warning
                        title: (live._tv.paired && live._tv.paired.name) || qsTr("Makine TV")
                        subtitle: live._tv.state === "connected" ? qsTr("Bağlı; altyazı TV'de çıkar.")
                                : live._tv.state === "connecting" ? qsTr("Bağlanıyor…")
                                : (live._tv.detail || qsTr("TV'ye ulaşılamıyor: TV açık ve aynı ağda mı?"))
                        AppButton {
                            style: "gray"
                            text: qsTr("Bağlantıyı kaldır")
                            onClicked: live._call("tv.unpair")
                        }
                    }
                }

                ListSection {
                    id: tvConnect
                    Layout.fillWidth: true
                    visible: live._consoleMode && live._output === "tv" && !live._tv.paired
                    title: qsTr("TV'yi bağla")
                    footer: qsTr("TV'de Makine TV'yi aç; ekranındaki 4 haneli kodu yaz.")

                    ListRow {
                        icon: Icons.monitor
                        title: qsTr("Makine TV'yi bul")
                        subtitle: live._tv.state === "bad_code" ? qsTr("Kod tutmadı; TV'deki 4 haneli kodu yeniden yaz.")
                                : qsTr("TV açık ve bu bilgisayarla aynı ağda olsun.")
                        AppButton {
                            style: "tinted"
                            text: qsTr("Ara")
                            busy: live._tv.searching === true || live._busyMethod === "tv.discover"
                            enabled: seen.firewallSeen || !(live._web.firewallHeadsUp || live._web.firewallNote || live._tv.firewallNote)
                            onClicked: live._call("tv.discover")
                        }
                    }
                    Repeater {
                        model: live._consoleMode && live._output === "tv" && !live._tv.paired && Array.isArray(live._tv.found)
                               ? live._tv.found : []
                        ListRow {
                            id: tvRow
                            required property var modelData
                            icon: Icons.monitor
                            title: modelData.name || modelData.host || ""
                            subtitle: modelData.host || ""
                            CodeField {
                                id: tvCode
                                implicitWidth: 90
                                placeholderText: qsTr("Kod")
                                maximumLength: 4
                                inputMethodHints: Qt.ImhDigitsOnly
                            }
                            AppButton {
                                style: "tinted"
                                text: qsTr("Bağlan")
                                enabled: tvCode.text.length === 4
                                busy: live._busyMethod === "tv.pair"
                                onClicked: live._call("tv.pair", { "id": tvRow.modelData["id"], code: tvCode.text })
                            }
                        }
                    }
                    ListRow {
                        icon: Icons.link
                        title: qsTr("Adresle bağlan")
                        subtitle: qsTr("TV bulunamazsa: TV'deki Makine TV'nin gösterdiği IP adresini ve 4 haneli kodu yaz.")
                        CodeField {
                            id: tvHost
                            implicitWidth: 140
                            placeholderText: "192.168.1.20"
                        }
                        CodeField {
                            id: tvHostCode
                            implicitWidth: 80
                            placeholderText: qsTr("Kod")
                            maximumLength: 4
                            inputMethodHints: Qt.ImhDigitsOnly
                        }
                        AppButton {
                            style: "tinted"
                            text: qsTr("Bağlan")
                            enabled: tvHost.text.length > 6 && tvHostCode.text.length === 4
                            busy: live._busyMethod === "tv.pair"
                            onClicked: live._call("tv.pair", { host: tvHost.text.trim(), code: tvHostCode.text })
                        }
                    }
                    // Not installed yet: opens the install wizard
                    ListRow {
                        icon: Icons.download
                        title: qsTr("TV'de Makine TV yok mu? Kur")
                        subtitle: live._install.state === "running" ? (live._install.step || qsTr("Kuruluyor…"))
                                : qsTr("Android TV, Google TV, Fire TV ve LG TV'ye buradan, adım adım kurulur.")
                        clickable: true
                        onClicked: tvWizard.open()
                    }
                }

                // ── Telefon / tablet ──
                ListSection {
                    Layout.fillWidth: true
                    visible: live._consoleMode && live._output === "phone"
                    title: qsTr("Telefon / tablet")
                    ListRow {
                        icon: Icons.smartphone
                        title: live._web.phoneUrl || qsTr("Telefon sayfası açılıyor…")
                        subtitle: live._web.error ? live._web.error
                                : qsTr("Telefonun tarayıcısında bu adresi aç; telefon bu bilgisayarla aynı ağda olmalı.")
                                  + (live._web.clients ? " " + qsTr("%1 cihaz bağlı.").arg(live._web.clients) : "")
                        AppButton {
                            visible: !!live._web.phoneUrl
                            style: "gray"
                            text: live._copied !== "" && live._copied === live._web.phoneUrl ? qsTr("Kopyalandı") : qsTr("Kopyala")
                            icon: live._copied !== "" && live._copied === live._web.phoneUrl ? Icons.check : Icons.copy
                            onClicked: live._copy(live._web.phoneUrl)
                        }
                    }
                }

                // ── OBS and the stream, any mode ──
                ListSection {
                    Layout.fillWidth: true
                    title: qsTr("OBS ve yayın")

                    ListRow {
                        icon: Icons.layers
                        title: qsTr("OBS kaynağı")
                        subtitle: live._state.obs === true && live._web.obsUrl
                                  ? qsTr("OBS'te Tarayıcı kaynağı olarak ekle: %1").arg(live._web.obsUrl)
                                  : qsTr("Altyazıyı OBS'e ayrı bir katman olarak verir; yalnız bu bilgisayar görür.")
                        AppButton {
                            visible: live._state.obs === true && !!live._web.obsUrl
                            style: "plain"
                            text: live._copied !== "" && live._copied === live._web.obsUrl ? qsTr("Kopyalandı") : qsTr("Kopyala")
                            onClicked: live._copy(live._web.obsUrl)
                        }
                        AppSwitch {
                            checked: live._state.obs === true
                            accessibleName: qsTr("OBS kaynağı")
                            onToggled: live._call("obs.set", { enabled: !(live._state.obs === true) })
                        }
                    }
                    ListRow {
                        icon: Icons.eye
                        title: qsTr("Yayında göster")
                        subtitle: live._style.showOnStream !== true
                                  ? qsTr("Kapalıyken altyazı yayına (OBS, Discord) ve ekran görüntülerine girmez; Live da onu okumaz.")
                                  : live._consoleMode ? qsTr("Altyazı yayında ve ekran görüntülerinde de görünür.")
                                  : qsTr("Altyazı yayında görünür. Bandın altındaki İngilizce satır yeniden okunabilir; Live kendi çevirisini ayıklar.")
                        AppSwitch {
                            checked: live._style.showOnStream === true
                            accessibleName: qsTr("Yayında göster")
                            onToggled: live._setStyle({ showOnStream: !(live._style.showOnStream === true) })
                        }
                    }
                }
            }

            // ═══ Oyun havuzları (hidden when the plugin does not send state.pools) ═══
            PoolsSection {
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                screen: live
                visible: live._running && !!live._state.pools
            }

            // ═══ Step 3: Görünüm (loaded when the plugin runs) ═══
            LiveStep {
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                number: 3
                title: qsTr("Görünüm")
                hint: qsTr("Altyazının biçimi, boyu ve yeri.")

                Loader {
                    Layout.fillWidth: true
                    active: live._running
                    asynchronous: true
                    sourceComponent: Component { AppearanceSection { screen: live } }
                }
            }

            // ═══ Diğer: collapsed ═══
            LiveFold {
                id: otherFold
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._exists
                icon: Icons.ellipsis
                title: qsTr("Diğer")
                subtitle: qsTr("Hesap, kısayollar, otomatik açılma, kullanım bildirimi, Discord, duyurular, günlük")

                ListSection {
                    Layout.fillWidth: true

                    // First run: a short donation note, closed for good with Kapat
                    ListRow {
                        visible: !!live._state.donate && live._state.donate.seen === false
                        icon: Icons.handHeart
                        iconTint: Theme.primary
                        title: qsTr("Live işine yaradıysa")
                        subtitle: qsTr("Sunucu ve çeviri havuzu bağışlarla ayakta; küçük bir destek de yeter.")
                        AppButton {
                            style: "tinted"
                            text: qsTr("Destek ol")
                            onClicked: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
                        }
                        AppButton {
                            style: "plain"
                            text: qsTr("Kapat")
                            onClicked: live._call("donate.dismiss")
                        }
                    }

                    // The account (signed out: the sign-in card above)
                    ListRow {
                        visible: live._running && live._signedIn
                        icon: Icons.circleCheck
                        iconTint: Theme.success
                        title: (live._login.user || qsTr("Giriş yapıldı"))
                               + (live._login.role ? "  ·  " + (({ admin: qsTr("Yönetici"), translator: qsTr("Çevirmen"),
                                                                    tester: qsTr("Test") })[live._login.role] || live._login.role) : "")
                        subtitle: (live._loginState === "offline"
                                   ? qsTr("Sunucuya şu an ulaşılamıyor; hesabın duruyor.")
                                   : qsTr("Çeviriler bu hesapla havuzdan gelir."))
                                  + (live._info.version ? "  " + qsTr("Makine Live %1").arg(live._info.version) : "")
                        AppButton {
                            style: "gray"
                            text: qsTr("Çıkış yap")
                            onClicked: live._call("logout")
                        }
                    }
                    ListRow {
                        icon: Icons.keyboard
                        title: qsTr("Kısayollar")
                        subtitle: (live._hotkeyError !== "" ? live._hotkeyError + " "
                                   : [live._hotkey("start", qsTr("başlatır / durdurur")), live._hotkey("region", qsTr("bölge seçtirir")),
                                      live._hotkey("move", qsTr("kutuyu taşır"))].join(" · ") + ". ")
                                  + qsTr("Oyun açıkken bu tuşlar başka programlara gitmez; oyun F9'u kullanıyorsa (hızlı kayıt gibi) eklenti ayarlarından başka bir tuş seç ya da kapat.")
                        iconTint: live._hotkeyError !== "" ? Theme.warning : Theme.primary
                    }
                    ListRow {
                        visible: live._running
                        icon: Icons.play
                        title: qsTr("Oyun başlayınca kendiliğinden aç")
                        subtitle: qsTr("Kütüphanendeki bir oyunu başlatınca Live okumaya hazır bekler.")
                        AppSwitch {
                            checked: live._state.autoStart === true
                            accessibleName: qsTr("Oyun başlayınca kendiliğinden aç")
                            onToggled: live._call("autoStart.set", { enabled: !(live._state.autoStart === true) })
                        }
                    }
                    ListRow {
                        visible: live._running
                        icon: Icons.activity
                        title: qsTr("Kullanım bildirimi")
                        subtitle: qsTr("Oyun, mod, okuma süresi ve çevrilen satır sayısı; ekran görüntüsü ya da okunan metin gitmez.")
                        AppSwitch {
                            checked: live._state.usageReport !== false
                            accessibleName: qsTr("Kullanım bildirimi")
                            onToggled: live._call("usageReport.set", { enabled: live._state.usageReport === false })
                        }
                    }
                    ListRow {
                        icon: Icons.discord
                        title: qsTr("Discord")
                        subtitle: live._state.discord && live._state.discord.connected
                                  ? qsTr("Bağlı: okurken profilinde Türkçe oynadığın görünür.")
                                  : qsTr("Discord açıksa okurken profilinde Türkçe oynadığın görünür.")
                    }
                    ListRow {
                        icon: Icons.scrollText
                        title: qsTr("Günlüğü aç")
                        subtitle: qsTr("Makine Live'ın son kayıtları; bir sorunu bildirirken işe yarar. Ayarlar › Eklentiler'de de var.")
                        clickable: true
                        onClicked: logDialog.open()
                    }
                    ListRow {
                        icon: Icons.handHeart
                        title: qsTr("Makine Çeviri'ye destek ol")
                        subtitle: qsTr("Live'ın sunucusu ve çeviri havuzu bağışlarla ayakta.")
                        clickable: true
                        onClicked: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
                    }
                }

                // Every announcement, newest first
                ListSection {
                    Layout.fillWidth: true
                    visible: live._announcements.length > 0
                    title: qsTr("Duyurular")
                    Repeater {
                        model: live._announcements
                        ListRow {
                            id: annRow
                            required property var modelData
                            icon: Icons.bell
                            title: modelData.title || modelData.kindLabel || ""
                            subtitle: modelData.body || ""
                            AppButton {
                                style: "plain"
                                text: qsTr("Kapat")
                                onClicked: live._call("announcement.read", { "id": annRow.modelData["id"] })
                            }
                        }
                    }
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

    // The TV install wizard: one thing on screen at a time
    TvInstallWizard {
        id: tvWizard
        parent: Overlay.overlay
        screen: live
        onPairRequested: live._scrollTo(tvConnect)
    }

    // The plugin's log (the same lines Settings › Eklentiler shows)
    BaseDialog {
        id: logDialog
        parent: Overlay.overlay
        width: 640
        title: qsTr("Makine Live günlüğü")
        property var lines: []
        onOpened: lines = PluginManager ? PluginManager.logLines(live.pluginId) : []
        contentItem: ScrollView {
            implicitHeight: 360
            clip: true
            TextArea {
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: "Consolas"
                font.pixelSize: Dimensions.footnote
                color: Theme.textSecondary
                background: null
                text: logDialog.lines.length > 0 ? logDialog.lines.join("\n") : qsTr("Henüz kayıt yok.")
            }
        }
    }

    // autoRegion.reset also forgets a hand-drawn region
    ConfirmDialog {
        id: resetConfirm
        parent: Overlay.overlay
        width: 420
        title: qsTr("Altyazı yeniden aransın mı?")
        message: qsTr("Senin çizdiğin bölge de silinir; Live altyazının bandını baştan arar.")
        confirmText: qsTr("Yeniden ara")
        glyph: Icons.scanSearch
        accentColor: Theme.primary
        onConfirmed: live._call("autoRegion.reset")
    }

    // ── Maintenance / an unsupported version: a dialog in the middle, not just the status card ──
    // Shown once per episode (code + text); a new maintenance or a new message asks again.
    readonly property string _blockCode: {
        const r = _state.reason
        return _running && r && typeof r === "object" && (r.code === "maintenance" || r.code === "update_required") ? r.code : ""
    }
    readonly property string _blockKey: _blockCode === "" ? "" : _blockCode + "|" + (_state.reason.text || "")
    property string _blockSeen: ""
    function _maybeShowBlock() {
        if (_blockKey === "") { _blockSeen = ""; if (blockDialog.opened) blockDialog.close(); return }
        if (!_onScreen || _blockKey === _blockSeen || blockDialog.opened) return
        _blockSeen = _blockKey
        donateDialog.close()
        blockDialog.open()
    }
    on_BlockKeyChanged: _maybeShowBlock()

    ConfirmDialog {
        id: blockDialog
        parent: Overlay.overlay
        width: 460
        readonly property bool update: live._blockCode === "update_required"
        title: update ? qsTr("Makine Live'ın bu sürümü artık çalışmıyor") : qsTr("Makine Live şu an bakımda")
        message: (live._state.reason && live._state.reason.text ? live._state.reason.text + "\n\n" : "")
                 + (update ? qsTr("Çeviri havuzu bu sürümü kabul etmiyor. Makine'yi güncelle; Live onunla birlikte güncellenir.")
                           : qsTr("Okuma bakım süresince duruyor. Bakım bitince Live kendiliğinden devam eder; bu sayfayı açık tutman gerekmez."))
        confirmText: update ? qsTr("Makine'yi güncelle") : qsTr("Tekrar dene")
        cancelText: qsTr("Tamam")
        glyph: update ? Icons.circleArrowUp : Icons.wrench
        accentColor: Theme.warning
        onConfirmed: update ? live._fix("update") : live._call("status.refresh")
    }

    // ── First time Live is opened: a word about support (Kreosus), once ──
    readonly property bool _wantDonate: _onScreen && _running && !!_state.donate && _state.donate.seen === false
                                        && _blockKey === ""
    on_WantDonateChanged: if (_wantDonate) donateDelay.restart()
    Timer {
        id: donateDelay
        interval: 1200   // let the page settle first
        onTriggered: if (live._wantDonate && !blockDialog.opened && !consoleConfirm.opened) donateDialog.open()
    }
    ConfirmDialog {
        id: donateDialog
        parent: Overlay.overlay
        width: 460
        title: qsTr("Makine Live gönüllü ve ücretsiz bir proje")
        message: qsTr("Sunucu giderlerini kendi cebimizden karşılıyoruz. Bağış yapmak istersen hem sunucunun açık kalmasına hem de motivasyonumuza destek olursun. Kreosus sayfamızdan dilediğin kadar destek olabilirsin; Live her durumda ücretsiz kalacak.")
        confirmText: qsTr("Kreosus'ta destek ol")
        cancelText: qsTr("Belki sonra")
        glyph: Icons.handHeart
        accentColor: Theme.primary
        onConfirmed: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
        onClosed: if (live._state.donate && live._state.donate.seen === false) live._call("donate.dismiss")
    }

    // First console start: the pool is GTA VI's, every line goes there
    ConfirmDialog {
        id: consoleConfirm
        parent: Overlay.overlay
        width: 440
        title: qsTr("Yalnız GTA VI oynarken başlat")
        message: qsTr("Konsol modunda okunan her satır GTA VI'nın ortak çeviri havuzuna yazılır. Başka bir oyunun altyazısı havuzu bozar.")
        confirmText: qsTr("Anladım, başlat")
        glyph: Icons.gamepad2
        accentColor: Theme.primary
        onConfirmed: {
            seen.consoleWarned = true
            live._call("consoleConsent.set", { granted: true })
            live._call("start")
        }
    }
}
