// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../home"
pragma ComponentBehavior: Bound

/**
 * GamesStep — "which of my games can I play in Turkish?", answered on the spot.
 *
 * The launcher scans the libraries by itself half a second after it starts,
 * so by the time the user reaches this step the answer is usually in. The old
 * step asked them to press "Taramayı Başlat" anyway, played three timed
 * captions ("Steam… Epic… GOG…") that had nothing to do with the scan, and
 * reported every game found — 16 — when two had a patch.
 */
Item {
    id: step

    signal next()
    signal back()

    function primaryAction() { step.next() }

    // The launcher scans on its own shortly after it starts; until that first
    // scan reports, "nothing found" would be a guess.
    property bool _scanDone: false
    readonly property bool _scanning: GameService.isScanning || !step._scanDone
    readonly property var _games: step._sorted(GameService.games, GameService.installedTranslations)
    readonly property int _catalogCount: GameService.supportedGamesModel
                                         ? GameService.supportedGamesModel.count : 0
    readonly property int _shown: Math.min(step._games.length, 6)

    function _sorted(games, installed) {
        const patched = {}
        for (const t of installed)
            patched[t.id] = t.hasUpdate === true
        const out = []
        for (const g of games) {
            const status = (g.id in patched) ? (patched[g.id] ? "update" : "installed") : "ready"
            out.push({ gameId: g.id, steamAppId: g.steamAppId || "", name: g.name || "", status: status })
        }
        const rank = { "ready": 0, "update": 1, "installed": 2 }
        out.sort((a, b) => rank[a.status] - rank[b.status] || a.name.localeCompare(b.name))
        return out
    }

    Connections {
        target: GameService
        function onScanCompleted() { step._scanDone = true }
    }
    // Never wait forever on a scan that does not come.
    Timer {
        interval: 30000
        running: !step._scanDone
        onTriggered: step._scanDone = true
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(step.width - 96, 880)
        spacing: 0

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: step._scanning ? qsTr("Oyunların aranıyor…")
                : step._games.length > 0 ? qsTr("Türkçe yaması olan %1 oyunun var").arg(step._games.length)
                : qsTr("Henüz Türkçe yaması olan bir oyun bulamadık")
            wrapMode: Text.WordWrap
            font.pixelSize: 32
            font.weight: Font.Bold
            font.letterSpacing: -0.6
            color: "#FFFFFF"
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.maximumWidth: 620
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: step._scanning
                ? qsTr("Steam, Epic, GOG ve diğer kütüphanelerin taranıyor. Bu birkaç saniye sürer.")
                : step._games.length > 0
                ? qsTr("Kurulmaya hazır olanları ana sayfadan tek tıkla Türkçe yapabilirsin.")
                : qsTr("Katalogda %1 oyun var. Yeni bir oyun kurduğunda launcher onu kendiliğinden bulur; başka bir klasördeki oyunu ana sayfadaki “Oyun ekle” ile ekleyebilirsin.").arg(step._catalogCount)
            wrapMode: Text.WordWrap
            lineHeight: 1.4
            font.pixelSize: 15
            color: Qt.rgba(1, 1, 1, 0.65)
        }

        // While scanning: a quiet indeterminate bar, not fake captions.
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 36
            Layout.preferredWidth: 240
            Layout.preferredHeight: 4
            radius: 2
            color: Qt.rgba(1, 1, 1, 0.08)
            visible: step._scanning
            clip: true

            Rectangle {
                id: runner
                width: parent.width * 0.35
                height: parent.height
                radius: parent.radius
                color: Theme.accentBase
                NumberAnimation on x {
                    running: step._scanning && step.visible
                    loops: Animation.Infinite
                    from: -runner.width
                    to: 240
                    duration: 1100
                    easing.type: Easing.InOutQuad
                }
            }
        }

        // Found: the games themselves, with what each one needs.
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 36
            visible: !step._scanning && step._games.length > 0
            spacing: 20

            Repeater {
                model: step._games.slice(0, step._shown)

                HomeCover {
                    id: cover
                    required property var modelData
                    width: 128
                    gameId: cover.modelData.gameId
                    steamAppId: cover.modelData.steamAppId
                    name: cover.modelData.name
                    status: cover.modelData.status
                    interactive: false
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 14
            visible: !step._scanning && step._games.length > step._shown
            textFormat: Text.PlainText
            text: qsTr("ve %1 oyun daha").arg(step._games.length - step._shown)
            font.pixelSize: 13
            color: Qt.rgba(1, 1, 1, 0.55)
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 40
            spacing: 12

            OnboardingButton {
                kind: "quiet"
                text: qsTr("Geri")
                onClicked: step.back()
            }
            OnboardingButton {
                kind: "quiet"
                visible: !step._scanning
                text: qsTr("Yeniden tara")
                onClicked: GameService.scanAllLibraries()
            }
            OnboardingButton {
                kind: "primary"
                text: qsTr("Devam")
                trailingIcon: "\uE72A"
                onClicked: step.next()
            }
        }
    }
}
