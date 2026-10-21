// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * WelcomeStep — the first screen anyone sees.
 *
 * One job: say what this is and why to trust it, then get out of the way.
 * Left: the promise, three reasons it is safe to try, one button. Right: the
 * catalogue itself — a tilted wall of covers drifting slowly in columns and
 * fading into the backdrop, the plainest proof that there is something here
 * for them. The old screen was a logo, a wordmark, an 11 px tagline at 25 %
 * white and a button; nothing on it said what "Başla" would lead to.
 */
Item {
    id: step

    signal next()

    // Off when the user turned animations off or the window is hidden.
    property bool animate: true

    function primaryAction() { step.next() }

    readonly property int _catalogCount: GameService.supportedGamesModel
                                         ? GameService.supportedGamesModel.count : 0
    readonly property bool _wide: step.width >= 960

    // Covers for the wall: the newest patches.
    property var _covers: []
    function _refreshCovers() {
        const out = []
        for (const e of GameService.recentlyUpdatedGames(24))
            out.push(e.steamAppId)
        step._covers = out
    }
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
        if (step.animate) entrance.start()
        else { copy.opacity = 1; copyShift.y = 0; wallArea.opacity = 1 }
    }
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { step._refreshCovers() }
    }

    ParallelAnimation {
        id: entrance
        NumberAnimation { target: copy; property: "opacity"; to: 1; duration: 520; easing.type: Easing.OutCubic }
        NumberAnimation { target: copyShift; property: "y"; to: 0; duration: 520; easing.type: Easing.OutCubic }
        SequentialAnimation {
            PauseAnimation { duration: 160 }
            NumberAnimation { target: wallArea; property: "opacity"; to: 1; duration: 900; easing.type: Easing.OutCubic }
        }
    }

    // ── Light behind the words ───────────────────────────────────────────────
    Canvas {
        id: glow
        x: copy.x - 260
        y: copy.y - 220
        width: 900
        height: 760
        renderStrategy: Canvas.Cooperative
        property color tint: Theme.accentBase
        onTintChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const g = ctx.createRadialGradient(width * 0.42, height * 0.45, 0,
                                               width * 0.42, height * 0.45, width * 0.5)
            g.addColorStop(0, Qt.rgba(tint.r, tint.g, tint.b, 0.22))
            g.addColorStop(0.55, Qt.rgba(tint.r, tint.g, tint.b, 0.07))
            g.addColorStop(1, Qt.rgba(tint.r, tint.g, tint.b, 0))
            ctx.fillStyle = g
            ctx.fillRect(0, 0, width, height)
        }
    }

    // ── The wall ─────────────────────────────────────────────────────────────
    // Bleeds past this step's box: up under the window controls, down to the
    // bottom edge, out to the right. Two soft masks fade it into the backdrop:
    // toward the words on the left, and at the top and bottom where columns
    // enter and leave.
    Item {
        id: wallArea
        visible: step._wide && step._covers.length >= 8
        x: step.width * 0.43
        y: -40
        width: step.width - x
        height: step.height + 40 + 56
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
                readonly property real coverW: 156
                readonly property real coverH: Math.round(coverW * 185 / 130)
                readonly property real gap: 20
                width: 4 * coverW + 3 * gap
                height: wallInner.height * 1.35
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
            GradientStop { position: 0.38; color: "#FFFFFFFF" }
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
            GradientStop { position: 0.2;  color: "#FFFFFFFF" }
            GradientStop { position: 0.8;  color: "#FFFFFFFF" }
            GradientStop { position: 1.0;  color: "transparent" }
        }
    }

    // ── The words ────────────────────────────────────────────────────────────
    ColumnLayout {
        id: copy
        x: step._wide ? Math.max(72, step.width * 0.07) : (step.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(step._wide ? step.width * 0.40 : step.width - 96, 520)
        spacing: 0
        opacity: 0
        transform: Translate { id: copyShift; y: 18 }

        Row {
            spacing: 12
            Layout.bottomMargin: 36

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 30; height: 30
                source: "qrc:/qt/qml/MakineLauncher/resources/images/logo_white.png"
                sourceSize: Qt.size(60, 60)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: "MAKİNE ÇEVİRİ"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.letterSpacing: 3.2
                color: Qt.rgba(1, 1, 1, 0.9)
            }
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: qsTr("Sevdiğin oyunlar,")
            font.pixelSize: 54
            font.weight: Font.Bold
            font.letterSpacing: -1.8
            lineHeight: 0.95
            color: "#FFFFFF"
        }

        // The promise in the brand's neon: pink → violet → cyan through the letters.
        Item {
            Layout.preferredWidth: promise.implicitWidth
            Layout.preferredHeight: promise.implicitHeight
            Layout.bottomMargin: 22

            Text {
                id: promise
                textFormat: Text.PlainText
                text: qsTr("artık Türkçe.")
                font.pixelSize: 54
                font.weight: Font.Bold
                font.letterSpacing: -1.8
                color: "white"
                visible: false
                layer.enabled: true
            }
            Rectangle {
                anchors.fill: promise
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#F472B6" }
                    GradientStop { position: 0.5; color: "#A78BFA" }
                    GradientStop { position: 1.0; color: "#22D3EE" }
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

        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 470
            Layout.bottomMargin: 34
            textFormat: Text.PlainText
            text: qsTr("Bilgisayarındaki oyunlar için Türkçe yamaları bulur, tek tıkla kurar. Oyunun orijinal dosyaları yedeklenir; istediğin an geri alırsın.")
            wrapMode: Text.WordWrap
            lineHeight: 1.45
            font.pixelSize: 16
            color: Qt.rgba(1, 1, 1, 0.7)
        }

        Repeater {
            model: [
                { icon: "\uE8F1",
                  text: step._catalogCount > 0
                        ? qsTr("%1 oyun için Türkçe yama").arg(step._catalogCount)
                        : qsTr("Yüzlerce oyun için Türkçe yama") },
                { icon: "\uEA18", text: qsTr("Orijinal dosyalar yedeklenir, tek tıkla geri alınır") },
                { icon: "\uEB51", text: qsTr("Kâr amacı gütmeyen, gönüllü bir topluluk projesi") }
            ]

            RowLayout {
                id: point
                required property var modelData
                Layout.fillWidth: true
                Layout.bottomMargin: 14
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 11
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.1)

                    Text {
                        anchors.centerIn: parent
                        textFormat: Text.PlainText
                        text: point.modelData.icon
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 15
                        color: Theme.accentLight
                    }
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: point.modelData.text
                    wrapMode: Text.WordWrap
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: Qt.rgba(1, 1, 1, 0.9)
                }
            }
        }

        Row {
            Layout.topMargin: 24
            spacing: 18

            OnboardingButton {
                kind: "primary"
                text: qsTr("Başlayalım")
                trailingIcon: "\uE72A"
                onClicked: step.next()
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: qsTr("3 kısa adım · yaklaşık bir dakika")
                font.pixelSize: 13
                color: Qt.rgba(1, 1, 1, 0.5)
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: copy.x
        anchors.bottomMargin: -32
        textFormat: Text.PlainText
        text: "v" + Qt.application.version + "  ·  makineceviri.org"
        font.pixelSize: 12
        color: Qt.rgba(1, 1, 1, 0.32)
    }
}
