// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
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
 *
 * In the page's language: a display title, and one lit container that shows
 * the scan running, then the games themselves rising in one after another,
 * or what to do when there are none.
 */
Item {
    id: step

    signal next()

    function primaryAction() { step.next() }

    // The launcher scans on its own shortly after it starts; until that first
    // scan reports, "nothing found" would be a guess.
    property bool _scanDone: false
    readonly property bool _scanning: GameService.isScanning || !step._scanDone
    // GameService.library: this PC's games with a patch and each one's state
    // (ready, update, installed, external — a patch on another team's site).
    readonly property var _games: step._sorted(GameService.library)
    readonly property int _catalogCount: GameService.supportedGamesModel
                                         ? GameService.supportedGamesModel.count : 0
    readonly property int _shown: Math.min(step._games.length, 5)

    function _sorted(library) {
        const out = []
        for (const g of library)
            out.push({ gameId: g.id, steamAppId: g.steamAppId || "", name: g.name || "",
                       status: g.status || "ready" })
        const rank = { "ready": 0, "update": 1, "installed": 2, "soon": 3 }
        out.sort((a, b) => (rank[a.status] ?? 4) - (rank[b.status] ?? 4) || a.name.localeCompare(b.name))
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
        width: Math.min(step.width - 96, 860)
        spacing: 0

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: step._scanning ? qsTr("Oyunların aranıyor…")
                : step._games.length > 0 ? qsTr("Türkçe yaması olan %1 oyunun var").arg(step._games.length)
                : qsTr("Henüz Türkçe yaması olan bir oyun bulamadık")
            wrapMode: Text.WordWrap
            font.family: Dimensions.fontDisplay
            font.pixelSize: 34
            font.weight: Font.Bold
            font.letterSpacing: -0.8
            color: Theme.textPrimary
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
                : qsTr("Katalogda %1 oyun var. Yeni bir oyun kurduğunda launcher onu kendiliğinden bulur; başka bir klasördeki oyunu Kütüphanem'deki \"Oyun ekle\" ile ekleyebilirsin.").arg(step._catalogCount)
            wrapMode: Text.WordWrap
            lineHeight: 1.4
            font.pixelSize: Dimensions.headline
            color: Theme.textSecondary
        }

        Card {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 34
            Layout.preferredWidth: step._scanning || step._games.length === 0
                                   ? 460 : Math.min(step.width - 96, shelf.implicitWidth + 64)
            implicitHeight: step._scanning ? 132
                          : step._games.length > 0 ? shelf.implicitHeight + 56 + (moreText.visible ? 26 : 0)
                          : 132
            radius: Dimensions.radiusSection
            glow: !step._scanning && step._games.length > 0
            Behavior on implicitHeight { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

            // While scanning: what is looked at, and a quiet running line
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 64
                visible: step._scanning
                spacing: 16
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10
                    Spinner { size: 18; color: Theme.primaryText }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Kütüphaneler taranıyor")
                        font.pixelSize: Dimensions.body
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Theme.fillTertiary
                    clip: true
                    Rectangle {
                        id: runner
                        width: parent.width * 0.32
                        height: parent.height
                        radius: parent.radius
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0) }
                            GradientStop { position: 0.5; color: Theme.primary }
                            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.primary, 0) }
                        }
                        NumberAnimation on x {
                            running: step._scanning && step.visible
                            loops: Animation.Infinite
                            from: -runner.width
                            to: runner.parent.width
                            duration: 1300
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }

            // Found: the games, rising in one after another
            Row {
                id: shelf
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 28
                visible: !step._scanning && step._games.length > 0
                spacing: 20

                Repeater {
                    model: step._scanning ? [] : step._games.slice(0, step._shown)

                    HomeCover {
                        id: cover
                        required property var modelData
                        required property int index
                        width: 124
                        gameId: cover.modelData.gameId
                        steamAppId: cover.modelData.steamAppId
                        name: cover.modelData.name
                        status: cover.modelData.status
                        interactive: false
                        opacity: 0
                        transform: Translate { id: coverLift; y: 18 }
                        Component.onCompleted: coverIn.start()
                        SequentialAnimation {
                            id: coverIn
                            PauseAnimation { duration: cover.index * 80 }
                            ParallelAnimation {
                                NumberAnimation { target: cover; property: "opacity"; to: 1; duration: 420; easing.type: Easing.OutCubic }
                                NumberAnimation { target: coverLift; property: "y"; to: 0; duration: 700; easing.type: Easing.OutExpo }
                            }
                        }
                    }
                }
            }
            Text {
                id: moreText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                visible: !step._scanning && step._games.length > step._shown
                textFormat: Text.PlainText
                text: qsTr("ve %1 oyun daha").arg(step._games.length - step._shown)
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }

            // None: where the launcher looks, and what else to do
            RowLayout {
                anchors.centerIn: parent
                width: parent.width - 56
                visible: !step._scanning && step._games.length === 0
                spacing: 16
                IconTile { size: 44; glyph: Icons.folderSearch; tint: Theme.primary }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    text: qsTr("Steam, Epic, GOG, Xbox ve Ubisoft kütüphanelerine baktık. Bir oyunu kurduğunda burada görürsün.")
                    font.pixelSize: Dimensions.callout
                    lineHeight: 1.3
                    color: Theme.textSecondary
                }
            }
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 18
            visible: !step._scanning
            style: "gray"
            icon: Icons.refreshCw
            text: qsTr("Yeniden tara")
            onClicked: GameService.scanAllLibraries()
        }
    }
}
