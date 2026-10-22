// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppButton — the one button of the launcher, in Apple's button styles.
 *
 *   style  "filled"       accent fill, white label — the page's main action
 *          "tinted"       accent wash, accent label — a strong second action
 *          "gray"         neutral fill — everyday actions
 *          "plain"        label only, accent — links and inline actions
 *          "destructive"  red fill · "destructiveTinted" red wash
 *          "glass"        dark glass, white label — on top of cover art
 *          "white"        white fill, black label — the main action on art
 *   size   "small" 28 · "regular" 34 · "large" 44
 *
 * No text → a round icon-only button. `busy` swaps the icon for a spinner
 * and ignores clicks.
 */
Item {
    id: btn

    property string text: ""
    property string icon: ""
    property string trailingIcon: ""
    property string style: "gray"
    property string size: "regular"
    property bool busy: false
    property bool capsule: true
    property string toolTip: ""
    // Stretch the label across the width (sheets, stacked buttons)
    property bool fillWidth: false
    property color tint: Theme.primary

    signal clicked()

    readonly property bool _iconOnly: btn.text === ""
    readonly property int _h: btn.size === "small" ? 28 : btn.size === "large" ? 44 : 34
    readonly property int _font: btn.size === "small" ? Dimensions.footnote
                               : btn.size === "large" ? Dimensions.headline : Dimensions.callout
    readonly property int _iconSize: btn.size === "small" ? 14 : btn.size === "large" ? 18 : 16
    readonly property int _padX: btn.size === "small" ? 12 : btn.size === "large" ? 22 : 16
    readonly property bool _hovered: hover.hovered && btn.enabled && !btn.busy
    readonly property bool _pressed: tap.pressed && btn.enabled && !btn.busy

    readonly property color _labelColor: {
        if (!btn.enabled) return Theme.textDisabled
        switch (btn.style) {
        case "filled": case "destructive": return Theme.textOnColor
        case "glass": return "#FFFFFF"
        case "white": return "#000000"
        case "tinted": case "plain": return btn.tint
        case "destructiveTinted": return Theme.error
        default: return Theme.textPrimary
        }
    }
    readonly property color _fill: {
        const h = btn._hovered
        switch (btn.style) {
        case "filled":
            return !btn.enabled ? Theme.fillTertiary
                 : h ? (Theme.darkMode ? Qt.lighter(btn.tint, 1.1) : Qt.darker(btn.tint, 1.08)) : btn.tint
        case "destructive":
            return !btn.enabled ? Theme.fillTertiary : h ? Qt.lighter(Theme.error, 1.08) : Theme.error
        case "tinted":
            return Theme.withAlpha(btn.tint, h ? 0.24 : 0.16)
        case "destructiveTinted":
            return Theme.withAlpha(Theme.error, h ? 0.22 : 0.14)
        case "glass":
            return h ? Theme.glassOnArtHover : Theme.glassOnArt
        case "white":
            return h ? "#E8E8ED" : "#FFFFFF"
        case "plain":
            return h ? Theme.fillQuaternary : "transparent"
        default:
            return h ? Theme.fillSecondary : Theme.fillTertiary
        }
    }

    implicitHeight: btn._h
    implicitWidth: btn._iconOnly ? btn._h : Math.max(btn._h, content.implicitWidth + btn._padX * 2)
    Layout.preferredHeight: implicitHeight
    opacity: btn._pressed ? 0.72 : 1.0
    Behavior on opacity { NumberAnimation { duration: Dimensions.animVeryFast } }

    activeFocusOnTab: btn.enabled
    Accessible.role: Accessible.Button
    Accessible.name: btn.text !== "" ? btn.text : btn.toolTip
    Accessible.onPressAction: if (btn.enabled && !btn.busy) btn.clicked()
    Keys.onReturnPressed: if (btn.enabled && !btn.busy) btn.clicked()
    Keys.onEnterPressed: if (btn.enabled && !btn.busy) btn.clicked()
    Keys.onSpacePressed: if (btn.enabled && !btn.busy) btn.clicked()

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: btn.capsule ? height / 2 : Dimensions.radiusControl
        color: btn._fill
        border.width: btn.style === "glass" ? 1 : 0
        border.color: Qt.rgba(1, 1, 1, 0.14)
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    }

    // macOS focus ring: accent halo just outside the shape
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: bg.radius + 3
        color: "transparent"
        border.width: 3
        border.color: Theme.withAlpha(Theme.primary, 0.45)
        visible: btn.activeFocus
    }

    RowLayout {
        id: content
        anchors.centerIn: parent
        width: btn.fillWidth ? Math.min(implicitWidth, parent.width - btn._padX * 2) : implicitWidth
        spacing: btn.size === "small" ? 5 : 7

        Item {
            visible: btn.icon !== "" || btn.busy
            Layout.preferredWidth: btn._iconSize
            Layout.preferredHeight: btn._iconSize
            Layout.alignment: Qt.AlignVCenter

            Icon {
                anchors.centerIn: parent
                visible: !btn.busy
                glyph: btn.icon
                size: btn._iconSize
                color: btn._labelColor
            }
            Spinner {
                anchors.centerIn: parent
                visible: btn.busy
                size: btn._iconSize
                color: btn._labelColor
            }
        }

        Text {
            visible: !btn._iconOnly
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: btn.fillWidth
            textFormat: Text.PlainText
            text: btn.text
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: btn._font
            font.weight: btn.style === "plain" || btn.style === "gray" ? Font.Medium : Font.DemiBold
            color: btn._labelColor
        }

        Icon {
            visible: btn.trailingIcon !== "" && !btn._iconOnly
            Layout.alignment: Qt.AlignVCenter
            glyph: btn.trailingIcon
            size: btn._iconSize - 2
            color: btn._labelColor
        }
    }

    HoverHandler {
        id: hover
        cursorShape: btn.enabled && !btn.busy ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    // A MouseArea, not a TapHandler: it takes the press, so a clickable row
    // or card under the button does not act on the same click.
    MouseArea {
        id: tap
        anchors.fill: parent
        enabled: btn.enabled && !btn.busy
        onClicked: btn.clicked()
    }

    StyledToolTip {
        visible: btn.toolTip !== "" && hover.hovered
        text: btn.toolTip
    }
}
