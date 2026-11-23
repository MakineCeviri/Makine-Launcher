// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
 * Order (Fable, 2026-11-24), console players first:
 *   1. header: one sentence, a status badge and the page's only filled button
 *      (Başlat / Durdur)
 *   2. a problem strip, only when something blocks: the most blocking reason,
 *      one sentence, one fix
 *   3. hero: "Nerede oynuyorsun?" (Konsolda | Bu bilgisayarda) and the chain
 *      Konsol görüntüsü → Makine Live → Altyazı
 *   4. "Şu an": the subtitle being shown, drawn by the overlay's SubtitleBlock
 *   5. setup in dependency order: Hesap → Konsol görüntüsü → Altyazı bölgesi →
 *      Altyazı nerede; a finished step folds to one ticked row
 *   6. Görünüm (loaded on demand)
 *   7. quietly at the bottom: announcements, auto start, usage report, Discord,
 *      donation
 * Sections the plugin's next phase brings (TV pairing, phone/OBS link, log
 * upload) slot into 5 and 7.
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
    readonly property bool _sourceOk: _consoleMode ? _hasSource : _hasRegion
    readonly property bool _ready: _loginState === "signedIn" && _sourceOk && _server.maintenance !== true
    readonly property var _announcements: Array.isArray(_state.announcements) ? _state.announcements : []

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
    Component.onCompleted: { _refreshInfo(); _refreshState(); if (_wanted) _call("state") }
    on_WantedChanged: if (_wanted) _call("state")

    // Every subtitle line publishes a new state: read it only while the page can be seen.
    readonly property bool _onScreen: visible && Window.visibility !== Window.Minimized
                                      && Window.visibility !== Window.Hidden
    on_OnScreenChanged: if (_onScreen) _refreshState()
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
            if (!ok) live._error = String(result || "")
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
            return { text: _server.message || qsTr("Çeviri havuzu bakımda; bakım bitince kendiliğinden devam eder."), fixText: "" }
        const reasons = {
            source_missing: { text: qsTr("Konsol görüntüsü bulunamadı: Remote Play ya da yakalama kartının penceresi açık değil."),
                              fixText: qsTr("Yeniden ara"), fix: "sources" },
            black_frame:    { text: qsTr("Görüntü siyah geliyor: pencere küçültülmüş ya da başka bir pencerenin altında olabilir."),
                              fixText: qsTr("Yeniden ara"), fix: "sources" },
            stream_stalled: { text: qsTr("Görüntü dondu: Remote Play bağlantısı kopmuş olabilir."), fixText: qsTr("Yeniden ara"), fix: "sources" },
            stream_poor:    { text: qsTr("Görüntü kalitesi düşük; altyazı okunamayabilir. Remote Play'de kaliteyi yükselt."), fixText: "" },
            hdr_washed:     { text: qsTr("Renkler soluk geliyor: konsolda HDR açık. Okuma için HDR'yi kapat."), fixText: "" },
            tv_offline:     { text: qsTr("Makine TV'ye ulaşılamıyor: TV açık ve aynı ağda mı?"), fixText: "" },
            quota:          { text: qsTr("Bugünkü çeviri hakkın doldu; yarın yeniden açılır."), fixText: "" }
        }
        if (_running && _state.reason && reasons[_state.reason]) return reasons[_state.reason]
        if (_error !== "") return { text: _error, fixText: "" }
        return null
    }
    function _fix(kind) {
        if (kind === "restart") PluginManager.restart(pluginId)
        else if (kind === "enable") PluginManager.setEnabled(pluginId, true)
        else if (kind === "sources") _call("console.sources")
    }

    // ── Small parts ──────────────────────────────────────────────────────────
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

    // One stop of the hero's chain: tile, name, one status line, one action
    component Stop: ColumnLayout {
        id: stop
        property string glyph: ""
        property string title: ""
        property string status: ""
        property bool ok: false
        property string actionText: ""
        property string actionStyle: "gray"
        property bool actionBusy: false
        signal action()
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 8
        IconTile { glyph: stop.glyph; size: 36; tint: stop.ok ? Theme.success : Theme.primary }
        Text {
            Layout.fillWidth: true
            text: stop.title
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: Dimensions.headline
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            text: stop.status
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Dimensions.footnote
            color: stop.ok ? Theme.textSecondary : Theme.textMuted
        }
        AppButton {
            visible: stop.actionText !== ""
            style: stop.actionStyle
            size: "small"
            text: stop.actionText
            busy: stop.actionBusy
            onClicked: stop.action()
        }
    }

    // The link between two stops: turns accent once the two are connected
    component Link: Item {
        id: link
        property bool connected: false
        Layout.preferredWidth: 40
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 17
        implicitHeight: 2
        FadeLine {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            tint: link.connected ? Theme.primary : Theme.separator
            Behavior on tint { ColorAnimation { duration: Dimensions.animMedium } }
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

        ColumnLayout {
            id: page
            width: flick.width
            spacing: 22

            // ── 1. Header: the page's only filled button ──
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
                AppButton {
                    visible: live._exists
                    style: "filled"
                    size: "large"
                    text: live._reading ? qsTr("Durdur") : qsTr("Başlat")
                    icon: live._reading ? Icons.squareStop : Icons.play
                    enabled: live._running && (live._reading || live._ready)
                    busy: live._busyMethod === "start" || live._busyMethod === "stop"
                    toolTip: live._reading ? qsTr("Okumayı durdurur; oyunun üstündeki altyazı kalkar.")
                           : live._ready ? qsTr("Altyazıyı okumaya başlar; Türkçesi oyunun üstünde çıkar.")
                           : qsTr("Önce aşağıdaki adımları tamamla.")
                    onClicked: live._reading ? live._call("stop") : live._start()
                }
            }

            // ── 2. Problem strip: the most blocking one, one fix ──
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

            // ── 3. Hero: where you play, and the chain to the subtitle ──
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                glow: true
                radius: Dimensions.radiusHero
                implicitHeight: heroCol.implicitHeight + 52

                ColumnLayout {
                    id: heroCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    spacing: 18

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text {
                                Layout.fillWidth: true
                                text: live._consoleMode ? qsTr("GTA VI'yı konsolda Türkçe oyna")
                                    : (live._state.game && live._state.game.name)
                                      ? qsTr("%1, Türkçe altyazıyla").arg(live._state.game.name)
                                      : qsTr("Bilgisayardaki oyunu Türkçe oyna")
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                                font.family: Dimensions.fontDisplay
                                font.pixelSize: Dimensions.title1
                                font.weight: Font.Bold
                                color: Theme.textPrimary
                            }
                            Body {
                                text: live._consoleMode
                                      ? qsTr("PS5 ya da Xbox görüntüsünü bilgisayarda açtığın pencereden okur; okunan her satır GTA VI havuzuna yazılır, bu yüzden yalnız GTA VI oynarken başlat.")
                                      : qsTr("Seçtiğin bölgedeki İngilizce altyazıyı oyun öndeyken okur, Türkçesini oyunun üstüne yazar.")
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            text: qsTr("Nerede oynuyorsun?")
                            textFormat: Text.PlainText
                            font.pixelSize: Dimensions.callout
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }
                        SegmentedControl {
                            model: [{ value: "console", text: qsTr("Konsolda (PS5 / Xbox)") },
                                    { value: "pc", text: qsTr("Bu bilgisayarda") }]
                            currentValue: live._consoleMode ? "console" : "pc"
                            onActivated: (value) => { live._askedSources = false; live._call("mode.set", { mode: value }) }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Konsol görüntüsü (or the region) → Makine Live → Altyazı
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Stop {
                            glyph: live._consoleMode ? Icons.gamepad2 : Icons.scan
                            title: live._consoleMode ? qsTr("Konsol görüntüsü") : qsTr("Altyazı bölgesi")
                            ok: live._sourceOk
                            status: live._consoleMode
                                    ? (live._selectedInfo ? (live._selectedInfo.label || live._selectedInfo.title)
                                       : live._hasSource ? qsTr("Seçili") : qsTr("Henüz bulunamadı"))
                                    : (live._hasRegion ? qsTr("%1 × %2 seçili").arg(live._region.width).arg(live._region.height)
                                       : qsTr("Seçilmedi"))
                            actionText: live._consoleMode ? (live._hasSource ? qsTr("Değiştir") : qsTr("Bul"))
                                                          : (live._hasRegion ? qsTr("Değiştir") : qsTr("Bölge seç"))
                            actionStyle: live._sourceOk ? "gray" : "tinted"
                            actionBusy: live._busyMethod === (live._consoleMode ? "console.sources" : "region.pick")
                            onAction: live._consoleMode ? live._scrollTo(sourceSection) : live._call("region.pick")
                        }
                        Link { connected: live._sourceOk }
                        Stop {
                            glyph: Icons.languages
                            title: "Makine Live"
                            ok: live._reading
                            status: live._reading ? qsTr("Okuyor · %1 satır").arg(live._counters.lines || 0)
                                  : !live._signedIn ? qsTr("Giriş gerekli")
                                  : live._ready ? qsTr("Hazır; Başlat'a bas.")
                                  : qsTr("Önceki adımı bekliyor")
                            actionText: !live._signedIn ? qsTr("Giriş yap") : live._reading ? "" : qsTr("Bir kez oku")
                            actionStyle: !live._signedIn ? "tinted" : "gray"
                            actionBusy: live._busyMethod === "readOnce" || live._busyMethod === "login.start"
                            onAction: !live._signedIn ? live._scrollTo(accountSection) : live._call("readOnce")
                        }
                        Link { connected: live._reading }
                        Stop {
                            glyph: Icons.laptop
                            title: qsTr("Altyazı")
                            ok: live._reading
                            status: live._consoleMode ? qsTr("Bu ekranda, Remote Play penceresinin üstünde")
                                                      : qsTr("Bu ekranda, oyunun üstünde")
                            actionText: qsTr("Görünüm")
                            onAction: live._scrollTo(lookLoader)
                        }
                    }
                }
            }

            // ── 4. Şu an: the subtitle as the game shows it ──
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
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
                            : qsTr("%1 satır · %2 tanesi havuzdan · %3")
                                  .arg(live._counters.lines || 0).arg(live._counters.poolHits || 0)
                                  .arg(live._lastLine.kind === "verified" ? qsTr("onaylı çeviri") : qsTr("makine çevirisi, onay bekliyor"))
                        textFormat: Text.PlainText
                        font.family: Dimensions.fontText
                        font.pixelSize: Dimensions.caption1
                        color: Theme.textMuted
                    }
                }
            }

            // ── 5a. Hesap ──
            Item {
                id: accountSection
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                implicitHeight: live._signedIn ? accountDone.implicitHeight : signCard.implicitHeight

                ListSection {
                    id: accountDone
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: live._signedIn
                    title: qsTr("Hesap")
                    ListRow {
                        icon: Icons.circleCheck
                        iconTint: Theme.success
                        title: live._login.user || qsTr("Giriş yapıldı")
                        subtitle: live._loginState === "offline"
                                  ? qsTr("Sunucuya şu an ulaşılamıyor; hesabın duruyor.")
                                  : qsTr("Çeviriler bu hesapla havuzdan gelir.")
                        AppButton {
                            style: "gray"
                            text: qsTr("Çıkış yap")
                            onClicked: live._call("logout")
                        }
                    }
                }

                // Device-code sign-in: a big code to type in the browser
                Card {
                    id: signCard
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: !live._signedIn
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
            }

            // ── 5b. Konsol görüntüsü: step by step until one is chosen ──
            ListSection {
                id: sourceSection
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && live._consoleMode
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
                    title: qsTr("PS5")
                    subtitle: qsTr("PS Remote Play, chiaki ya da Asobi'yi aç, konsoluna bağlan, GTA VI'yı orada başlat.")
                }
                ListRow {
                    visible: !live._hasSource || sourceSection.open
                    icon: Icons.gamepad2
                    title: qsTr("Xbox")
                    subtitle: qsTr("Xbox uygulamasında ya da Greenlight'ta konsoluna bağlan, oyunu orada başlat.")
                }
                ListRow {
                    visible: !live._hasSource || sourceSection.open
                    icon: Icons.monitor
                    title: qsTr("Yakalama kartı")
                    subtitle: qsTr("Kartın programını (Elgato, OBS önizlemesi, Windows Kamera) aç; görüntü bu bilgisayarda görünsün.")
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

            // ── 5c. Altyazı bölgesi ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Altyazı bölgesi")
                footer: live._hasRegion ? ""
                        : live._consoleMode ? qsTr("Konsolda bölge görüntünün içinde seçilir; görüntü penceresi açık olmalı.")
                        : qsTr("Bölgeyi altyazının çıktığı satırlara sıkı çiz: ne kadar dar, o kadar hızlı ve doğru.")

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
            }

            // ── 5d. Altyazı nerede ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Altyazı nerede çıksın?")
                footer: qsTr("TV ve telefon, Makine Live'ın bir sonraki güncellemesiyle bu bölüme gelir.")

                ListRow {
                    icon: Icons.circleCheck
                    iconTint: Theme.success
                    title: qsTr("Bu ekranda")
                    subtitle: live._consoleMode ? qsTr("Remote Play penceresinin üstünde, İngilizce altyazının yerinde.")
                                                : qsTr("Oyunun üstünde, İngilizce altyazının yerinde.")
                }
                ListRow {
                    icon: Icons.monitor
                    iconTint: Theme.textMuted
                    title: qsTr("TV'de (Makine TV)")
                    subtitle: qsTr("Konsolu TV'de oynarken altyazı TV'nin üstünde çıkar. Yakında.")
                }
                ListRow {
                    icon: Icons.smartphone
                    iconTint: Theme.textMuted
                    title: qsTr("Telefonda ya da OBS'te")
                    subtitle: qsTr("Telefonun tarayıcısında ya da OBS'te bir sayfa olarak. Yakında.")
                }
                ListRow {
                    icon: Icons.eye
                    title: qsTr("Yayında göster")
                    subtitle: live._style.showOnStream === true
                              ? qsTr("Altyazı yayında ve ekran görüntülerinde de görünür.")
                              : qsTr("Kapalıyken altyazı yayına (OBS, Discord) ve ekran görüntülerine girmez; Live da onu okumaz.")
                    AppSwitch {
                        checked: live._style.showOnStream === true
                        accessibleName: qsTr("Yayında göster")
                        onToggled: live._setStyle({ showOnStream: !(live._style.showOnStream === true) })
                    }
                }
            }

            // ── 6. Görünüm (loaded when the plugin runs) ──
            Loader {
                id: lookLoader
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                active: live._running
                visible: live._running
                asynchronous: true
                sourceComponent: appearanceComponent
            }

            // ── 7. Quietly at the bottom ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._exists
                title: qsTr("Diğer")

                Repeater {
                    model: live._announcements
                    ListRow {
                        required property var modelData
                        icon: Icons.bell
                        title: modelData.title || modelData.text || ""
                        subtitle: modelData.title ? (modelData.text || "") : ""
                        clickable: !!modelData.url
                        onClicked: PluginManager.openUrl(modelData.url)
                    }
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
                    subtitle: qsTr("Discord açıksa oynadığın oyunun yanında Makine Live ile Türkçe oynadığın görünür.")
                }
                ListRow {
                    icon: Icons.handHeart
                    title: qsTr("Makine Çeviri'ye destek ol")
                    subtitle: qsTr("Live'ın sunucusu ve çeviri havuzu bağışlarla ayakta.")
                    clickable: true
                    onClicked: PluginManager.openUrl("https://makineceviri.org/destek")
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
        onConfirmed: { seen.consoleWarned = true; live._call("start") }
    }

    // ── 6. Görünüm: preview + the few choices that matter ────────────────────
    Component {
        id: appearanceComponent

        ColumnLayout {
            id: look
            spacing: 14

            // Resolved values, the same fallbacks SubtitleBlock uses
            readonly property string preset: live._style.preset || "band"
            readonly property int sizePx: live._style.sizePx || 28
            readonly property int maxLines: live._style.maxLines || 2
            readonly property real bandOpacity: preview.bandOpacity
            readonly property string colorKey: String(preview.textColor).toLowerCase() === "#f2c230" ? "yellow" : "white"
            readonly property string position: live._style.position || "region"
            readonly property bool showSource: live._style.showSource === true

            function nearest(value, steps) {
                let best = steps[0]
                for (let i = 1; i < steps.length; ++i)
                    if (Math.abs(steps[i] - value) < Math.abs(best - value)) best = steps[i]
                return best
            }

            Text {
                text: qsTr("Görünüm")
                textFormat: Text.PlainText
                font.pixelSize: Dimensions.footnote
                font.weight: Font.DemiBold
                color: Theme.textMuted
                Layout.leftMargin: 16
            }

            // Preview at the real size: a light and a dark half
            Card {
                Layout.fillWidth: true
                implicitHeight: 190
                radius: Dimensions.radiusSection
                reveal: false

                Rectangle {
                    id: lookScene
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: Dimensions.radiusLG
                    clip: true
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.textPrimary }
                        GradientStop { position: 0.5; color: Theme.textMuted }
                        GradientStop { position: 1.0; color: Theme.bgPrimary }
                    }

                    SubtitleBlock {
                        id: preview
                        x: Math.round((lookScene.width - width) / 2)
                        y: look.position === "top" ? 4
                         : look.position === "bottom" ? lookScene.height - height - 4
                         : Math.round((lookScene.height - height) / 2)
                        maxWidth: lookScene.width * 0.8
                        stretchWidth: lookScene.width * 0.95
                        animate: Dimensions.animFast > 0
                        style: live._style
                        lines: [
                            { text: qsTr("Arabayı arkaya çek, birazdan dönerim."), sourceText: "Pull the car around back, I'll be right out.", kind: "verified" },
                            { text: qsTr("Kimseye bir şey söyleme."), sourceText: "Don't tell anyone.", kind: "machine" }
                        ]
                    }
                }
            }

            ListSection {
                Layout.fillWidth: true
                footer: qsTr("Onay bekleyen makine çevirisinin bandında solda ince sarı bir çizgi olur. Değişiklik bir sonraki satırda oyuna yansır.")

                ListRow {
                    icon: Icons.palette
                    title: qsTr("Biçim")
                    subtitle: look.preset === "contrast" ? qsTr("Koyu bant, sarı yazı: parlak, kalabalık sahneler için.")
                            : look.preset === "classic" ? qsTr("Bant yok; kalın kontur ve gölge, film altyazısı gibi.")
                            : look.preset === "minimal" ? qsTr("Bant yok, yalnız kontur; en az yer kaplar.")
                            : qsTr("Yazının arkasında koyu bant; her sahnede okunur.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "band", text: qsTr("Bant") }, { value: "contrast", text: qsTr("Kontrast") },
                                { value: "classic", text: qsTr("Film") }, { value: "minimal", text: qsTr("Sade") }]
                        currentValue: look.preset
                        // A preset brings its own band, colour and outline back
                        onActivated: (value) => live._setStyle({ preset: value, bandOpacity: null, textColor: null,
                                                                  outlinePx: null, shadow: null })
                    }
                }
                ListRow {
                    icon: Icons.type
                    title: qsTr("Yazı boyu")
                    subtitle: qsTr("1080p ekrandaki boy; ekran büyüdükçe altyazı da büyür.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "22", text: qsTr("Küçük") }, { value: "28", text: qsTr("Orta") },
                                { value: "34", text: qsTr("Büyük") }, { value: "42", text: qsTr("Çok büyük") }]
                        currentValue: String(look.nearest(look.sizePx, [22, 28, 34, 42]))
                        onActivated: (value) => live._setStyle({ sizePx: Number(value) })
                    }
                }
                ListRow {
                    icon: Icons.listOrdered
                    title: qsTr("Satır")
                    subtitle: qsTr("Bir altyazı en çok kaç satır olsun; uzun satırda bant genişler, yazı küçülmez.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "1", text: "1" }, { value: "2", text: "2" }, { value: "3", text: "3" }]
                        currentValue: String(Math.min(3, look.maxLines))
                        onActivated: (value) => live._setStyle({ maxLines: Number(value) })
                    }
                }
                ListRow {
                    icon: Icons.swatchBook
                    title: qsTr("Renk")
                    subtitle: qsTr("Sarı, oyunun kendi beyaz altyazısından ayrılır.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "white", text: qsTr("Beyaz") }, { value: "yellow", text: qsTr("Sarı") }]
                        currentValue: look.colorKey
                        onActivated: (value) => live._setStyle({ textColor: value === "yellow" ? preview.signatureYellow.toString() : "#FFFFFF" })
                    }
                }
                ListRow {
                    icon: Icons.layers
                    title: qsTr("Bant")
                    subtitle: qsTr("Koyu bant altındaki İngilizce altyazıyı örter. Bant azaldıkça kontur kalınlaşır.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "0", text: qsTr("Yok") }, { value: "0.5", text: qsTr("Hafif") },
                                { value: "0.9", text: qsTr("Koyu") }, { value: "1", text: qsTr("Tam") }]
                        currentValue: String(look.nearest(look.bandOpacity, [0, 0.5, 0.9, 1]))
                        onActivated: (value) => live._setStyle({ bandOpacity: Number(value) })
                    }
                }
                ListRow {
                    icon: Icons.pictureInPicture2
                    title: qsTr("Yer")
                    subtitle: look.position === "region" ? qsTr("İngilizce altyazının tam üstünde çıkar.")
                            : look.position === "top" ? qsTr("Ekranın üstünde çıkar.")
                            : qsTr("Ekranın altında çıkar.")
                    SegmentedControl {
                        small: true
                        model: [{ value: "region", text: qsTr("Altyazının üstü") }, { value: "bottom", text: qsTr("Alt") },
                                { value: "top", text: qsTr("Üst") }]
                        currentValue: look.position
                        onActivated: (value) => live._setStyle({ position: value })
                    }
                }
                ListRow {
                    icon: Icons.languages
                    title: qsTr("İngilizcesini de göster")
                    subtitle: qsTr("Türkçenin altında küçük harflerle aslı durur.")
                    AppSwitch {
                        checked: look.showSource
                        accessibleName: qsTr("İngilizcesini de göster")
                        onToggled: live._setStyle({ showSource: !look.showSource })
                    }
                }
            }
        }
    }
}
