// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Window
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * A plugin's small question over the game (overlay.prompt), in the top-left
 * corner of the screen the plugin points at (`near`, else the primary one):
 * "the subtitle area is not locked yet, lock it?". Takes clicks but never the
 * focus (WindowDoesNotAcceptFocus: the game stays in front and keeps its
 * keyboard), so an action may also name the global key the plugin listens to.
 * Kept out of screen capture like the subtitle. The plugin hides it
 * (overlay.promptHide); a click answers and closes it (answerPrompt).
 */
Window {
    id: promptWindow
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.ToolTip | Qt.WindowDoesNotAcceptFocus
    color: "transparent"
    visible: false

    // {id, title, text, actions: [{id, label, key}], near}; empty = no prompt
    property var prompt: ({})
    readonly property bool _shown: !!prompt && !!prompt.id
    readonly property bool _animate: Dimensions.animFast > 0

    readonly property var _screen: {
        const near = prompt && prompt.near
        const screens = Qt.application.screens
        if (near && near.width > 0) {
            const cx = near.x + near.width / 2, cy = near.y + near.height / 2
            for (let i = 0; i < screens.length; ++i) {
                const s = screens[i]
                if (cx >= s.virtualX && cx < s.virtualX + s.width && cy >= s.virtualY && cy < s.virtualY + s.height)
                    return s
            }
        }
        return screens[0]
    }
    readonly property int _margin: 24

    x: (_screen ? _screen.virtualX : 0) + _margin
    y: (_screen ? _screen.virtualY : 0) + _margin
    width: card.width
    height: card.height

    on_ShownChanged: {
        if (_shown) {
            hideAnim.stop()
            if (!visible) {
                card.opacity = 0
                visible = true
                if (typeof PluginManager !== "undefined" && PluginManager) PluginManager.keepOutOfCapture(promptWindow, true)
            }
            showAnim.restart()
        } else if (visible) {
            showAnim.stop()
            hideAnim.restart()
        }
    }

    NumberAnimation { id: showAnim; target: card; property: "opacity"; to: 1; duration: promptWindow._animate ? 160 : 0; easing.type: Easing.OutCubic }
    SequentialAnimation {
        id: hideAnim
        NumberAnimation { target: card; property: "opacity"; to: 0; duration: promptWindow._animate ? 200 : 0; easing.type: Easing.OutCubic }
        ScriptAction { script: promptWindow.visible = false }
    }

    // Dark and opaque enough to read over any game, whatever the launcher's theme
    Rectangle {
        id: card
        width: Math.min(380, Math.max(260, body.implicitWidth + 28))
        height: body.implicitHeight + 24
        radius: 10
        color: "#EB15151B"
        border.color: "#33FFFFFF"
        border.width: 1

        Column {
            id: body
            x: 14
            y: 12
            width: card.width - 28
            spacing: 6

            Text {
                width: parent.width
                visible: text !== ""
                text: (promptWindow.prompt && promptWindow.prompt.title) || ""
                color: "#FFFFFF"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
            Text {
                width: parent.width
                text: (promptWindow.prompt && promptWindow.prompt.text) || ""
                color: "#D0D0D6"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
            Row {
                spacing: 8
                topPadding: 4
                Repeater {
                    model: (promptWindow.prompt && promptWindow.prompt.actions) || []
                    Rectangle {
                        id: actionButton
                        required property var modelData
                        required property int index
                        readonly property bool _primary: index === 0
                        width: actionLabel.implicitWidth + 22
                        height: 28
                        radius: 6
                        color: _primary ? (actionMouse.containsMouse ? Theme.primaryHover : Theme.primary)
                                        : (actionMouse.containsMouse ? "#33FFFFFF" : "#1FFFFFFF")
                        Text {
                            id: actionLabel
                            anchors.centerIn: parent
                            text: actionButton.modelData.label + (actionButton.modelData.key ? "  (" + actionButton.modelData.key + ")" : "")
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            textFormat: Text.PlainText
                        }
                        MouseArea {
                            id: actionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (PluginManager) PluginManager.answerPrompt(actionButton.modelData.id)
                        }
                    }
                }
            }
        }
    }
}
