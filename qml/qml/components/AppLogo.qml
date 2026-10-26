// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AppLogo — Makine Çeviri's mark, the same as the Discord server's icon: a
 * rounded app icon. Still by default; under the pointer (or `playing`) its
 * colours flow round the maze as they do on Discord. Once it has moved it
 * stays on the frame it reached, so stopping never jumps.
 *
 *   AppLogo { size: 30 }                  // sidebar: plays on hover
 *   AppLogo { size: 72; playing: true }   // About: plays while shown
 */
Item {
    id: logo

    property real size: 30
    property bool playing: false

    readonly property bool _play: (logo.playing || hover.hovered) && logo.visible
                                  && Dimensions.animSlow > 0
                                  && Qt.application.state === Qt.ApplicationActive
    // Set the first time it is asked to move: loads the animation then
    property bool _started: false
    onPlayingChanged: if (playing) _started = true
    Component.onCompleted: if (playing) _started = true

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        visible: !moving.visible
        source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
        sourceSize: Qt.size(logo.size * 3, logo.size * 3)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
    }

    // Loaded the first time it is asked to move. Without the WebP plugin it
    // never becomes Ready and the still mark stays.
    AnimatedImage {
        id: moving
        anchors.fill: parent
        visible: logo._started && status === AnimatedImage.Ready && frameCount > 1
        source: logo._started ? "qrc:/qt/qml/MakineLauncher/resources/images/logo_anim.webp" : ""
        playing: logo._play
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
    }

    HoverHandler {
        id: hover
        onHoveredChanged: if (hovered) logo._started = true
    }
}
