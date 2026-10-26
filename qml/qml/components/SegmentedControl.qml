// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SegmentedControl — Apple's segmented control: a recessed track, hairlines
 * between the segments, the chosen one on a knob of the accent (lit from
 * above, white label) that slides to the new choice — as macOS draws the
 * selected segment in the front window.
 *
 *   model: [{ value: "all", text: "Tümü", count: 238 }, …]   (count optional)
 *          [{ value: "grid", icon: Icons.layoutGrid, tip: "…" }, …] (icon-only)
 *   currentValue / activated(value)
 */
Item {
    id: seg

    property var model: []
    property string currentValue: ""
    property bool small: false
    // Narrow window: counts go, segments tighten
    property bool compact: false

    signal activated(string value)

    readonly property int _h: seg.small ? 26 : 30
    property int _made: 0
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
        border.width: 1
        border.color: Theme.separator
    }

    RectangularShadow {
        anchors.fill: knob
        visible: knob.visible
        radius: knob.radius
        offset.y: 1
        blur: 3
        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.45 : 0.18)
    }
    // The raised knob behind the chosen segment
    Rectangle {
        id: knob
        // itemAt() notifies nothing: _made re-reads it as the segments appear
        readonly property Item _target: seg._made >= 0 && seg._index >= 0 ? repeater.itemAt(seg._index) : null
        visible: _target !== null
        x: _target ? _target.x + row.x : 0
        y: 2
        width: _target ? _target.width : 0
        height: seg._h - 4
        radius: height / 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.tint(Theme.primary, Qt.rgba(1, 1, 1, 0.14)) }
            GradientStop { position: 1.0; color: Theme.primary }
        }
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.12)
        // Springs to the chosen segment: a little past, then back
        Behavior on x { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutBack; easing.overshoot: 1.2 } }
        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: 2
        anchors.verticalCenter: parent.verticalCenter

        Repeater {
            id: repeater
            model: seg.model
            onItemAdded: seg._made++
            onItemRemoved: seg._made++

            Item {
                id: segment
                required property var modelData
                required property int index
                readonly property bool selected: segment.index === seg._index

                readonly property string _icon: segment.modelData.icon ?? ""
                // Chosen, its icon answers (MotionIcon); pressed, it gives
                onSelectedChanged: if (selected) segIcon.play()
                scale: segTap.pressed ? 0.92 : 1
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
                width: (segment._icon !== "" ? 16 : label.implicitWidth)
                       + (countLabel.visible ? countLabel.implicitWidth + 6 : 0)
                       + (seg.compact ? 16 : seg.small ? 20 : 26)
                height: seg._h - 4

                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: segment.modelData.text ?? segment.modelData.tip ?? ""
                Accessible.checked: segment.selected
                Keys.onReturnPressed: seg.activated(segment.modelData.value)
                Keys.onSpacePressed: seg.activated(segment.modelData.value)

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: !segment.selected && segHover.hovered ? Theme.fillQuaternary : "transparent"
                }
                // Hairline from the segment before; none beside the knob
                Rectangle {
                    visible: segment.index > 0 && !segment.selected && segment.index - 1 !== seg._index
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: Math.round(parent.height * 0.5)
                    color: Theme.separatorStrong
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

                // On whole pixels: a centred half pixel snaps left and up
                Row {
                    x: Math.round((parent.width - width) / 2)
                    y: Math.round((parent.height - height) / 2)
                    spacing: 6

                    MotionIcon {
                        id: segIcon
                        anchors.verticalCenter: parent.verticalCenter
                        visible: segment._icon !== ""
                        glyph: segment._icon
                        size: 16
                        color: segment.selected ? "#FFFFFF"
                             : segHover.hovered ? Theme.textPrimary : Theme.textSecondary
                    }
                    Text {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        visible: segment._icon === ""
                        textFormat: Text.PlainText
                        text: segment.modelData.text ?? ""
                        font.pixelSize: seg.small ? Dimensions.footnote : Dimensions.callout
                        font.weight: segment.selected ? Font.DemiBold : Font.Medium
                        color: segment.selected ? "#FFFFFF"
                             : segHover.hovered ? Theme.textPrimary : Theme.textSecondary
                    }
                    Text {
                        id: countLabel
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !seg.compact && segment.modelData.count !== undefined && segment.modelData.count >= 0
                        textFormat: Text.PlainText
                        text: segment.modelData.count !== undefined ? segment.modelData.count : ""
                        font.pixelSize: seg.small ? Dimensions.caption1 : Dimensions.footnote
                        font.weight: Font.Medium
                        font.features: { "tnum": 1 }
                        color: segment.selected ? Qt.rgba(1, 1, 1, 0.75) : Theme.textMuted
                    }
                }

                HoverHandler {
                    id: segHover
                    cursorShape: Qt.PointingHandCursor
                    onHoveredChanged: if (hovered && !segment.selected) segIcon.play()
                }
                StyledToolTip {
                    visible: segHover.hovered && (segment.modelData.tip ?? "") !== ""
                    text: segment.modelData.tip ?? ""
                }
                MouseArea {
                    id: segTap
                    anchors.fill: parent
                    onClicked: seg.activated(segment.modelData.value)
                }
            }
        }
    }
}
