// SPDX-License-Identifier: AGPL-3.0-WITH-Commons-Clause
//
// Stale-While-Revalidate refresh badge — corner toast that auto-fades
// after 3 sn whenever ManifestSync.catalogRefreshed reports new content.
// See docs/specs/swr-cache.md.
import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: root

    // Anchored bottomRight by parent — caller positions us via anchors.
    width: badge.implicitWidth + 24
    height: badge.implicitHeight + 16
    visible: opacity > 0
    opacity: 0

    property int changedCount: 0

    function show(count: int): void {
        changedCount = count
        fadeAnim.restart()
    }

    Rectangle {
        id: badge
        anchors.fill: parent
        anchors.margins: 0
        radius: 10
        color: Theme.bgSecondary
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                text: "✨"
                font.pixelSize: 16
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: qsTr("Yeni içerik mevcut: %1 yama").arg(root.changedCount)
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    SequentialAnimation {
        id: fadeAnim
        NumberAnimation { target: root; property: "opacity"; to: 1.0; duration: 200 }
        PauseAnimation { duration: 2600 }
        NumberAnimation { target: root; property: "opacity"; to: 0.0; duration: 200 }
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
