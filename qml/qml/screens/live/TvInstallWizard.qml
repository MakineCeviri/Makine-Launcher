// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
import "../../dialogs"
pragma ComponentBehavior: Bound

/**
 * TvInstallWizard — "Makine TV'yi TV'ne kur", one thing on screen at a time:
 *   1. which TV (Android TV / Google TV, Fire TV, LG webOS)
 *   2. what to do on the TV (the plugin's own guide, numbered)
 *   3. find the TV (Tara) or type its address, then one Kur; progress and result
 * Calls the plugin through the page: tv.install.scan, tv.install (android / lg),
 * tv.install.saveFile. State: the page's _install (= state.tvInstall:
 * platform, state running|done|error, step, message, hosts, hostsPlatform,
 * scanning, guide).
 */
BaseDialog {
    id: wiz

    // The page (MakineLiveScreen): _install, _call, _tvInstallAt, _tvInstallLg
    property var screen: null

    // 0 = which TV, 1 = on the TV, 2 = install
    property int step: 0
    // "android" (Android TV / Google TV) | "firetv" | "lg"
    property string platform: ""
    // Kur was pressed in this visit: only then a done / error from the plugin is this visit's
    property bool started: false

    // The user wants to go and pair now
    signal pairRequested()

    readonly property var inst: screen && screen._install ? screen._install : ({})
    readonly property bool lg: platform === "lg"
    readonly property bool running: inst.state === "running"
    readonly property bool finished: started && inst.state === "done"
    readonly property bool failed: started && inst.state === "error"
    readonly property var guide: inst.guide && inst.guide[platform] ? inst.guide[platform] : []
    // The last scan's TVs, only for the platform shown (LG scans Developer Mode's 9922, the others ADB's 5555)
    readonly property var hosts: Array.isArray(inst.hosts) && (inst.hostsPlatform || "android") === (lg ? "lg" : "android")
                                 ? inst.hosts : []

    function _hostText(h) {
        if (typeof h === "string") return h
        return h.host + (!lg && h.port && h.port !== 5555 ? ":" + h.port : "")
    }
    function _scan() { screen._call("tv.install.scan", { platform: lg ? "lg" : "android" }) }
    function _install() {
        started = true
        if (lg) screen._tvInstallLg(address.text, pass.text)
        else screen._tvInstallAt(address.text)
    }
    function _go(n) {
        step = n
        // Entering the last step: look for TVs once, the user should not have to
        if (n === 2 && hosts.length === 0 && !running && inst.scanning !== true) _scan()
    }
    // A single TV found: its address is filled in
    onHostsChanged: if (hosts.length === 1 && address.text === "") address.text = _hostText(hosts[0])

    // A visit starts at the first page, or at the progress when an install is still running
    // (not a handler of its own: BaseDialog already uses onAboutToShow)
    Connections {
        target: wiz
        function onAboutToShow() {
            wiz.started = wiz.running
            if (wiz.running) { wiz.platform = wiz.inst.platform === "lg" ? "lg" : "android"; wiz.step = 2 }
            else wiz.step = 0
            address.text = ""
            pass.text = ""
        }
    }

    width: 560
    contentHeight: col.implicitHeight

    header: Item { implicitHeight: 0 }

    contentItem: ColumnLayout {
        id: col
        spacing: 0

        // Title, where we are, close
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 22
            Layout.leftMargin: 26
            Layout.rightMargin: 18
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Makine TV'yi TV'ne kur")
                    textFormat: Text.PlainText
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: Dimensions.title3
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Text {
                    text: qsTr("Adım %1 / 3").arg(wiz.step + 1)
                    textFormat: Text.PlainText
                    font.pixelSize: Dimensions.footnote
                    color: Theme.textMuted
                }
            }
            AppButton {
                style: "plain"
                icon: Icons.x
                toolTip: qsTr("Kapat")
                onClicked: wiz.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 26
            Layout.rightMargin: 26
            spacing: 6
            Repeater {
                model: 3
                Rectangle {
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: index <= wiz.step ? Theme.primary : Theme.fillTertiary
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: 26
            Layout.rightMargin: 26
            currentIndex: wiz.step

            // ── 1. Which TV ──
            ColumnLayout {
                spacing: 10
                Text {
                    Layout.fillWidth: true
                    text: qsTr("TV'n hangisi?")
                    textFormat: Text.PlainText
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Repeater {
                    model: [
                        { value: "android", glyph: Icons.monitor, title: qsTr("Android TV / Google TV"),
                          sub: qsTr("Sony, Philips, TCL, Xiaomi, Mi Box, Chromecast with Google TV…") },
                        { value: "firetv", glyph: Icons.monitor, title: qsTr("Fire TV"),
                          sub: qsTr("Amazon Fire TV Stick ve Fire TV'li televizyonlar.") },
                        { value: "lg", glyph: Icons.monitor, title: qsTr("LG webOS"),
                          sub: qsTr("LG televizyonlar; geliştirici hesabı ve Developer Mode gerekir.") }
                    ]
                    Rectangle {
                        id: pick
                        required property var modelData
                        readonly property bool chosen: wiz.platform === modelData.value
                        Layout.fillWidth: true
                        implicitHeight: 64
                        radius: Dimensions.radiusLG
                        color: tap.pressed ? Theme.fillTertiary : hover.hovered ? Theme.fillQuaternary : Theme.primary06
                        border.width: 1
                        border.color: chosen ? Theme.primary : Theme.containerBorder
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title
                        function choose() { wiz.platform = modelData.value; address.text = ""; wiz._go(1) }
                        Keys.onReturnPressed: choose()
                        Keys.onSpacePressed: choose()
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 14
                            IconTile { glyph: pick.modelData.glyph; size: 32 }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: pick.modelData.title
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.body
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: pick.modelData.sub
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.footnote
                                    color: Theme.textMuted
                                }
                            }
                            Icon { glyph: pick.chosen ? Icons.circleCheck : Icons.chevronRight; size: 16
                                   color: pick.chosen ? Theme.primary : Theme.textMuted }
                        }
                        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            id: tap
                            onTapped: pick.choose()
                        }
                    }
                }
            }

            // ── 2. On the TV ──
            ColumnLayout {
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    text: qsTr("TV'de şunları yap")
                    textFormat: Text.PlainText
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Repeater {
                    model: wiz.guide
                    RowLayout {
                        id: guideRow
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            radius: 13
                            color: Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.18 : 0.12)
                            Text {
                                anchors.centerIn: parent
                                text: String(guideRow.index + 1)
                                textFormat: Text.PlainText
                                font.pixelSize: Dimensions.footnote
                                font.weight: Font.Bold
                                color: Theme.primaryText
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: typeof guideRow.modelData === "string" ? guideRow.modelData : (guideRow.modelData.text || "")
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.callout
                            lineHeight: 1.2
                            color: Theme.textPrimary
                        }
                    }
                }
                Text {
                    visible: wiz.guide.length === 0
                    Layout.fillWidth: true
                    text: qsTr("Rehber şu an alınamadı; bir sonraki adımda TV'nin adresini yazıp deneyebilirsin.")
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.callout
                    color: Theme.textSecondary
                }
            }

            // ── 3. Find and install ──
            ColumnLayout {
                spacing: 14

                // Progress / result, big
                Rectangle {
                    visible: wiz.running || wiz.finished || wiz.failed
                    Layout.fillWidth: true
                    implicitHeight: progressCol.implicitHeight + 36
                    radius: Dimensions.radiusLG
                    color: wiz.failed ? Theme.errorBg : wiz.finished ? Theme.successBg
                         : Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.12 : 0.08)
                    ColumnLayout {
                        id: progressCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 8
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12
                            Spinner { visible: wiz.running; size: 22 }
                            Icon {
                                visible: !wiz.running
                                glyph: wiz.finished ? Icons.circleCheck : Icons.circleAlert
                                size: 24
                                color: wiz.finished ? Theme.success : Theme.error
                            }
                            Text {
                                text: wiz.running ? qsTr("Kuruluyor…") : wiz.finished ? qsTr("Kuruldu") : qsTr("Kurulamadı")
                                textFormat: Text.PlainText
                                font.family: Dimensions.fontDisplay
                                font.pixelSize: Dimensions.title2
                                font.weight: Font.Bold
                                color: Theme.textPrimary
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: wiz.running ? (wiz.inst.step || qsTr("Hazırlanıyor…"))
                                : (wiz.inst.message || (wiz.finished ? qsTr("Makine TV kuruldu ve açıldı.") : ""))
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                        }
                        Text {
                            visible: wiz.running
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: wiz.lg ? qsTr("Birkaç dakika sürebilir; TV'yi kapatma.")
                                         : qsTr("TV'de bir soru çıkarsa İzin ver de.")
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.footnote
                            color: Theme.textMuted
                        }
                        AppButton {
                            visible: wiz.finished
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: 6
                            style: "filled"
                            size: "large"
                            text: qsTr("Şimdi eşleş")
                            icon: Icons.link
                            onClicked: { wiz.close(); wiz.pairRequested() }
                        }
                    }
                }

                // Find the TV, type its address
                ColumnLayout {
                    visible: !wiz.running && !wiz.finished
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            Layout.fillWidth: true
                            text: wiz.hosts.length > 0 ? qsTr("Bulunan TV'ler; birini seç.")
                                : wiz.inst.scanning === true ? qsTr("Ağda TV aranıyor…")
                                : qsTr("Ağda TV bulunamadı. Tara'ya yeniden bas ya da adresi aşağıya yaz.")
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                        }
                        AppButton {
                            style: "gray"
                            text: qsTr("Tara")
                            icon: Icons.scanSearch
                            busy: wiz.inst.scanning === true
                            onClicked: wiz._scan()
                        }
                    }

                    Repeater {
                        model: wiz.hosts
                        Rectangle {
                            id: hostRow
                            required property var modelData
                            readonly property string text: wiz._hostText(modelData)
                            readonly property bool chosen: address.text.trim() === text
                            Layout.fillWidth: true
                            implicitHeight: 44
                            radius: Dimensions.radiusMD
                            color: hostTap.pressed ? Theme.fillTertiary : hostHover.hovered ? Theme.fillQuaternary : Theme.primary06
                            border.width: 1
                            border.color: chosen ? Theme.primary : Theme.containerBorder
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 10
                                Icon { glyph: Icons.monitor; size: 16; color: Theme.primary }
                                Text {
                                    Layout.fillWidth: true
                                    text: hostRow.text
                                    textFormat: Text.PlainText
                                    font.pixelSize: Dimensions.body
                                    color: Theme.textPrimary
                                }
                                Icon { visible: hostRow.chosen; glyph: Icons.circleCheck; size: 16; color: Theme.primary }
                            }
                            HoverHandler { id: hostHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { id: hostTap; onTapped: address.text = hostRow.text }
                        }
                    }

                    Text {
                        text: qsTr("TV'nin adresi")
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.footnote
                        font.weight: Font.DemiBold
                        color: Theme.textSecondary
                    }
                    CodeField {
                        id: address
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignLeft
                        placeholderText: "192.168.1.40"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: wiz.lg ? qsTr("Tarama bulamazsa: Developer Mode ekranındaki TV adresini (IP) yaz.")
                                     : qsTr("Tarama bulamazsa: TV'nin IP adresini yaz (Ayarlar › Ağ'da görünür; port 5555 değilse adres:port).")
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Dimensions.footnote
                        color: Theme.textMuted
                    }

                    // LG: the 6 characters Developer Mode shows while Key Server is on
                    Text {
                        visible: wiz.lg
                        text: qsTr("Passphrase")
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.footnote
                        font.weight: Font.DemiBold
                        color: Theme.textSecondary
                    }
                    CodeField {
                        id: pass
                        visible: wiz.lg
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignLeft
                        placeholderText: "A1B2C3"
                        maximumLength: 12
                        font.capitalization: Font.AllUppercase
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferUppercase
                    }
                    Text {
                        visible: wiz.lg
                        Layout.fillWidth: true
                        text: qsTr("Developer Mode ekranındaki 6 karakter; Key Server açıkken görünür.")
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Dimensions.footnote
                        color: Theme.textMuted
                    }

                    AppButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 4
                        style: "filled"
                        size: "large"
                        text: qsTr("Kur")
                        icon: Icons.download
                        enabled: address.text.trim().length > 6 && (!wiz.lg || pass.text.trim().length >= 6)
                        onClicked: wiz._install()
                    }
                }
            }
        }

        // Geri / small last-resort link
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 18
            Layout.bottomMargin: 20
            Layout.leftMargin: 18
            Layout.rightMargin: 26
            spacing: 8
            AppButton {
                visible: wiz.step > 0
                enabled: !wiz.running
                style: "plain"
                text: qsTr("Geri")
                icon: Icons.arrowLeft
                onClicked: wiz.step = wiz.step - 1
            }
            Item { Layout.fillWidth: true }
            AppButton {
                visible: wiz.step === 2 && !wiz.running && !wiz.finished
                style: "plain"
                size: "small"
                text: qsTr("Dosyayı kendim kuracağım")
                toolTip: wiz.lg ? qsTr("Kurulum dosyasını (.ipk) İndirilenler klasörüne kaydeder; webOS Dev Manager ile kurulur.")
                                : qsTr("Kurulum dosyasını (.apk) İndirilenler klasörüne kaydeder.")
                onClicked: wiz.screen._call("tv.install.saveFile", { platform: wiz.lg ? "lg" : "android" })
            }
            AppButton {
                visible: wiz.step === 1
                style: "filled"
                text: qsTr("İleri")
                trailingIcon: Icons.arrowRight
                onClicked: wiz._go(2)
            }
        }
    }
}
