// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * InstallNotesDialog.qml - Post-install instructions the patch does not work without
 *
 * 61 of the 237 catalogue packages carry installNotes, and for most of them the
 * note IS the last install step: Far Cry 6 wants the in-game language set to
 * Turkish, Thief wants it set to English, Mad Max put Turkish in the Polish
 * slot, The Sims 4 needs mods enabled. Until this dialog existed the note was
 * only reachable in the detail page's About card, so the normal outcome was a
 * patch reported as installed over a game still running in its old language.
 *
 * Shown once, right after a successful install — the moment the instruction is
 * actionable. Acknowledge-only: there is nothing here to decline.
 */
BaseDialog {
    id: root

    property string message: ""
    accentColor: Theme.success

    width: 460
    contentHeight: Math.min(contentColumn.implicitHeight, 320)
    title: qsTr("Yama kuruldu — son bir adım var")

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.withAlpha(root.accentColor, 0.10)
                border.color: Theme.withAlpha(root.accentColor, 0.20)
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 15
                    text: ""                      // info
                    color: root.accentColor
                }
            }

            Label {
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            DialogCloseButton {
                onClicked: { root.cancelled(); root.close() }
            }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.textPrimary06
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingMD

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        // The note can run to a numbered list (Kenshi, Skyrim, The Sims 4), so
        // it scrolls rather than being elided — an instruction cut in half is
        // the failure this dialog exists to prevent.
        ScrollView {
            id: _noteScroll
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            Layout.preferredHeight: Math.min(_noteLabel.implicitHeight, 240)
            clip: true

            Label {
                id: _noteLabel
                textFormat: Text.PlainText
                // availableWidth, not parent.width: inside a ScrollView the
                // label's parent is the flickable content item, which sizes
                // itself FROM this label — binding to it loops.
                width: _noteScroll.availableWidth
                text: root.message
                font.pixelSize: Dimensions.fontSM
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.5
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 56

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.textPrimary06
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Label {
                textFormat: Text.PlainText
                text: qsTr("Bu not oyun sayfasındaki Yama Notları bölümünde durmaya devam eder.")
                font.pixelSize: Dimensions.fontMicro
                color: Theme.textMuted
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.preferredWidth: _okLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _okMouse.containsMouse ? root.accentColor : Theme.withAlpha(root.accentColor, 0.85)
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                scale: _okMouse.pressed ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Anladım")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.close()

                Label {
                    id: _okLbl
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: qsTr("Anladım")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }

                MouseArea {
                    id: _okMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }
    }
}
