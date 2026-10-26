// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * AboutSection — the patch's notes and the game's description.
 *
 * The notes come first and always show when the catalogue has any: they
 * hold what the patch needs before it is installed (an online mode to turn
 * off, a redistributable) as much as after. The description folds to three
 * lines with "devamını oku". Both sit in the detail page's one container
 * (HeroSection), under a lit hairline.
 */
ColumnLayout {
    id: aboutRoot

    required property var vm

    spacing: 0
    // Not notesCard.visible: a child's visible follows its parent's, so the
    // section could never turn itself on.
    readonly property bool _hasNotes: aboutRoot.vm.translationNotes !== ""
    readonly property bool _hasDesc: aboutRoot.vm.description !== ""
    visible: _hasNotes || _hasDesc

    // ── Patch notes ──────────────────────────────────────────────────────
    Item {
        id: notesCard
        Layout.fillWidth: true
        visible: aboutRoot._hasNotes
        implicitHeight: notesCol.implicitHeight + 40

        ColumnLayout {
            id: notesCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            RowLayout {
                spacing: 10
                IconTile { size: 26; glyph: Icons.scrollText; tint: Theme.warning }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Yama notları")
                    font.pixelSize: Dimensions.title3
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: aboutRoot.vm.translationNotes
                font.pixelSize: Dimensions.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.35
            }
        }
    }

    FadeLine {
        visible: aboutRoot._hasNotes && aboutRoot._hasDesc
        lit: true
    }

    // ── About the game ───────────────────────────────────────────────────
    Item {
        id: descCard
        Layout.fillWidth: true
        visible: aboutRoot._hasDesc
        implicitHeight: descCol.implicitHeight + 40

        // Hidden measurement — does the full text run past three lines?
        // Outside the layout: it only has to be as wide as the text below.
        Text {
            id: measure
            textFormat: Text.PlainText
            visible: false
            width: descCol.width
            text: aboutRoot.vm.description
            font.pixelSize: Dimensions.body
            wrapMode: Text.WordWrap
            lineHeight: 1.35
            maximumLineCount: 3
        }

        ColumnLayout {
            id: descCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            Text {
                textFormat: Text.PlainText
                text: qsTr("Hakkında")
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                color: Theme.textPrimary
            }

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: aboutRoot.vm.description
                font.pixelSize: Dimensions.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                maximumLineCount: aboutRoot.vm.descriptionExpanded ? 9999 : 3
                elide: Text.ElideRight
            }

            AppButton {
                visible: measure.truncated
                size: "small"
                style: "plain"
                Layout.leftMargin: -12
                text: aboutRoot.vm.descriptionExpanded ? qsTr("Daha az göster") : qsTr("Devamını oku")
                trailingIcon: aboutRoot.vm.descriptionExpanded ? Icons.chevronUp : Icons.chevronDown
                onClicked: aboutRoot.vm.descriptionExpanded = !aboutRoot.vm.descriptionExpanded
            }
        }
    }
}
