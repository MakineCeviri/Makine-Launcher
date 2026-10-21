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
        color: Theme.withAlpha(Theme.bgPrimary, 0.95)

        Rectangle {
            anchors.fill: parent
            anchors.margins: Dimensions.spacingSection
            radius: Dimensions.radiusSection
            color: Theme.primary08
            border.width: 2
            border.color: Theme.primary50

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - Dimensions.paddingXXL * 2, 520)
                spacing: Dimensions.spacingLG

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 72
                    height: 72
                    radius: 36
                    color: Theme.primary15

                    Text {
                        anchors.centerIn: parent
                        textFormat: Text.PlainText
                        text: "\uED25"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 30
                        color: Theme.accentLight
                    }
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    text: drop.title
                    font.pixelSize: Dimensions.fontHeadline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    text: drop.subtitle
                    font.pixelSize: Dimensions.fontMD
                    color: Theme.textSecondary
                }
            }
        }
    }
}
