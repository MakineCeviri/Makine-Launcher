// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SegmentedControl — Apple's segmented control: a recessed track, the chosen
 * segment raised on a knob that slides to the new choice.
 *
 *   model: [{ value: "all", text: "Tümü", count: 238 }, …]   (count optional)
 *   currentValue / activated(value)
 */
Item {
    id: seg

    property var model: []
    property string currentValue: ""
    property bool small: false

    signal activated(string value)

    readonly property int _h: seg.small ? 26 : 30
    readonly property int _index: {
        for (let i = 0; i < seg.model.length; ++i)
            if (seg.model[i].value === seg.currentValue) return i
        return -1
    }

    implicitHeight: seg._h
    implicitWidth: row.implicitWidth + 4

    Accessible.role: Accessible.PageTabList

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.fillTertiary
    }

    // The raised knob behind the chosen segment
    Rectangle {
        id: knob
        readonly property Item _target: seg._index >= 0 ? repeater.itemAt(seg._index) : null
        visible: _target !== null
        x: _target ? _target.x + row.x : 0
        y: 2
        width: _target ? _target.width : 0
        height: seg._h - 4
        radius: height / 2
        color: Theme.segmentSelected
        border.width: Theme.darkMode ? 0 : 0.5
        border.color: Qt.rgba(0, 0, 0, 0.06)
        Behavior on x { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: 2
        anchors.verticalCenter: parent.verticalCenter

        Repeater {
            id: repeater
            model: seg.model

            Item {
                id: segment
                required property var modelData
                required property int index
                readonly property bool selected: segment.index === seg._index

                width: label.implicitWidth + (countLabel.visible ? countLabel.implicitWidth + 6 : 0)
                       + (seg.small ? 20 : 26)
                height: seg._h - 4

                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: segment.modelData.text
                Accessible.checked: segment.selected
                Keys.onReturnPressed: seg.activated(segment.modelData.value)
                Keys.onSpacePressed: seg.activated(segment.modelData.value)

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: !segment.selected && segHover.hovered ? Theme.fillQuaternary : "transparent"
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: height / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.withAlpha(Theme.primary, 0.5)
                    visible: segment.activeFocus
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: segment.modelData.text
                        font.pixelSize: seg.small ? Dimensions.footnote : Dimensions.callout
                        font.weight: segment.selected ? Font.DemiBold : Font.Medium
                        color: segment.selected ? Theme.textPrimary : Theme.textSecondary
                    }
                    Text {
                        id: countLabel
                        anchors.verticalCenter: parent.verticalCenter
                        visible: segment.modelData.count !== undefined && segment.modelData.count >= 0
                        textFormat: Text.PlainText
                        text: segment.modelData.count !== undefined ? segment.modelData.count : ""
                        font.pixelSize: seg.small ? Dimensions.caption1 : Dimensions.footnote
                        font.weight: Font.Medium
                        font.features: { "tnum": 1 }
                        color: Theme.textMuted
                    }
                }

                HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                MouseArea {
                    anchors.fill: parent
                    onClicked: seg.activated(segment.modelData.value)
                }
            }
        }
    }
}
