// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtCore
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
 * The plugin owns the logic; this page shows its latest `ui.state`
 * (PluginManager.pluginState) and calls its methods (PluginManager.invoke).
 * Top down: a one-time note on what Live reads; the lead card (PC or console,
 * what is still missing, Başlat/Durdur); the line being read now; the account
 * (device-code sign-in with a big code); where the subtitle comes from (console
 * sources, region); how the subtitle looks, with a live preview drawn by the
 * overlay's own SubtitleBlock. When the plugin is off, broken or switched off
 * remotely (launcher.json) the page says so instead.
 *
 * State (Live's Engine::stateLocked): {login:{state: checking|starting|waiting|
 *         signedOut|signedIn|offline, code, url, user, message}, mode: pc|console,
 *         sources:[{id,key,label,console: ps5|xbox|capture|custom,exe,title,pid}],
 *         selectedSource:{key,pid}|null, region:{pc:{x,y,width,height}|null, console:{…}},
 *         picking, running, status, lastLine:{source,text,kind,origin},
 *         counters:{lines,poolHits}, server:{maintenance,message}, notice, style,
 *         game:{name,slug}}
 *
 * Cheap while a game runs: no running animation, the state is only re-read while
 * the page is visible, the appearance section loads on demand.
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
    readonly property var _sources: _state.sources || []
    readonly property var _region: _state.region ? _state.region[_consoleMode ? "console" : "pc"] || null : null
    readonly property bool _hasRegion: !!_region && _region.width > 0 && _region.height > 0
    readonly property var _selected: _state.selectedSource || null
    readonly property bool _hasSource: !!_selected
    readonly property var _lastLine: _state.lastLine || ({})
    readonly property var _counters: _state.counters || ({})
    readonly property var _server: _state.server || ({})
    readonly property var _style: _state.style || ({})
    readonly property bool _reading: _state.running === true
    readonly property bool _ready: _loginState === "signedIn" && (_consoleMode ? _hasSource : _hasRegion) && _server.maintenance !== true

    function _refreshInfo() { _info = PluginManager ? PluginManager.plugin(pluginId) : ({}) }
    function _refreshState() { _state = PluginManager ? PluginManager.pluginState(pluginId) : ({}) }
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

    // Opening the page is the plugin's first use: asking for its state starts its
    // host (PluginManager.invoke). Nothing runs before the page is opened.
    readonly property bool _wanted: visible && _exists && _info.enabled === true && _pluginState !== "remote-off"
    Component.onCompleted: { _refreshInfo(); _refreshState(); if (_wanted) _call("state") }
    on_WantedChanged: if (_wanted) _call("state")
    // Every subtitle line publishes a new state: only read it while someone looks
    onVisibleChanged: if (visible) _refreshState()

    // Console mode with no source list yet: look once, the user should not have to
    property bool _askedSources: false
    readonly property bool _wantSources: _running && _consoleMode && _sources.length === 0 && !_askedSources
    on_WantSourcesChanged: if (_wantSources) { _askedSources = true; _call("console.sources") }

    Connections {
        target: PluginManager
        function onPluginsChanged() { live._refreshInfo() }
        function onPluginChanged(id) { if (id === live.pluginId) live._refreshInfo() }
        function onPluginStateChanged(id) { if (id === live.pluginId && live.visible) live._refreshState() }
        function onInvokeFinished(id, method, ok, result) {
            if (id !== live.pluginId) return
            if (live._busyMethod === method) live._busyMethod = ""
            if (!ok) live._error = String(result || "")
            else if (method === "state" && result && typeof result === "object") live._state = result
        }
    }

    // The note on what Live reads shows once, until the user closes it
    Settings {
        id: seen
        category: "makineLive"
        property bool introDone: false
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

    // One readiness line in the lead card: a tick or an open circle, and what it means
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
                subtitle: qsTr("Oyundaki İngilizce altyazıyı okur, Türkçesini oyunun üstüne altyazı olarak yazar.")
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
                    Need { done: true; text: qsTr("Yalnız seçtiğin altyazı bölgesini ya da konsol görüntüsünün penceresini okur; ekranın geri kalanına bakmaz.") }
                    Need { done: true; text: qsTr("Görüntü bilgisayarından çıkmaz. Okunan metin bilgisayarında çıkarılır.") }
                    Need { done: true; text: qsTr("Yalnız okunan İngilizce satır, Makine hesabınla makineceviri.org'daki ortak çeviri havuzuna gider; Türkçesi oradan gelir.") }
                }
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
                         : live._pluginState === "starting" || live._pluginState === "idle" ? qsTr("Makine Live açılıyor…")
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
                    icon: Icons.triangleAlert
                    iconTint: Theme.warning
                    title: qsTr("Çeviri havuzu bakımda")
                    subtitle: live._server.message || qsTr("Bir süre yeni satır çevrilemez; bakım bitince kendiliğinden devam eder.")
                }
            }

            // ── Lead: where you play, what is missing, start ──
            Card {
                id: lead
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                glow: true
                radius: Dimensions.radiusSection
                implicitHeight: leadCol.implicitHeight + 48

                ColumnLayout {
                    id: leadCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 26
                    anchors.rightMargin: 26
                    spacing: 16

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
                                      ? qsTr("PS5 ya da Xbox görüntüsünü bilgisayarda açtığın pencereden okur: PS Remote Play, Xbox uygulaması ya da yakalama kartı. Türkçe altyazı o pencerenin üstünde çıkar.")
                                      : qsTr("Seçtiğin bölgedeki İngilizce altyazıyı okur, Türkçesini oyunun üstüne yazar.")
                            }
                        }
                        SegmentedControl {
                            Layout.alignment: Qt.AlignTop
                            model: [{ value: "console", text: qsTr("Konsol") }, { value: "pc", text: qsTr("PC") }]
                            currentValue: live._consoleMode ? "console" : "pc"
                            onActivated: (value) => { live._askedSources = false; live._call("mode.set", { mode: value }) }
                        }
                    }

                    FadeLine { Layout.fillWidth: true }

                    // What is still missing, in the order to do it
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: !live._reading
                        Need {
                            done: live._signedIn
                            text: live._signedIn ? qsTr("Makine hesabınla giriş yaptın.")
                                                 : qsTr("Makine hesabınla giriş yap (aşağıda).")
                        }
                        Need {
                            visible: live._consoleMode
                            done: live._hasSource
                            text: live._hasSource ? qsTr("Konsol görüntüsü bulundu.")
                                                  : qsTr("Konsol görüntüsünün penceresini aç ve aşağıdan seç.")
                        }
                        Need {
                            visible: !live._consoleMode
                            done: live._hasRegion
                            text: live._hasRegion ? qsTr("Altyazı bölgesi seçili.")
                                                  : qsTr("Oyunda altyazının çıktığı yeri seç (aşağıda).")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        AppButton {
                            style: live._reading ? "gray" : "filled"
                            size: "large"
                            text: live._reading ? qsTr("Durdur") : qsTr("Başlat")
                            icon: live._reading ? Icons.squareStop : Icons.play
                            enabled: live._reading || live._ready
                            busy: live._busyMethod === "start" || live._busyMethod === "stop"
                            toolTip: live._reading ? qsTr("Okumayı durdurur; oyunun üstündeki altyazı kalkar.")
                                                   : qsTr("Altyazıyı okumaya başlar; Türkçesi oyunun üstünde çıkar.")
                            onClicked: live._call(live._reading ? "stop" : "start")
                        }
                        AppButton {
                            visible: !live._reading
                            style: "gray"
                            size: "large"
                            text: qsTr("Bir kez oku")
                            enabled: live._ready
                            busy: live._busyMethod === "readOnce"
                            toolTip: qsTr("Bölgeyi bir kez okur ve çevirir; ayarı denemek için.")
                            onClicked: live._call("readOnce")
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: 6
                            text: live._error !== "" ? live._error
                                : live._reading ? (live._state.status || qsTr("Çalışıyor; altyazı bekleniyor."))
                                : (live._state.notice || live._state.status || "")
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            font.pixelSize: Dimensions.footnote
                            color: live._error !== "" ? Theme.warning
                                 : live._reading ? Theme.success : Theme.textMuted
                        }
                    }
                }
            }

            // ── The line being read now: English, then the Turkish as it shows in the game ──
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && (live._reading || !!live._lastLine.text)
                radius: Dimensions.radiusSection
                implicitHeight: nowCol.implicitHeight + 40

                ColumnLayout {
                    id: nowCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Son satır")
                            textFormat: Text.PlainText
                            font.pixelSize: Dimensions.headline
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            text: live._counters.lines !== undefined
                                  ? qsTr("%1 satır · %2 tanesi havuzdan").arg(live._counters.lines).arg(live._counters.poolHits || 0)
                                  : ""
                            textFormat: Text.PlainText
                            font.pixelSize: Dimensions.footnote
                            color: Theme.textMuted
                        }
                    }
                    Body {
                        visible: !live._lastLine.text
                        text: qsTr("Oyunda bir altyazı çıkınca burada da görünür.")
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !!live._lastLine.source
                        text: live._lastLine.source || ""
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Dimensions.callout
                        color: Theme.textMuted
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !!live._lastLine.text
                        text: live._lastLine.text || ""
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Dimensions.title3
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        visible: !!live._lastLine.text
                        text: live._lastLine.kind === "verified"
                              ? qsTr("Topluluğun onayladığı çeviri")
                              : qsTr("Makine çevirisi; havuzda onay bekliyor")
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.footnote
                        color: live._lastLine.kind === "verified" ? Theme.success : Theme.textMuted
                    }
                }
            }

            // ── Account: device-code sign-in ──
            Card {
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
                                text: signCol.waiting ? qsTr("Tarayıcıda bu kodu gir") : qsTr("Makine hesabınla giriş yap")
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
                            style: "filled"
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
                            style: "filled"
                            text: qsTr("Tarayıcıda aç")
                            icon: Icons.externalLink
                            visible: !!live._login.url
                            onClicked: PluginManager.openUrl(live._login.url)
                        }
                    }
                }
            }

            // ── Account, signed in: one quiet row ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && live._signedIn
                title: qsTr("Hesap")

                ListRow {
                    icon: Icons.circleUser
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

            // ── Console: which window shows the console ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running && live._consoleMode
                title: qsTr("Konsol görüntüsü")
                footer: qsTr("Live bu pencereyi okur. Pencere küçültülmüş olmasın; başka bir pencerenin altında kalabilir.")

                ListRow {
                    visible: live._sources.length === 0
                    icon: Icons.circleAlert
                    iconTint: Theme.warning
                    title: qsTr("Konsol görüntüsü bulunamadı")
                    subtitle: qsTr("PS Remote Play, Xbox uygulaması ya da yakalama kartının programını aç, oyunu orada başlat, sonra Yenile'ye bas.")
                    AppButton {
                        style: "gray"
                        text: qsTr("Başka pencere")
                        toolTip: qsTr("Açık bütün pencereleri listeler; konsol görüntün başka bir programdaysa onu seç.")
                        busy: live._busyMethod === "console.sources"
                        onClicked: live._call("console.sources", { all: true })
                    }
                }
                Repeater {
                    model: live._consoleMode ? live._sources : []
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
                        onClicked: live._call("console.select", { sourceId: String(modelData.id) })
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
                    icon: Icons.refreshCw
                    title: qsTr("Listeyi yenile")
                    subtitle: qsTr("Programı yeni açtıysan bir kez daha bak.")
                    AppButton {
                        style: "gray"
                        text: qsTr("Yenile")
                        busy: live._busyMethod === "console.sources"
                        onClicked: live._call("console.sources")
                    }
                }
            }

            // ── Where the subtitle is ──
            ListSection {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: live._running
                title: qsTr("Altyazı bölgesi")
                footer: live._consoleMode
                        ? qsTr("Konsolda bölge görüntünün içinde seçilir; görüntü penceresi açık olmalı.")
                        : qsTr("Bölgeyi altyazının çıktığı satırlara sıkı çiz: ne kadar dar, o kadar hızlı ve doğru.")

                ListRow {
                    icon: Icons.scan
                    title: live._state.picking ? qsTr("Bölge seçiliyor…")
                         : live._hasRegion ? qsTr("%1 × %2 piksel").arg(live._region.width).arg(live._region.height)
                         : qsTr("Bölge seçilmedi")
                    subtitle: live._state.picking ? qsTr("Ekranda altyazının çıktığı yeri sürükleyerek çiz; Esc vazgeçer.")
                            : live._hasRegion ? qsTr("Live yalnız bu dikdörtgenin içini okur.")
                            : qsTr("Oyunda altyazının çıktığı yeri seç.")
                    AppButton {
                        style: live._hasRegion ? "gray" : "tinted"
                        text: live._hasRegion ? qsTr("Değiştir") : qsTr("Bölge seç")
                        icon: Icons.scan
                        enabled: !live._state.picking && (!live._consoleMode || live._hasSource)
                        busy: live._busyMethod === "region.pick"
                        onClicked: live._call("region.pick")
                    }
                }
            }

            // ── How the subtitle looks (loaded when the page has the plugin running) ──
            Loader {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                active: live._running
                visible: live._running
                asynchronous: true
                sourceComponent: appearanceComponent
            }
        }
    }

    TopDissolve {
        flick: flick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    // ── Appearance: preview + the few choices that matter ────────────────────
    Component {
        id: appearanceComponent

        ColumnLayout {
            id: look
            spacing: 14

            // Resolved values, the same fallbacks SubtitleBlock uses
            readonly property string preset: live._style.preset || "band"
            readonly property int sizePx: live._style.sizePx || 28
            readonly property int maxLines: live._style.maxLines || 2
            readonly property real bandOpacity: live._style.bandOpacity !== undefined && live._style.bandOpacity !== null
                                                ? live._style.bandOpacity : preview.bandOpacity
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
                text: qsTr("Altyazının görünümü")
                textFormat: Text.PlainText
                font.pixelSize: Dimensions.footnote
                font.weight: Font.DemiBold
                color: Theme.textMuted
                Layout.leftMargin: 16
            }

            // Live preview: a light and a dark half, so readability shows on both
            Card {
                Layout.fillWidth: true
                implicitHeight: 200
                radius: Dimensions.radiusSection
                reveal: false

                Rectangle {
                    id: scene
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
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: look.position === "top" ? 4
                         : look.position === "bottom" ? parent.height - height - 4
                         : Math.round((parent.height - height) / 2)
                        maxWidth: scene.width * 0.9
                        style: live._style
                        lines: [
                            { text: qsTr("Arabayı arkaya çek, birazdan dönerim."), sourceText: "Pull the car around back, I'll be right out." },
                            { text: qsTr("Kimseye bir şey söyleme."), sourceText: "Don't tell anyone." }
                        ]
                    }
                }
            }

            ListSection {
                Layout.fillWidth: true
                footer: qsTr("Değişiklik bir sonraki satırda oyuna yansır.")

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
                    subtitle: qsTr("Oyunun kendi altyazısıyla aynı boy en rahat okunanıdır.")
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
                    subtitle: qsTr("Aynı anda en çok kaç altyazı satırı görünsün.")
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
                                { value: "0.78", text: qsTr("Koyu") }, { value: "1", text: qsTr("Tam") }]
                        currentValue: String(look.nearest(look.bandOpacity, [0, 0.5, 0.78, 1]))
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
