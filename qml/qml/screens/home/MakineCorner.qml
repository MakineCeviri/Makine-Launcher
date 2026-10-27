// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * MakineCorner — Makine's own corner on the home page, beside the featured art.
 *
 * He is not drawn here: he floats over it in a window of his own (CompanionService)
 * and only learns where it is. While most of the corner is in view he lives in it
 * (the same size as in the sidebar); scrolled away, or on another page, he flies
 * back to the sidebar's room. Under him, what you can do with him and the way to his settings.
 */
Card {
    id: corner

    // The page it scrolls in, and how much of that page's top is under the top strip
    property Flickable view: null
    property real clearTop: 0

    radius: Dimensions.radiusHero
    glow: true
    glowCorner: "bottomLeft"
    glowStrength: 0.1

    // He comes when most of it shows and leaves when most of it is gone (not both at one line,
    // so a little scrolling does not send him to and fro)
    property bool _here: false
    function report() {
        if (typeof Companion === "undefined" || !Companion) return
        let here = corner.visible && corner.view !== null && stage.height > 0
        if (here) {
            const top = stage.mapToItem(corner.view, 0, 0).y
            const shown = (Math.min(corner.view.height, top + stage.height) - Math.max(corner.clearTop, top)) / stage.height
            here = shown > (corner._here ? 0.45 : 0.75)
        }
        corner._here = here
        Companion.setCorner(here ? stage.mapToItem(null, 0, 0, stage.width, stage.height) : Qt.rect(0, 0, 0, 0))
    }
    onVisibleChanged: report()
    onWidthChanged: report()
    onHeightChanged: report()
    Connections {
        target: corner.view
        function onContentYChanged() { corner.report() }
        function onHeightChanged() { corner.report() }
    }
    // What moves it without a word to it (the rows above it, its own rise into place): a look now and then
    Timer {
        interval: 250
        repeat: true
        running: corner.visible
        onTriggered: corner.report()
    }

    // Where he floats; his light falls on its floor
    Item {
        id: stage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: foot.top
        anchors.topMargin: 10
        anchors.bottomMargin: 4
    }

    ColumnLayout {
        id: foot
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 18
        anchors.rightMargin: 12
        anchors.bottomMargin: 14
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: qsTr("Makine")
                font.pixelSize: Dimensions.headline
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
            AppButton {
                style: "gray"
                size: "small"
                icon: Icons.settings
                toolTip: qsTr("Makine'nin ayarları")
                onClicked: if (typeof Companion !== "undefined" && Companion) Companion.settingsRequested()
            }
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: qsTr("Tıkla, başından ya da elinden tutup gezdir. Sağ tıklarsan ne yapacağını seçersin.")
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
        }
    }
}
