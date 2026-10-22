// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * FolderDropOverlay — a page that takes a folder dragged in from Explorer.
 *
 * Covers its parent while a folder (or file) is held over it and hands the
 * first one dropped to `folderDropped`. `accepts` decides whether this page
 * wants one at all; a refused drag passes to whatever lies below.
 */
DropArea {
    id: drop

    property string title: ""
    property string subtitle: ""
    property bool accepts: true

    signal folderDropped(url url)

    keys: ["text/uri-list"]
    onEntered: (drag) => {
        if (!drop.accepts || !drag.hasUrls) drag.accepted = false
    }
    onDropped: (ev) => {
        if (ev.hasUrls && ev.urls.length > 0) {
            ev.accept(Qt.CopyAction)
            drop.folderDropped(ev.urls[0])
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: drop.containsDrag
        color: Theme.withAlpha(Theme.bgPrimary, 0.92)

        Rectangle {
            anchors.fill: parent
            anchors.margins: 20
            radius: Dimensions.radiusHero
            color: Theme.withAlpha(Theme.primary, 0.08)
            border.width: 2
            border.color: Theme.withAlpha(Theme.primary, 0.6)

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 64, 520)
                spacing: 14

                IconTile {
                    anchors.horizontalCenter: parent.horizontalCenter
                    size: 64
                    glyph: Icons.folderInput
                    tint: Theme.primary
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    text: drop.title
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: Dimensions.title2
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                    text: drop.subtitle
                    font.pixelSize: Dimensions.body
                    color: Theme.textSecondary
                }
            }
        }
    }
}
