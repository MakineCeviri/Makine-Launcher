// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ToggleSetting.qml - A settings row with a switch: what it is, what it
 * does, and the switch. For new pages prefer ListSection + ListRow.
 */
Item {
    id: _toggleRoot
    property string title: ""
    property string description: ""
    property bool checked: false
    property bool disableAnimations: false
    signal toggled()

    activeFocusOnTab: true
    Keys.onReturnPressed: toggled()
    Keys.onSpacePressed: toggled()
    Accessible.role: Accessible.CheckBox
    Accessible.name: title
    Accessible.description: description
    Accessible.checked: checked
    Accessible.onToggleAction: toggled()
    Layout.fillWidth: true
    Layout.preferredHeight: Math.max(54, _col.implicitHeight + 22)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        spacing: 12
        ColumnLayout {
            id: _col
            Layout.fillWidth: true
            spacing: 2
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true; text: _toggleRoot.title
                font.pixelSize: Dimensions.body; font.weight: Font.Medium
                color: Theme.textPrimary; elide: Text.ElideRight
            }
            Label {
                textFormat: Text.PlainText
                visible: _toggleRoot.description !== ""
                Layout.fillWidth: true; text: _toggleRoot.description
                font.pixelSize: Dimensions.footnote; color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }
        AppSwitch {
            checked: _toggleRoot.checked
            animate: !_toggleRoot.disableAnimations
            accessibleName: _toggleRoot.title
            onToggled: _toggleRoot.toggled()
        }
    }
}
