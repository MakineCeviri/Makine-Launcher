// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * RefreshIndicator — a short-lived capsule when a background catalogue
 * refresh brought new patches (ManifestSync.catalogRefreshed). The caller
 * anchors it; it fades in, stays three seconds, fades out.
 */
Item {
    id: root

    width: badge.implicitWidth
    height: badge.implicitHeight
    visible: opacity > 0
    opacity: 0

    property int changedCount: 0

    function show(count: int): void {
        changedCount = count
        fadeAnim.restart()
    }

    RectangularShadow {
        anchors.fill: badge
        radius: badge.radius
        offset.y: 4
        blur: 16
        color: Theme.shadowMedium
    }

    Rectangle {
        id: badge
        implicitWidth: row.implicitWidth + 28
        implicitHeight: 40
        radius: height / 2
        color: Theme.surfaceElevated
        border.width: Theme.darkMode ? 1 : 0
        border.color: Theme.separator

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Icon {
                glyph: Icons.sparkles
                size: 16
                color: Theme.primary
            }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Katalog güncellendi: %1 yama yeni ya da değişti").arg(root.changedCount)
                color: Theme.textPrimary
                font.pixelSize: Dimensions.callout
                font.weight: Font.Medium
            }
        }
    }

    SequentialAnimation {
        id: fadeAnim
        NumberAnimation { target: root; property: "opacity"; to: 1.0; duration: Dimensions.animMedium }
        PauseAnimation { duration: 3000 }
        NumberAnimation { target: root; property: "opacity"; to: 0.0; duration: Dimensions.animMedium }
    }

    Connections {
        target: typeof ManifestSync !== "undefined" ? ManifestSync : null
        ignoreUnknownSignals: true
        function onCatalogRefreshed(totalCount, changedCount) {
            if (changedCount > 0)
                root.show(changedCount)
        }
    }
}
