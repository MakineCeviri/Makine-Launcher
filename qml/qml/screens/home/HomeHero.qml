// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeHero — the home page's lead: a few games, one at a time, each with the
 * one action that matters for it.
 *
 * Slides are plain objects: { gameId, steamAppId, name, installPath, engine,
 * kind, eyebrow, meta }. kind "installable" offers the install itself; every
 * other kind opens the game's page. Art is Steam's library hero; a game
 * without one shows its cover on a plain field.
 */
Item {
    id: hero

    property var slides: []
    property int current: 0

    signal openGame(var slide)
    signal installGame(var slide)

    readonly property var _slide: hero.slides.length > 0
                                  ? hero.slides[Math.min(hero.current, hero.slides.length - 1)]
                                  : null

    function _go(i) {
        if (hero.slides.length === 0) return
        hero.current = (i + hero.slides.length) % hero.slides.length
    }

    onSlidesChanged: if (hero.current >= hero.slides.length) hero.current = 0

    activeFocusOnTab: true
    Accessible.role: Accessible.Pane
    Accessible.name: hero._slide ? hero._slide.name : ""
    Keys.onLeftPressed: hero._go(hero.current - 1)
    Keys.onRightPressed: hero._go(hero.current + 1)

    // Moves on by itself only while nobody is looking at it closely.
    Timer {
        interval: 7000
        repeat: true
        running: hero.visible && hero.slides.length > 1 && !heroHover.hovered
                 && !hero.activeFocus && hero.Window.active
        onTriggered: hero._go(hero.current + 1)
    }

    HoverHandler { id: heroHover }

    // ── Art (rounded through a mask) ─────────────────────────────────────────
    Item {
        id: art
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: artMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.surface
        }

        Repeater {
            model: hero.slides

            Item {
                id: layerItem
                required property var modelData
                required property int index
                // New Steam apps keep their art under hashed paths we cannot
                // guess (Death Stranding 2: 404); they get the cover instead.
                readonly property bool fallback: heroArt.status === Image.Error
                property string coverSrc: ""
                function resolveCover() { layerItem.coverSrc = ImageCache.resolve(layerItem.modelData.steamAppId) }
                onFallbackChanged: if (layerItem.fallback) layerItem.resolveCover()

                anchors.fill: parent
                opacity: layerItem.index === hero.current ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Dimensions.easeStandard } }

                // Fixed target, gated by `enabled`: see GameCard (NATIVE-74).
                Connections {
                    target: ImageCache
                    enabled: layerItem.fallback && layerItem.coverSrc === ""
                    function onImageReady(readyId) {
                        if (readyId === layerItem.modelData.steamAppId) layerItem.resolveCover()
                    }
                }

                Image {
                    id: heroArt
                    anchors.fill: parent
                    source: GameService.steamHeroUrl(layerItem.modelData.steamAppId)
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 1920
                    asynchronous: true
                }

                // Fallback backdrop: the cover, blurred and dimmed, edge to edge.
                Image {
                    id: backdrop
                    anchors.fill: parent
                    visible: false
                    source: layerItem.fallback ? layerItem.coverSrc : ""
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(260, 370)
                    asynchronous: true
                }
                MultiEffect {
                    anchors.fill: parent
                    visible: layerItem.fallback && backdrop.status === Image.Ready
                    source: backdrop
                    blurEnabled: true
                    blurMax: 64
                    blur: 1.0
                    brightness: -0.3
                    saturation: -0.15
                }

                // Legibility: the text side darkens, the art side stays clean.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0;  color: Theme.withAlpha(Theme.bgPrimary, 0.94) }
                        GradientStop { position: 0.38; color: Theme.withAlpha(Theme.bgPrimary, 0.62) }
                        GradientStop { position: 0.72; color: Theme.withAlpha(Theme.bgPrimary, 0.0) }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * 0.5
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bgPrimary, 0.0) }
                        GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.7) }
                    }
                }

                // ...and the cover itself, sharp, above the scrims and clear
                // of the slide dots.
                Image {
                    visible: layerItem.fallback
                    anchors.right: parent.right
                    anchors.rightMargin: Dimensions.marginXXL + 24
                    anchors.top: parent.top
                    anchors.topMargin: Dimensions.marginXL + 8
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Dimensions.marginXXL * 2
                    width: height * 130 / 185
                    source: layerItem.coverSrc
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }
            }
        }
    }

    Item {
        id: artMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusSection + 4
            color: "white"
        }
    }

    // Hairline edge so the card holds its shape on a dark page.
    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusSection + 4
        color: "transparent"
        border.width: hero.activeFocus ? 2 : 1
        border.color: hero.activeFocus ? Theme.primary : Theme.textPrimary08
    }

    // ── Text and actions ─────────────────────────────────────────────────────
    ColumnLayout {
        id: copy
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: Dimensions.marginXXL
        anchors.bottomMargin: Dimensions.marginXL + 8
        width: Math.min(560, hero.width * 0.5)
        spacing: Dimensions.spacingLG

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: hero._slide ? hero._slide.eyebrow : ""
            elide: Text.ElideRight
            font.pixelSize: Dimensions.fontXS
            font.weight: Font.Bold
            font.letterSpacing: 1.4
            font.capitalization: Font.AllUppercase
            color: Theme.darkMode ? Theme.accentLight : Theme.accentDark
        }

        // Steam's logo art when there is one; the name otherwise.
        Image {
            id: logo
            Layout.preferredWidth: Math.min(380, copy.width)
            Layout.preferredHeight: Math.min(120, hero.height * 0.3)
            visible: status === Image.Ready
            source: hero._slide ? GameService.steamLogoUrl(hero._slide.steamAppId) : ""
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            verticalAlignment: Image.AlignBottom
            asynchronous: true
            mipmap: true
        }
        Text {
            Layout.fillWidth: true
            visible: !logo.visible
            textFormat: Text.PlainText
            text: hero._slide ? hero._slide.name : ""
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Dimensions.fontBanner
            font.weight: Font.Bold
            font.letterSpacing: Dimensions.letterSpacingHeadline
            lineHeight: 1.1
            color: Theme.textPrimary
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            textFormat: Text.PlainText
            text: hero._slide ? hero._slide.meta : ""
            elide: Text.ElideRight
            font.pixelSize: Dimensions.fontBody
            color: Theme.textSecondary
        }

        Row {
            Layout.topMargin: Dimensions.spacingSM
            spacing: Dimensions.spacingMD

            HomeButton {
                kind: "primary"
                icon: hero._slide && hero._slide.kind === "installable" ? "" : ""
                text: !hero._slide ? ""
                    : hero._slide.kind === "installable" ? qsTr("Türkçe Yamayı Kur")
                    : hero._slide.kind === "update" ? qsTr("Yamayı Güncelle")
                    : qsTr("İncele")
                onClicked: {
                    if (hero._slide.kind === "installable") hero.installGame(hero._slide)
                    else hero.openGame(hero._slide)
                }
            }
            HomeButton {
                kind: "secondary"
                visible: hero._slide !== null && hero._slide.kind === "installable"
                text: qsTr("Ayrıntılar")
                onClicked: hero.openGame(hero._slide)
            }
        }
    }

    // ── Which slide ──────────────────────────────────────────────────────────
    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Dimensions.marginXXL
        anchors.bottomMargin: Dimensions.marginXL + 8 + 17
        spacing: Dimensions.spacingSM
        visible: hero.slides.length > 1

        Repeater {
            model: hero.slides.length

            Rectangle {
                id: dot
                required property int index
                readonly property bool active: dot.index === hero.current
                anchors.verticalCenter: parent.verticalCenter
                width: dot.active ? 24 : 8
                height: 8
                radius: 4
                color: dot.active ? Theme.textPrimary : Theme.textPrimary25
                Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Dimensions.easeStandard } }
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Öne çıkan %1").arg(dot.index + 1)

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    // A finger-sized target around an 8 px dot.
                    margin: 8
                    onTapped: hero._go(dot.index)
                }
            }
        }
    }
}
