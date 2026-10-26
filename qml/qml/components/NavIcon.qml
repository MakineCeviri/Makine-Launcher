// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * NavIcon — a sidebar icon that comes alive under the pointer, the way
 * Resend's dashboard icons do: its parts (Lucide's strokes, drawn here as
 * shapes on Lucide's 24-unit canvas) hop, squash, turn or blink for well
 * under a second, then rest exactly as the static icon. An icon without a
 * drawing of its own (a brand mark from the symbols font) hops as one glyph.
 *
 *   NavIcon { name: "house"; glyph: Icons.house; size: 17; color: Theme.primaryText }
 *   …  navIcon.play()
 *
 * Names: house · grid · gamepad · help · sparkles · settings · users · heart.
 * Nothing moves while animations are off (Dimensions.animSlow is 0).
 */
Item {
    id: icon

    property string name: ""
    // The font glyph, for an icon without a drawing
    property string glyph: ""
    property real size: 17
    property color color: Theme.textPrimary

    function play() {
        if (Dimensions.animSlow === 0) return
        const motion = drawing.item ? drawing.item.motion : glyphMotion
        if (!motion.running) motion.restart()
    }

    implicitWidth: icon.size
    implicitHeight: icon.size
    Accessible.ignored: true

    Loader {
        id: drawing
        width: 24
        height: 24
        scale: icon.size / 24
        transformOrigin: Item.TopLeft
        sourceComponent: icon.name === "house" ? house
                       : icon.name === "grid" ? grid
                       : icon.name === "gamepad" ? gamepad
                       : icon.name === "help" ? help
                       : icon.name === "sparkles" ? sparkles
                       : icon.name === "settings" ? settings
                       : icon.name === "users" ? users
                       : icon.name === "heart" ? heart
                       : null
    }

    // ── Without a drawing: the glyph hops and tilts ───────────────────────
    Icon {
        id: glyphIcon
        visible: drawing.sourceComponent === null
        anchors.centerIn: parent
        glyph: icon.glyph
        size: icon.size
        color: icon.color
        transform: [
            Rotation { id: glyphTurn; origin.x: glyphIcon.width / 2; origin.y: glyphIcon.height; angle: 0 },
            Translate { id: glyphLift; y: 0 }
        ]
    }
    ParallelAnimation {
        id: glyphMotion
        SequentialAnimation {
            NumberAnimation { target: glyphLift; property: "y"; to: -icon.size * 0.12; duration: 170; easing.type: Easing.OutQuad }
            NumberAnimation { target: glyphLift; property: "y"; to: 0; duration: 420; easing.type: Easing.OutBounce }
        }
        SequentialAnimation {
            NumberAnimation { target: glyphTurn; property: "angle"; to: -10; duration: 130; easing.type: Easing.OutQuad }
            NumberAnimation { target: glyphTurn; property: "angle"; to: 8; duration: 170; easing.type: Easing.InOutQuad }
            NumberAnimation { target: glyphTurn; property: "angle"; to: -4; duration: 140; easing.type: Easing.InOutQuad }
            NumberAnimation { target: glyphTurn; property: "angle"; to: 0; duration: 200; easing.type: Easing.OutQuad }
        }
    }

    // ── Parts ─────────────────────────────────────────────────────────────

    // One stroke group of a Lucide icon, movable around its own pivot
    component Part: Shape {
        id: part
        property string d: ""
        property real px: 12
        property real py: 12
        property real sx: 1
        property real sy: 1
        property real rot: 0
        property real dx: 0
        property real dy: 0
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer
        transform: [
            Scale { origin.x: part.px; origin.y: part.py; xScale: part.sx; yScale: part.sy },
            Rotation { origin.x: part.px; origin.y: part.py; angle: part.rot },
            Translate { x: part.dx; y: part.dy }
        ]
        ShapePath {
            strokeColor: icon.color
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: part.d }
        }
    }

    // Parts that move together (turned as one around px, py)
    component Group: Item {
        id: group
        property real px: 12
        property real py: 12
        property real rot: 0
        width: 24
        height: 24
        transform: Rotation { origin.x: group.px; origin.y: group.py; angle: group.rot }
    }

    // Up, down, a squash on landing and a springy return
    component Hop: SequentialAnimation {
        id: hop
        property Item part: null
        property real lift: 2
        property real squash: 0.9
        property int delay: 0
        PauseAnimation { duration: hop.delay }
        ParallelAnimation {
            NumberAnimation { target: hop.part; property: "dy"; to: -hop.lift; duration: 170; easing.type: Easing.OutQuad }
            NumberAnimation { target: hop.part; property: "sy"; to: 1.05; duration: 170; easing.type: Easing.OutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: hop.part; property: "dy"; to: 0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: hop.part; property: "sy"; to: 1; duration: 150; easing.type: Easing.InQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: hop.part; property: "sy"; to: hop.squash; duration: 80; easing.type: Easing.OutQuad }
            NumberAnimation { target: hop.part; property: "sx"; to: 1 + (1 - hop.squash) * 0.6; duration: 80; easing.type: Easing.OutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: hop.part; property: "sy"; to: 1; duration: 300; easing.type: Easing.OutBack }
            NumberAnimation { target: hop.part; property: "sx"; to: 1; duration: 300; easing.type: Easing.OutBack }
        }
    }

    // Shrinks away and springs back
    component Pop: SequentialAnimation {
        id: pop
        property Item part: null
        property real to: 0.55
        property int delay: 0
        PauseAnimation { duration: pop.delay }
        ParallelAnimation {
            NumberAnimation { target: pop.part; property: "sx"; to: pop.to; duration: 130; easing.type: Easing.InOutQuad }
            NumberAnimation { target: pop.part; property: "sy"; to: pop.to; duration: 130; easing.type: Easing.InOutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: pop.part; property: "sx"; to: 1; duration: 380; easing.type: Easing.OutBack }
            NumberAnimation { target: pop.part; property: "sy"; to: 1; duration: 380; easing.type: Easing.OutBack }
        }
    }

    // A quick shake that settles: -a, +a·0.75, -a·0.4, 0
    component Wiggle: SequentialAnimation {
        id: wiggle
        property Item part: null
        property real angle: 12
        property int delay: 0
        PauseAnimation { duration: wiggle.delay }
        NumberAnimation { target: wiggle.part; property: "rot"; to: -wiggle.angle; duration: 120; easing.type: Easing.OutQuad }
        NumberAnimation { target: wiggle.part; property: "rot"; to: wiggle.angle * 0.75; duration: 160; easing.type: Easing.InOutQuad }
        NumberAnimation { target: wiggle.part; property: "rot"; to: -wiggle.angle * 0.4; duration: 140; easing.type: Easing.InOutQuad }
        NumberAnimation { target: wiggle.part; property: "rot"; to: 0; duration: 220; easing.type: Easing.OutQuad }
    }

    // ── The icons ─────────────────────────────────────────────────────────

    // Ana Sayfa: the house hops, its door a beat behind
    Component {
        id: house
        Item {
            property Animation motion: ParallelAnimation {
                Hop { part: houseBody; lift: 2; squash: 0.9 }
                Hop { part: houseDoor; lift: 2.4; squash: 0.72; delay: 60 }
            }
            Part {
                id: houseBody
                py: 21
                d: "M3 10a2 2 0 0 1 .709-1.528l7-6a2 2 0 0 1 2.582 0l7 6A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
            }
            Part {
                id: houseDoor
                py: 21
                d: "M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8"
            }
        }
    }

    // Tüm Oyunlar: the four tiles pop in turn, clockwise
    Component {
        id: grid
        Item {
            property Animation motion: ParallelAnimation {
                Pop { part: tile1 }
                Pop { part: tile2; delay: 70 }
                Pop { part: tile3; delay: 140 }
                Pop { part: tile4; delay: 210 }
            }
            Part { id: tile1; px: 6.5; py: 6.5; d: "M4 3h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z" }
            Part { id: tile2; px: 17.5; py: 6.5; d: "M15 3h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z" }
            Part { id: tile3; px: 17.5; py: 17.5; d: "M15 14h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1v-5a1 1 0 0 1 1-1z" }
            Part { id: tile4; px: 6.5; py: 17.5; d: "M4 14h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1v-5a1 1 0 0 1 1-1z" }
        }
    }

    // Kütüphanem: the pad rocks in the hands, its buttons blink
    Component {
        id: gamepad
        Item {
            property Animation motion: ParallelAnimation {
                Wiggle { part: pad; angle: 11 }
                Pop { part: buttonA; to: 0.1; delay: 140 }
                Pop { part: buttonB; to: 0.1; delay: 230 }
            }
            Group {
                id: pad
                Part {
                    d: "M17.32 5H6.68a4 4 0 0 0-3.978 3.59c-.006.052-.01.101-.017.152C2.604 9.416 2 14.456 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.414-1.414A2 2 0 0 1 9.828 16h4.344a2 2 0 0 1 1.414.586L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.545-.604-6.584-.685-7.258-.007-.05-.011-.1-.017-.151A4 4 0 0 0 17.32 5z"
                }
                Part { d: "M6 11h4M8 9v4" }
                Part { id: buttonA; px: 15; py: 12; d: "M15 12h.01" }
                Part { id: buttonB; px: 18; py: 10; d: "M18 10h.01" }
            }
        }
    }

    // Nasıl çalışır?: the question mark shakes its head, the dot drops in
    Component {
        id: help
        Item {
            property Animation motion: ParallelAnimation {
                Wiggle { part: hook; angle: 16 }
                SequentialAnimation {
                    PauseAnimation { duration: 90 }
                    NumberAnimation { target: helpDot; property: "dy"; to: -3; duration: 150; easing.type: Easing.OutQuad }
                    NumberAnimation { target: helpDot; property: "dy"; to: 0; duration: 420; easing.type: Easing.OutBounce }
                }
            }
            Part { d: "M2 12a10 10 0 1 0 20 0a10 10 0 1 0-20 0" }
            Part { id: hook; px: 12; py: 13; d: "M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3" }
            Part { id: helpDot; d: "M12 17h.01" }
        }
    }

    // Yenilikler: the big star dips and spins a quarter turn (it looks the
    // same after one), the small ones twinkle after it
    Component {
        id: sparkles
        Item {
            property Animation motion: ParallelAnimation {
                Pop { part: star; to: 0.7 }
                SequentialAnimation {
                    NumberAnimation { target: star; property: "rot"; from: 0; to: 90; duration: 640; easing.type: Easing.OutBack }
                    PropertyAction { target: star; property: "rot"; value: 0 }
                }
                Pop { part: plus; to: 0; delay: 130 }
                Pop { part: spark; to: 0; delay: 230 }
            }
            Part {
                id: star
                d: "M11.017 2.814a1 1 0 0 1 1.966 0l1.051 5.558a2 2 0 0 0 1.594 1.594l5.558 1.051a1 1 0 0 1 0 1.966l-5.558 1.051a2 2 0 0 0-1.594 1.594l-1.051 5.558a1 1 0 0 1-1.966 0l-1.051-5.558a2 2 0 0 0-1.594-1.594l-5.558-1.051a1 1 0 0 1 0-1.966l5.558-1.051a2 2 0 0 0 1.594-1.594z"
            }
            Part { id: plus; px: 20; py: 4; d: "M20 2v4M22 4h-4" }
            Part { id: spark; px: 4; py: 20; d: "M2 20a2 2 0 1 0 4 0a2 2 0 1 0-4 0" }
        }
    }

    // Ayarlar: the gear turns one tooth on (six teeth: the same gear again)
    Component {
        id: settings
        Item {
            property Animation motion: SequentialAnimation {
                NumberAnimation { target: gear; property: "rot"; from: 0; to: 60; duration: 720; easing.type: Easing.OutBack }
                PropertyAction { target: gear; property: "rot"; value: 0 }
            }
            Part {
                id: gear
                d: "M9.671 4.136a2.34 2.34 0 0 1 4.659 0 2.34 2.34 0 0 0 3.319 1.915 2.34 2.34 0 0 1 2.33 4.033 2.34 2.34 0 0 0 0 3.831 2.34 2.34 0 0 1-2.33 4.033 2.34 2.34 0 0 0-3.319 1.915 2.34 2.34 0 0 1-4.659 0 2.34 2.34 0 0 0-3.32-1.915 2.34 2.34 0 0 1-2.33-4.033 2.34 2.34 0 0 0 0-3.831A2.34 2.34 0 0 1 6.35 6.051a2.34 2.34 0 0 0 3.319-1.915"
            }
            Part { d: "M9 12a3 3 0 1 0 6 0a3 3 0 1 0-6 0" }
        }
    }

    // Topluluk: the person in front hops, the one behind answers
    Component {
        id: users
        Item {
            property Animation motion: ParallelAnimation {
                Hop { part: frontHead; lift: 2.4; squash: 0.86 }
                Hop { part: frontBody; lift: 1.4; squash: 0.9; delay: 40 }
                Hop { part: back; lift: 2; squash: 0.9; delay: 150 }
            }
            Part { id: back; px: 19; py: 21; d: "M16 3.128a4 4 0 0 1 0 7.744M22 21v-2a4 4 0 0 0-3-3.87" }
            Part { id: frontBody; px: 9; py: 21; d: "M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2" }
            Part { id: frontHead; px: 9; py: 11; d: "M5 7a4 4 0 1 0 8 0a4 4 0 1 0-8 0" }
        }
    }

    // Destek ol: two heartbeats
    Component {
        id: heart
        Item {
            property Animation motion: SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { target: heartPart; property: "sx"; to: 1.2; duration: 120; easing.type: Easing.OutQuad }
                    NumberAnimation { target: heartPart; property: "sy"; to: 1.2; duration: 120; easing.type: Easing.OutQuad }
                }
                ParallelAnimation {
                    NumberAnimation { target: heartPart; property: "sx"; to: 0.94; duration: 120; easing.type: Easing.InQuad }
                    NumberAnimation { target: heartPart; property: "sy"; to: 0.94; duration: 120; easing.type: Easing.InQuad }
                }
                ParallelAnimation {
                    NumberAnimation { target: heartPart; property: "sx"; to: 1.13; duration: 110; easing.type: Easing.OutQuad }
                    NumberAnimation { target: heartPart; property: "sy"; to: 1.13; duration: 110; easing.type: Easing.OutQuad }
                }
                ParallelAnimation {
                    NumberAnimation { target: heartPart; property: "sx"; to: 1; duration: 300; easing.type: Easing.OutBack }
                    NumberAnimation { target: heartPart; property: "sy"; to: 1; duration: 300; easing.type: Easing.OutBack }
                }
            }
            Part {
                id: heartPart
                py: 13
                d: "M2 9.5a5.5 5.5 0 0 1 9.591-3.676.56.56 0 0 0 .818 0A5.49 5.49 0 0 1 22 9.5c0 2.29-1.5 4-3 5.5l-5.492 5.313a2 2 0 0 1-3 .019L5 15c-1.5-1.5-3-3.2-3-5.5"
            }
        }
    }
}
