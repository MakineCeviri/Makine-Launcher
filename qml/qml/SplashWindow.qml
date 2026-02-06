import QtQuick
import QtQuick.Window
import QtQuick.Shapes
import MakineAI 1.0

/**
 * SplashWindow.qml - Native Qt SplashScreen birebir port
 * Kaynak: ui/src/widgets/splashscreen.cpp
 *
 * Features:
 * - Frameless splash window (500x400)
 * - 20 particles (gold, pink, white)
 * - 6-layer glow effect with pulse
 * - Logo scale/opacity animation
 * - Gradient text "Makine Ceviri"
 * - AI badge with sparkle
 * - Animated progress bar
 * - Sequential animation: intro -> text -> progress -> fadeout
 * - Total duration: ~2200ms
 */
Window {
    id: splashWindow

    // Native Qt: Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.SplashScreen
    color: "transparent"

    // Native Qt: 500x400 fixed size
    width: 500
    height: 400

    // Center on screen
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2

    signal finished()

    // ===== ANIMATION PROPERTIES =====
    property real logoScale: 0.7
    property real logoOpacity: 0.0
    property real glowIntensity: 0.0
    property real glowPulse: 0.0
    property real textOpacity: 0.0
    property real progress: 0.0
    property real fadeOut: 0.0
    property real particlePhase: 0.0

    // ===== PARTICLE SYSTEM =====
    ListModel {
        id: particleModel
    }

    // Initialize particles
    function initParticles() {
        particleModel.clear()
        for (var i = 0; i < 20; i++) {
            particleModel.append({
                px: Math.random() * 500,
                py: Math.random() * 400,
                vx: (Math.random() - 0.5) * 0.6,  // -0.3 to 0.3
                vy: -(Math.random() * 0.4 + 0.2), // -0.2 to -0.6
                psize: Math.random() * 3 + 2,     // 2-5
                palpha: Math.random() * 0.5 + 0.3, // 0.3-0.8
                ptype: Math.floor(Math.random() * 3) // 0=gold, 1=pink, 2=white
            })
        }
    }

    // Update particle positions
    function updateParticles() {
        for (var i = 0; i < particleModel.count; i++) {
            var p = particleModel.get(i)
            var newX = p.px + p.vx
            var newY = p.py + p.vy

            // Wrap around
            if (newY < -10) {
                newY = 410
                newX = Math.random() * 500
            }
            if (newX < -10) newX = 510
            if (newX > 510) newX = -10

            particleModel.set(i, { px: newX, py: newY })
        }
    }

    // ===== START ANIMATION =====
    function start() {
        initParticles()
        glowPulseAnimation.start()
        particleAnimation.start()
        mainSequence.start()
        show()
    }

    // ===== ANIMATIONS =====

    // Glow pulse - Native Qt: 2000ms infinite
    NumberAnimation {
        id: glowPulseAnimation
        target: splashWindow
        property: "glowPulse"
        from: 0.0
        to: 1.0
        duration: 2000
        loops: Animation.Infinite
    }

    // Particle animation - Native Qt: 3000ms infinite
    NumberAnimation {
        id: particleAnimation
        target: splashWindow
        property: "particlePhase"
        from: 0.0
        to: 1.0
        duration: 3000
        loops: Animation.Infinite
        onRunningChanged: if (running) updateTimer.start()
    }

    Timer {
        id: updateTimer
        interval: 50
        repeat: true
        running: particleAnimation.running
        onTriggered: updateParticles()
    }

    // Main sequence - Native Qt: total ~2630ms
    SequentialAnimation {
        id: mainSequence

        // Intro group (parallel) - Native Qt: 880ms
        ParallelAnimation {
            // Logo scale 0.7 -> 1.0, 880ms, easeOutBack
            NumberAnimation {
                target: splashWindow
                property: "logoScale"
                from: 0.7
                to: 1.0
                duration: 880
                easing.type: Easing.OutBack
            }
            // Logo opacity 0 -> 1, 660ms, easeIn
            NumberAnimation {
                target: splashWindow
                property: "logoOpacity"
                from: 0.0
                to: 1.0
                duration: 660
                easing.type: Easing.InQuad
            }
            // Glow intensity 0 -> 1, 880ms, easeOutCubic
            NumberAnimation {
                target: splashWindow
                property: "glowIntensity"
                from: 0.0
                to: 1.0
                duration: 880
                easing.type: Easing.OutCubic
            }
        }

        // Text opacity 0 -> 1, 440ms, easeIn
        NumberAnimation {
            target: splashWindow
            property: "textOpacity"
            from: 0.0
            to: 1.0
            duration: 440
            easing.type: Easing.InQuad
        }

        // Progress 0 -> 1, 990ms, easeInOut
        NumberAnimation {
            target: splashWindow
            property: "progress"
            from: 0.0
            to: 1.0
            duration: 990
            easing.type: Easing.InOutQuad
        }

        // Pause 100ms
        PauseAnimation { duration: 100 }

        // Fade out 0 -> 1, 220ms, easeOut
        NumberAnimation {
            target: splashWindow
            property: "fadeOut"
            from: 0.0
            to: 1.0
            duration: 220
            easing.type: Easing.OutQuad
        }

        onFinished: {
            glowPulseAnimation.stop()
            particleAnimation.stop()
            updateTimer.stop()
            splashWindow.finished()
            splashWindow.close()
        }
    }

    // ===== MAIN CONTENT =====
    Rectangle {
        id: mainContent
        anchors.fill: parent
        radius: Dimensions.radiusStandard  // Native Qt: rounded rect clip
        color: "#0A0A0F"
        opacity: 1.0 - fadeOut
        clip: true

        // Radial gradient simulation - Native Qt: gold 6%, pink 3%
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -parent.height * 0.3
            width: parent.width * 1.5
            height: parent.width * 1.5
            radius: width / 2
            color: Qt.rgba(1, 0.84, 0, 0.04)  // gold subtle
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -parent.height * 0.2
            width: parent.width
            height: parent.width
            radius: width / 2
            color: Qt.rgba(1, 0.41, 0.71, 0.02)  // pink subtle
        }

        // Border - Native Qt: white 12%
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
        }

        // ===== PARTICLES =====
        Repeater {
            model: particleModel
            delegate: Rectangle {
                x: model.px
                y: model.py
                width: model.psize * 2
                height: model.psize * 2
                radius: model.psize
                color: model.ptype === 0 ? "#FFD700" :  // gold
                       model.ptype === 1 ? "#FF69B4" :  // pink
                       "#FFFFFF"  // white
                opacity: model.palpha * glowIntensity * (1.0 - fadeOut)
            }
        }

        // ===== GLOW LAYERS (6) =====
        Item {
            id: glowContainer
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -30
            width: 240
            height: 240
            visible: glowIntensity > 0.01

            property real pulse: 0.8 + 0.2 * Math.sin(glowPulse * 2 * Math.PI)
            property real intensity: glowIntensity * pulse

            Repeater {
                model: 6
                delegate: Rectangle {
                    anchors.centerIn: parent
                    width: 240 * (1.0 + index * 0.3) * glowContainer.intensity
                    height: width
                    radius: width / 2
                    // Simplified glow - gold/pink blend
                    color: Qt.rgba(
                        1,
                        0.6 + index * 0.04,
                        0.3 + index * 0.07,
                        (15 - index * 2) / 255 * glowContainer.intensity
                    )
                }
            }
        }

        // ===== LOGO =====
        Item {
            id: logoItem
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -30
            width: 100 * logoScale
            height: 100 * logoScale
            opacity: logoOpacity * (1.0 - fadeOut)

            // Logo placeholder (gradient circle with M)
            Rectangle {
                anchors.centerIn: parent
                width: 80 * logoScale
                height: 80 * logoScale
                radius: width / 2

                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#FFD700" }
                    GradientStop { position: 1.0; color: "#FF69B4" }
                }

                Text {
                    anchors.centerIn: parent
                    text: "M"
                    font.pixelSize: 32 * logoScale
                    font.weight: Font.Bold
                    color: "white"
                }
            }
        }

        // ===== TEXT SECTION =====
        Item {
            id: textSection
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: logoItem.bottom
            anchors.topMargin: 20
            width: parent.width
            height: 100
            opacity: textOpacity * (1.0 - fadeOut)

            // Title "Makine Çeviri" - Flutter: fontSize 42, w900, letterSpacing -1.5
            Text {
                id: titleText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                text: "Makine Çeviri"
                font.pixelSize: 42
                font.weight: Font.Black
                font.letterSpacing: -1.5
                color: "#FFD700"  // Gold
            }

            // AI Badge - Native Qt: gradient gold/pink 20%, border gold 30%
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: titleText.bottom
                anchors.topMargin: 12
                width: badgeRow.width + 24
                height: 24
                radius: Dimensions.radiusStandard

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(1, 0.84, 0, 0.2) }
                    GradientStop { position: 1.0; color: Qt.rgba(1, 0.41, 0.71, 0.2) }
                }

                border.color: Qt.rgba(1, 0.84, 0, 0.3)
                border.width: 1

                Row {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 4

                    Text {
                        text: "\u2728"  // Sparkle
                        font.pixelSize: 12
                        color: "#FFD700"
                    }

                    Text {
                        text: "AI Destekli"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.3
                        color: "#FFD700"
                    }
                }
            }
        }

        // ===== PROGRESS BAR =====
        Item {
            id: progressSection
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 100
            width: 200
            height: 40
            opacity: textOpacity * (1.0 - fadeOut)
            visible: progress > 0

            // Background track - Native Qt: white 8%
            Rectangle {
                id: progressTrack
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: 200
                height: 4
                radius: Dimensions.radiusStandard
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            // Progress glow
            Rectangle {
                anchors.left: progressTrack.left
                anchors.top: progressTrack.top
                anchors.topMargin: -3
                width: 200 * progress
                height: 10
                radius: Dimensions.radiusStandard
                color: Qt.rgba(1, 0.41, 0.71, 0.4)
                visible: progress > 0.1
            }

            // Progress fill
            Rectangle {
                anchors.left: progressTrack.left
                anchors.top: progressTrack.top
                width: 200 * progress
                height: 4
                radius: Dimensions.radiusStandard

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#FFD700" }
                    GradientStop { position: 0.5; color: "#FF8C00" }
                    GradientStop { position: 1.0; color: "#FF69B4" }
                }
            }

            // Loading text - Flutter: fontSize 11, w500, letterSpacing 1.5, white 35%
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: progressTrack.bottom
                anchors.topMargin: 14
                text: "Yükleniyor..."
                font.pixelSize: 11
                font.weight: Font.Medium
                font.letterSpacing: 1.5
                color: Qt.rgba(1, 1, 1, 0.35)
            }
        }

        // ===== BOTTOM INFO =====
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 36
            width: 200
            height: 50
            opacity: textOpacity * (1.0 - fadeOut)

            // Version badge - Native Qt: white 4% bg, borderRadius 6
            Rectangle {
                id: versionBadge
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: versionText.width + 20
                height: 18
                radius: Dimensions.radiusStandard
                color: Qt.rgba(1, 1, 1, 0.04)

                Text {
                    id: versionText
                    anchors.centerIn: parent
                    text: "v0.1.0-alpha"
                    font.pixelSize: 8
                    font.weight: Font.Medium
                    color: Qt.rgba(1, 1, 1, 0.45)
                }
            }

            // Community text - Flutter: fontSize 10, white 30%, letterSpacing 0.3
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                text: "Makine Çeviri Topluluğu"
                font.pixelSize: 10
                font.letterSpacing: 0.3
                color: Qt.rgba(1, 1, 1, 0.3)
            }
        }
    }

    // Start on completion
    Component.onCompleted: {
        start()
    }
}
