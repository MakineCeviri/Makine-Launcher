// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * MotionIcon — an Icon that moves once when asked (play()), the way what it
 * shows would move: an arrow goes where it points, refresh turns, download
 * drops, a gear turns a tooth, a bell or a folder wiggles, a heart or a check
 * pops. Buttons play it under the pointer, rows when they are hovered — the
 * sidebar's NavIcon is the same idea drawn part by part.
 *
 * Nothing moves while animations are off (Dimensions.animSlow is 0).
 */
Icon {
    id: mi

    function play() {
        if (Dimensions.animSlow === 0 || mi.glyph === "") return
        const m = mi._motion
        const a = m === "spin" ? spin : m === "spinBack" ? spinBack : m === "tooth" ? tooth
                : m === "drop" ? drop : m === "lift" ? lift : m === "diag" ? diag
                : m === "right" ? right : m === "left" ? left : m === "wiggle" ? wiggle
                : m === "tilt" ? tilt : pop
        if (!a.running) a.restart()
    }

    readonly property string _motion: {
        const g = mi.glyph
        const any = (list) => list.indexOf(g) >= 0
        if (any([Icons.refreshCw, Icons.rotateCw, Icons.loaderCircle])) return "spin"
        if (any([Icons.rotateCcw, Icons.history, Icons.undo2])) return "spinBack"
        if (any([Icons.settings, Icons.cog])) return "tooth"
        if (any([Icons.download, Icons.arrowDownToLine, Icons.hardDriveDownload, Icons.cloudDownload,
                 Icons.circleArrowDown, Icons.chevronDown, Icons.chevronsDown, Icons.monitorDown,
                 Icons.arrowUpDown, Icons.arrowDownAZ, Icons.arrowDownNarrowWide])) return "drop"
        if (any([Icons.circleArrowUp, Icons.chevronUp, Icons.clockArrowUp, Icons.rocket])) return "lift"
        if (any([Icons.arrowUpRight, Icons.externalLink])) return "diag"
        if (any([Icons.arrowRight, Icons.chevronRight, Icons.send, Icons.moveRight, Icons.logOut,
                 Icons.play, Icons.circlePlay])) return "right"
        if (any([Icons.arrowLeft, Icons.chevronLeft])) return "left"
        if (any([Icons.search, Icons.scanSearch, Icons.folderSearch, Icons.eye])) return "tilt"
        if (any([Icons.layers, Icons.layoutGrid, Icons.list, Icons.monitor, Icons.circleCheck])) return "pop"
        if (any([Icons.bell, Icons.trash2, Icons.folder, Icons.folderOpen, Icons.folderInput, Icons.folderCheck,
                 Icons.lock, Icons.gamepad2, Icons.wrench, Icons.discord, Icons.youtube, Icons.globe,
                 Icons.mail, Icons.link, Icons.puzzle, Icons.palette, Icons.ban, Icons.x])) return "wiggle"
        return "pop"
    }

    transformOrigin: Item.Center
    transform: [
        Rotation { id: turn; origin.x: mi.width / 2; origin.y: mi.height / 2; angle: 0 },
        Translate { id: shift; x: 0; y: 0 }
    ]

    readonly property real _step: Math.max(2, mi.size * 0.16)

    SequentialAnimation {
        id: spin
        NumberAnimation { target: turn; property: "angle"; from: 0; to: 360; duration: 640; easing.type: Easing.OutCubic }
        PropertyAction { target: turn; property: "angle"; value: 0 }
    }
    SequentialAnimation {
        id: spinBack
        NumberAnimation { target: turn; property: "angle"; from: 0; to: -360; duration: 640; easing.type: Easing.OutCubic }
        PropertyAction { target: turn; property: "angle"; value: 0 }
    }
    SequentialAnimation {
        id: tooth
        NumberAnimation { target: turn; property: "angle"; from: 0; to: 60; duration: 620; easing.type: Easing.OutBack }
        PropertyAction { target: turn; property: "angle"; value: 0 }
    }
    SequentialAnimation {
        id: drop
        NumberAnimation { target: shift; property: "y"; to: mi._step; duration: 130; easing.type: Easing.OutQuad }
        NumberAnimation { target: shift; property: "y"; to: 0; duration: 420; easing.type: Easing.OutBounce }
    }
    SequentialAnimation {
        id: lift
        NumberAnimation { target: shift; property: "y"; to: -mi._step; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: shift; property: "y"; to: 0; duration: 380; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: diag
        ParallelAnimation {
            NumberAnimation { target: shift; property: "x"; to: mi._step; duration: 140; easing.type: Easing.OutQuad }
            NumberAnimation { target: shift; property: "y"; to: -mi._step; duration: 140; easing.type: Easing.OutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: shift; property: "x"; to: 0; duration: 380; easing.type: Easing.OutBack }
            NumberAnimation { target: shift; property: "y"; to: 0; duration: 380; easing.type: Easing.OutBack }
        }
    }
    SequentialAnimation {
        id: right
        NumberAnimation { target: shift; property: "x"; to: mi._step; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: shift; property: "x"; to: 0; duration: 380; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: left
        NumberAnimation { target: shift; property: "x"; to: -mi._step; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: shift; property: "x"; to: 0; duration: 380; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: wiggle
        NumberAnimation { target: turn; property: "angle"; to: -12; duration: 110; easing.type: Easing.OutQuad }
        NumberAnimation { target: turn; property: "angle"; to: 9; duration: 150; easing.type: Easing.InOutQuad }
        NumberAnimation { target: turn; property: "angle"; to: -5; duration: 130; easing.type: Easing.InOutQuad }
        NumberAnimation { target: turn; property: "angle"; to: 0; duration: 200; easing.type: Easing.OutQuad }
    }
    SequentialAnimation {
        id: tilt
        NumberAnimation { target: turn; property: "angle"; to: -16; duration: 160; easing.type: Easing.OutQuad }
        NumberAnimation { target: turn; property: "angle"; to: 0; duration: 420; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: pop
        NumberAnimation { target: mi; property: "scale"; to: 1.25; duration: 130; easing.type: Easing.OutQuad }
        NumberAnimation { target: mi; property: "scale"; to: 1; duration: 360; easing.type: Easing.OutBack }
    }
}
