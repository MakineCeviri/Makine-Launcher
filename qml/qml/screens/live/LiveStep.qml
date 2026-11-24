// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * LiveStep — one numbered step of the Makine Live page: a round number (a tick
 * once done), the step's name, one plain sentence, and the step's content below.
 */
ColumnLayout {
    id: step

    property int number: 0
    property string title: ""
    property string hint: ""
    property bool done: false
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 4
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 14
            color: step.done ? Theme.successBg : Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.18 : 0.12)
            Text {
                visible: !step.done
                anchors.centerIn: parent
                text: String(step.number)
                textFormat: Text.PlainText
                font.pixelSize: Dimensions.callout
                font.weight: Font.Bold
                color: Theme.primaryText
            }
            Icon {
                visible: step.done
                anchors.centerIn: parent
                glyph: Icons.check
                size: 14
                color: Theme.success
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: step.title
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.family: Dimensions.fontDisplay
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Text {
                visible: step.hint !== ""
                Layout.fillWidth: true
                text: step.hint
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
        }
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: 16
    }
}
