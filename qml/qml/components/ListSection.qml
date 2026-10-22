// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ListSection — Apple's grouped inset list: a heading, rows on one rounded
 * card with hairlines between them, an explanatory footer under it.
 *
 *   ListSection {
 *       title: qsTr("Başlangıç")
 *       footer: qsTr("…")
 *       ListRow { … }
 *       ListRow { … }
 *   }
 *
 * ListRows get their separator and corner shape from here; any other item
 * is laid out as it is.
 */
ColumnLayout {
    id: section

    property string title: ""
    property string footer: ""
    default property alias rows: col.data

    Layout.fillWidth: true
    spacing: 7

    function _relayout() {
        const rows = []
        for (let i = 0; i < col.children.length; ++i) {
            const c = col.children[i]
            if (c && c.visible && c._isListRow === true) rows.push(c)
        }
        for (let j = 0; j < rows.length; ++j) {
            rows[j]._isFirst = j === 0
            rows[j]._isLast = j === rows.length - 1
        }
    }
    function _watch() {
        for (let i = 0; i < col.children.length; ++i) {
            const c = col.children[i]
            if (c && c._isListRow === true && !c._watched) {
                c._watched = true
                c.visibleChanged.connect(section._relayout)
            }
        }
        section._relayout()
    }

    Text {
        visible: section.title !== ""
        Layout.fillWidth: true
        Layout.leftMargin: 6
        textFormat: Text.PlainText
        text: section.title
        font.pixelSize: Dimensions.callout
        font.weight: Font.DemiBold
        color: Theme.textSecondary
        elide: Text.ElideRight
    }

    Card {
        Layout.fillWidth: true
        implicitHeight: col.implicitHeight
        radius: Dimensions.radiusLG

        ColumnLayout {
            id: col
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0
            onChildrenChanged: Qt.callLater(section._watch)
            Component.onCompleted: section._watch()
        }
    }

    Text {
        visible: section.footer !== ""
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        textFormat: Text.PlainText
        text: section.footer
        wrapMode: Text.WordWrap
        font.pixelSize: Dimensions.footnote
        lineHeight: 1.15
        color: Theme.textMuted
    }
}
