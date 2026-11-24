// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * LiveTour — the Makine Live page's guided tour ("Nasıl kullanılır?"). One step
 * at a time: the page scrolls to the step's section, the rest of the page dims
 * round it, and a card says what the section is for (Geri / İleri, the last one
 * "Başlayalım", and "Geç"; Esc skips). Makine, the companion, says a short version
 * in his bubble (when his speech is on); the card always says all of it.
 *
 * A step whose section is not on the page (Live not running yet, no pools) shows
 * the card alone, so the tour never waits on the plugin. While a dialog is open
 * over the page (`paused`) the tour waits under it and leaves Esc to the dialog.
 */
Item {
    id: tour

    // [{target: Item | null, title, text, say}]
    property var steps: []
    property Flickable flick: null
    property bool paused: false

    property int index: 0
    property bool active: false
    readonly property var step: active && index >= 0 && index < steps.length ? steps[index] : null
    readonly property Item target: step && step.target && step.target.visible ? step.target : null
    readonly property bool last: index >= steps.length - 1

    // completed: the last step's "Başlayalım"; otherwise skipped (Geç, Esc, the page left)
    signal finished(bool completed)

    function start() {
        index = 0
        active = true   // (the page binds `steps` while the tour is active)
        if (steps.length === 0) { active = false; return }
        _scroll()
    }
    function next() {
        if (last) { _end(true); return }
        index++
        _scroll()
    }
    function back() {
        if (index <= 0) return
        index--
        _scroll()
    }
    function skip() { if (active) _end(false) }

    function _end(completed) {
        active = false
        tour.finished(completed)
    }
    // Makine says the step's short line (and clears it when the tour ends); a section that comes or goes
    // while its step shows (the plugin's state arrived) is scrolled to again
    readonly property string _line: step && step.say ? step.say : ""
    on_LineChanged: if (typeof Companion !== "undefined" && Companion) Companion.say(_line)
    onTargetChanged: if (active) _scroll()
    // The step's section a little below the page's top edge (under the top dissolve), the card under it
    function _scroll() {
        if (!tour.flick) return
        const t = tour.target
        const y = t ? t.mapToItem(tour.flick.contentItem, 0, 0).y - 72 : 0
        scrollAnim.to = Math.max(0, Math.min(y, tour.flick.contentHeight - tour.flick.height))
        scrollAnim.restart()
    }
    NumberAnimation {
        id: scrollAnim
        target: tour.flick
        property: "contentY"
        duration: Dimensions.animSlow
        easing.type: Easing.OutCubic
    }

    // The section's rect on the tour, a little larger; follows scrolling, resizing and the page's layout
    readonly property rect hole: {
        const t = tour.target
        if (!t || !tour.flick) return Qt.rect(0, 0, 0, 0)
        const deps = [tour.flick.contentY, tour.flick.contentHeight, tour.width, tour.height, t.y, t.width, t.height]
        const r = t.mapToItem(tour, 0, 0, t.width, t.height)
        return deps.length > 0 ? Qt.rect(r.x - 10, r.y - 10, r.width + 20, r.height + 20) : Qt.rect(0, 0, 0, 0)
    }
    readonly property bool _hasHole: hole.width > 0 && hole.height > 0

    visible: active || fade.running
    opacity: active ? 1 : 0
    Behavior on opacity { NumberAnimation { id: fade; duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

    // Nothing under the tour takes a click, a wheel or a hover while it runs
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        preventStealing: true
        onWheel: (wheel) => { wheel.accepted = true }
    }

    // The dim, with the section cut out of it (rounded like the section's card)
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: -1
            fillColor: Theme.scrim
            fillRule: ShapePath.OddEvenFill
            PathRectangle { width: tour.width; height: tour.height }
            PathRectangle {
                x: tour._hasHole ? tour.hole.x : tour.width / 2
                y: tour._hasHole ? tour.hole.y : tour.height / 2
                width: tour._hasHole ? tour.hole.width : 0
                height: tour._hasHole ? tour.hole.height : 0
                radius: Dimensions.radiusSection + 6
            }
        }
    }
    // A calm accent line round the section
    Rectangle {
        visible: tour._hasHole
        x: tour.hole.x
        y: tour.hole.y
        width: tour.hole.width
        height: tour.hole.height
        radius: Dimensions.radiusSection + 6
        color: "transparent"
        border.width: 2
        border.color: Theme.primary
    }

    Card {
        id: card
        reveal: false
        glow: true
        radius: Dimensions.radiusHero
        width: Math.min(440, tour.width - Dimensions.pageMargin * 2)
        implicitHeight: cardCol.implicitHeight + 40
        x: tour.width - width - Dimensions.pageMargin
        // under the section when it fits, else over it, else at the bottom (a section taller than the view)
        y: {
            const m = 16, h = card.height
            if (!tour._hasHole) return Math.round((tour.height - h) / 2)
            const below = tour.hole.y + tour.hole.height + m
            if (below + h <= tour.height - m) return below
            const above = tour.hole.y - m - h
            if (above >= m) return above
            return tour.height - h - m
        }
        Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

        ColumnLayout {
            id: cardCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Icon {
                    glyph: Icons.circleHelp
                    size: 14
                    color: Theme.primaryText
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Makine Live nasıl kullanılır?")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    font.pixelSize: Dimensions.footnote
                    font.weight: Font.DemiBold
                    color: Theme.primaryText
                }
                Text {
                    text: qsTr("%1 / %2").arg(tour.index + 1).arg(tour.steps.length)
                    textFormat: Text.PlainText
                    font.pixelSize: Dimensions.footnote
                    color: Theme.textMuted
                }
            }
            Text {
                Layout.fillWidth: true
                text: tour.step ? tour.step.title : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.family: Dimensions.fontDisplay
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                text: tour.step ? tour.step.text : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.body
                lineHeight: 1.2
                color: Theme.textSecondary
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 8
                AppButton {
                    style: "plain"
                    text: qsTr("Geç")
                    toolTip: qsTr("Turu kapatır; başlıktaki \"Nasıl kullanılır?\" ile yeniden açılır.")
                    onClicked: tour.skip()
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    visible: tour.index > 0
                    style: "gray"
                    text: qsTr("Geri")
                    onClicked: tour.back()
                }
                AppButton {
                    style: "filled"
                    text: tour.last ? qsTr("Başlayalım") : qsTr("İleri")
                    onClicked: tour.next()
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: tour.active && !tour.paused
        onActivated: tour.skip()
    }
}
