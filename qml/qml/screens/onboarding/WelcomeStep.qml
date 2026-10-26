// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * WelcomeStep — the first screen anyone sees.
 *
 * One job: say what this is and why to trust it, then get out of the way.
 * Left: the promise in the page's display type (its second line in the
 * accent), what it does in a sentence, three reasons it is safe to try on a
 * lit container. Right: the catalogue itself — a tilted wall of covers
 * drifting slowly in columns and fading into the page, the plainest proof
 * that there is something here for them. The words arrive one after
 * another; the wizard's foot carries the button.
 */
Item {
    id: step

    signal next()

    // Off when the user turned animations off or the window is hidden.
    property bool animate: true

    function primaryAction() { step.next() }

    readonly property int _catalogCount: GameService.supportedGamesModel
                                         ? GameService.supportedGamesModel.count : 0
    readonly property bool _wide: step.width >= 900

    // Covers for the wall: the newest patches whose cover is already on disk
    // (one still on its way, or missing, would leave a blank tile).
    property var _covers: []
    function _refreshCovers() {
        const out = []
        for (const e of GameService.recentlyUpdatedGames(48)) {
            if (e.steamAppId && ImageCache.resolve(e.steamAppId) !== "")
                out.push(e.steamAppId)
            if (out.length >= 24) break
        }
        if (out.join() !== step._covers.join())
            step._covers = out
    }
    // Covers arriving fill a wall that was short of them (not after: a
    // changing wall would jump)
    Connections {
        target: ImageCache
        enabled: step._covers.length < 24
        function onImageReady() { coverRefresh.restart() }
    }
    Timer { id: coverRefresh; interval: 800; onTriggered: step._refreshCovers() }
    // Six per column, each column its own slice of the list.
    function _column(i) {
        const n = step._covers.length
        const out = []
        for (let k = 0; k < 6 && n > 0; ++k)
            out.push(step._covers[(i * 6 + k) % n])
        return out
    }
    Component.onCompleted: {
        _refreshCovers()
        if (step.animate) wallIn.start()
        else wallArea.opacity = 1
    }
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { step._refreshCovers() }
    }

    NumberAnimation {
        id: wallIn
        target: wallArea; property: "opacity"; to: 1
        duration: 1100; easing.type: Easing.OutCubic
    }

    // ── The wall ─────────────────────────────────────────────────────────────
    // Two soft masks fade it into the page: toward the words on the left, and
    // at the top and bottom where columns enter and leave.
    Item {
        id: wallArea
        visible: step._wide && step._covers.length >= 8
        x: step.width * 0.46
        y: 0
        width: step.width - x
        height: step.height
        opacity: 0

        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: fadeSides
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Item {
            id: wallInner
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: fadeEnds
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }

            Item {
                id: group
                readonly property real coverW: 150
                readonly property real coverH: Math.round(coverW * 185 / 130)
                readonly property real gap: 20
                width: 4 * coverW + 3 * gap
                height: wallInner.height * 1.4
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: 40

                // Tipped back and turned: a shelf seen from the side, not a table.
                transform: [
                    Rotation {
                        origin.x: group.width / 2
                        origin.y: group.height / 2
                        axis { x: 0; y: 1; z: 0 }
                        angle: -18
                    },
                    Rotation {
                        origin.x: group.width / 2
                        origin.y: group.height / 2
                        angle: 9
                    }
                ]

                Repeater {
                    model: 4

                    Item {
                        id: column
                        required property int index
                        readonly property var covers: step._column(column.index)
                        readonly property real setHeight: column.covers.length * (group.coverH + group.gap)
                        // Alternate columns run the other way and start half a cover lower.
                        readonly property bool rising: column.index % 2 === 0
                        readonly property real base: -(column.index % 2) * group.coverH * 0.5
                                                     - column.index * 37

                        x: column.index * (group.coverW + group.gap)
                        width: group.coverW
                        height: group.height

                        Column {
                            id: strip
                            spacing: group.gap
                            y: column.rising ? column.base : column.base - column.setHeight

                            // Three copies: the loop moves by one, the other
                            // two keep the column full at either end.
                            Repeater {
                                model: column.covers.concat(column.covers).concat(column.covers)

                                Image {
                                    id: tile
                                    required property string modelData
                                    required property int index
                                    width: group.coverW
                                    height: group.coverH
                                    fillMode: Image.PreserveAspectCrop
                                    sourceSize: Qt.size(260, 370)
                                    asynchronous: true
                                    mipmap: true
                                    source: tile._src
                                    opacity: status === Image.Ready ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }

                                    property string _src: ImageCache.resolve(tile.modelData)
                                    // Fixed target, gated by `enabled`: see GameCard (NATIVE-74).
                                    Connections {
                                        target: ImageCache
                                        enabled: tile._src === ""
                                        function onImageReady(readyId) {
                                            if (readyId === tile.modelData)
                                                tile._src = ImageCache.resolve(tile.modelData)
                                        }
                                    }
                                }
                            }
                        }

                        // A slow drift, ~14 px/s: alive, never busy.
                        NumberAnimation {
                            target: strip
                            property: "y"
                            from: column.rising ? column.base : column.base - column.setHeight
                            to: column.rising ? column.base - column.setHeight : column.base
                            duration: Math.max(1, column.setHeight / 14 * 1000)
                            loops: Animation.Infinite
                            running: step.animate && wallArea.visible && column.setHeight > 0
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: fadeSides
        x: wallArea.x; y: wallArea.y
        width: wallArea.width; height: wallArea.height
        visible: false
        layer.enabled: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0;  color: "transparent" }
            GradientStop { position: 0.4;  color: "#FFFFFFFF" }
            GradientStop { position: 1.0;  color: "#FFFFFFFF" }
        }
    }
    Rectangle {
        id: fadeEnds
        x: wallArea.x; y: wallArea.y
        width: wallArea.width; height: wallArea.height
        visible: false
        layer.enabled: true
        gradient: Gradient {
            GradientStop { position: 0.0;  color: "transparent" }
            GradientStop { position: 0.18; color: "#FFFFFFFF" }
            GradientStop { position: 0.82; color: "#FFFFFFFF" }
            GradientStop { position: 1.0;  color: "transparent" }
        }
    }

    // ── The words ────────────────────────────────────────────────────────────
    ColumnLayout {
        id: copy
        x: step._wide ? Math.max(56, step.width * 0.06) : (step.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(step._wide ? step.width * 0.44 : step.width - 96, 540)
        spacing: 0

        Arrive {
            order: 0
            Layout.fillWidth: true
            implicitHeight: line1.implicitHeight
            Text {
                id: line1
                width: parent.width
                textFormat: Text.PlainText
                text: qsTr("Sevdiğin oyunlar,")
                font.family: Dimensions.fontDisplay
                font.pixelSize: 54
                font.weight: Font.Bold
                font.letterSpacing: -1.6
                lineHeight: 0.95
                color: Theme.textPrimary
            }
        }

        // The promise in the accent, lit from its lighter tone
        Arrive {
            order: 1
            Layout.preferredWidth: promise.implicitWidth
            Layout.preferredHeight: promise.implicitHeight
            Layout.bottomMargin: 22
            Text {
                id: promise
                textFormat: Text.PlainText
                text: qsTr("artık Türkçe.")
                font.family: Dimensions.fontDisplay
                font.pixelSize: 54
                font.weight: Font.Bold
                font.letterSpacing: -1.6
                color: "white"
                visible: false
                layer.enabled: true
            }
            Rectangle {
                anchors.fill: promise
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.accentLightest }
                    GradientStop { position: 0.55; color: Theme.primaryText }
                    GradientStop { position: 1.0; color: Theme.primary }
                }
                layer.enabled: true
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: promise
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                }
            }
        }

        Arrive {
            order: 2
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.bottomMargin: 30
            implicitHeight: intro.implicitHeight
            Text {
                id: intro
                width: parent.width
                textFormat: Text.PlainText
                text: qsTr("Bilgisayarındaki oyunlar için Türkçe yamaları bulur, tek tıkla kurar. Oyunun orijinal dosyaları yedeklenir; istediğin an geri alırsın.")
                wrapMode: Text.WordWrap
                lineHeight: 1.4
                font.pixelSize: Dimensions.headline
                color: Theme.textSecondary
            }
        }

        // Three reasons to trust it, on one lit container
        Arrive {
            order: 3
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            implicitHeight: reasons.implicitHeight
            ListSection {
                id: reasons
                width: parent.width
                ListRow {
                    icon: Icons.library
                    iconTint: Theme.primary
                    title: step._catalogCount > 0
                           ? qsTr("%1 oyun için Türkçe yama").arg(step._catalogCount)
                           : qsTr("Yüzlerce oyun için Türkçe yama")
                    subtitle: qsTr("Yeni yamalar geldikçe katalog kendiliğinden güncellenir.")
                }
                ListRow {
                    icon: Icons.shieldCheck
                    iconTint: Theme.success
                    title: qsTr("Orijinal dosyalar yedeklenir")
                    subtitle: qsTr("Yamayı tek tıkla kaldırır, oyunu eski hâline getirirsin.")
                }
                ListRow {
                    icon: Icons.heartHandshake
                    iconTint: "#FF375F"
                    title: qsTr("Kâr amacı gütmeyen bir topluluk")
                    subtitle: qsTr("Yamalar gönüllü çevirmenlerin emeği; launcher ücretsiz ve açık kaynak.")
                }
            }
        }

        Arrive {
            order: 4
            Layout.topMargin: 18
            implicitWidth: steps.implicitWidth
            implicitHeight: steps.implicitHeight
            Row {
                id: steps
                spacing: 8
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: Icons.clock
                    size: 14
                    color: Theme.textMuted
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    text: qsTr("3 kısa adım · yaklaşık bir dakika")
                    font.pixelSize: Dimensions.footnote
                    font.family: Dimensions.fontText
                    color: Theme.textMuted
                }
            }
        }
    }

    // One part of the words, rising into place after the one before
    component Arrive: Item {
        id: arrive
        property int order: 0
        default property alias content: holder.data
        Item { id: holder; anchors.fill: parent }
        opacity: 0
        transform: Translate { id: lift; y: 16 }
        Component.onCompleted: {
            if (!step.animate || Dimensions.animSlow === 0) { arrive.opacity = 1; lift.y = 0; return }
            rise.start()
        }
        SequentialAnimation {
            id: rise
            PauseAnimation { duration: 120 + arrive.order * 90 }
            ParallelAnimation {
                NumberAnimation { target: arrive; property: "opacity"; to: 1; duration: 480; easing.type: Easing.OutCubic }
                NumberAnimation { target: lift; property: "y"; to: 0; duration: 760; easing.type: Easing.OutExpo }
            }
        }
    }
}
