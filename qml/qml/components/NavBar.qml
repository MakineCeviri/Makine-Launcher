import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import MakineAI 1.0

/**
 * NavBar.qml - Flutter home_screen.dart _buildTopNavBar birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/screens/home_screen.dart
 *
 * Features:
 * - 72px height with glassmorphism blur
 * - Logo button with hover animation
 * - AI Toggle with animated gradient text
 * - NavBarItem components with underline indicator
 * - Responsive layout
 * - Donate button
 */
Rectangle {
    id: root

    property int currentIndex: 0
    property bool isAIActive: false
    property bool isDark: true
    property bool animationsEnabled: true  // GPU optimization - set from parent window

    signal navItemClicked(int index)
    signal aiToggleClicked()
    signal donateClicked()
    signal websiteClicked()
    signal settingsClicked()

    implicitHeight: Dimensions.navbarHeight  // 72
    color: "transparent"

    // Animated gradient phase for bottom glow
    property real glowPhase: 0.0
    property int glowColorIndex: 0

    Timer {
        interval: 600
        repeat: true
        running: root.animationsEnabled
        onTriggered: glowColorIndex = (glowColorIndex + 1) % Theme.brandGradient.length
    }

    NumberAnimation on glowPhase {
        from: 0.0
        to: 1.0
        duration: 600
        loops: Animation.Infinite
        running: root.animationsEnabled
    }

    // Current animated glow color
    property color currentGlowColor: Theme.brandGradient[glowColorIndex]
    property color nextGlowColor: Theme.brandGradient[(glowColorIndex + 1) % Theme.brandGradient.length]
    property color animatedGlowColor: Qt.rgba(
        currentGlowColor.r * (1 - glowPhase) + nextGlowColor.r * glowPhase,
        currentGlowColor.g * (1 - glowPhase) + nextGlowColor.g * glowPhase,
        currentGlowColor.b * (1 - glowPhase) + nextGlowColor.b * glowPhase,
        1.0
    )

    // Glassmorphism background - Enhanced frosted glass effect
    Rectangle {
        id: blurBackground
        anchors.fill: parent
        color: "transparent"

        // Base frosted layer
        Rectangle {
            anchors.fill: parent
            color: isDark
                ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.88)
                : Qt.rgba(Theme.lightSurface.r, Theme.lightSurface.g, Theme.lightSurface.b, 0.92)
        }

        // Subtle top highlight (glass reflection)
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.3)
        }

        // Gradient overlay for depth
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, isDark ? 0.04 : 0.1) }
                GradientStop { position: 0.3; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, isDark ? 0.06 : 0.02) }
            }
        }

        // Bottom border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.1)
        }

        // Animated gradient glow line at bottom
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            height: 2
            opacity: isDark ? 0.25 : 0.15
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: root.animatedGlowColor }
                GradientStop { position: 0.5; color: root.nextGlowColor }
                GradientStop { position: 1.0; color: root.animatedGlowColor }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginXL  // 24
        anchors.rightMargin: Dimensions.marginXL
        spacing: Dimensions.marginMD  // 16

        // Logo Home Button - Flutter: MakineLogoSimple(size: 38)
        LogoHomeButton {
            id: logoButton
            isSelected: root.currentIndex === 0
            isDark: root.isDark
            onClicked: root.navItemClicked(0)
        }

        // AI Toggle Button - Flutter: _buildMiniAIToggle
        AIToggleButton {
            id: aiToggle
            isActive: root.isAIActive
            isDark: root.isDark
            onClicked: root.aiToggleClicked()
        }

        // Projects NavBarItem - Flutter: Icons.campaign_rounded
        NavBarItem {
            icon: "\uD83D\uDCE2"  // Megaphone emoji
            label: "Projelerimiz"
            isSelected: root.currentIndex === 1
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.navItemClicked(1)
        }

        // Web NavBarItem - Flutter: Icons.language_rounded
        NavBarItem {
            visible: root.width > 600
            icon: "\uD83C\uDF10"  // Globe emoji
            label: "Web"
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.websiteClicked()
        }

        // Settings NavBarItem - Flutter: Icons.settings_rounded
        NavBarItem {
            icon: "\u2699"  // Gear emoji
            label: "Ayarlar"
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.settingsClicked()
        }

        Item { Layout.fillWidth: true }

        // Donate Button - Flutter: DonateButton
        DonateButton {
            visible: root.width > 900
            isDark: root.isDark
            onClicked: root.donateClicked()
        }
    }

    // =========================================================================
    // LogoHomeButton Component - Flutter: _LogoHomeButton
    // Circular black hover effect like Flutter version
    // =========================================================================
    component LogoHomeButton: Item {
        property bool isSelected: false
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: 50
        Layout.preferredHeight: 50

        // Logo size (Flutter: size parameter, default 38)
        readonly property real logoSize: Dimensions.navbarIconSizeLogo  // 38
        readonly property real cornerRadius: logoSize * 0.25  // Flutter: size * 0.25

        // Hover colors - Flutter: black.withOpacity(0.85) for dark mode
        readonly property color hoverColor: isDark
            ? Qt.rgba(0, 0, 0, 0.85)
            : Qt.rgba(0, 0, 0, 0.12)

        readonly property color selectedColor: isDark
            ? Qt.rgba(0, 0, 0, 0.5)
            : Qt.rgba(0, 0, 0, 0.06)

        // Circular hover background - Flutter: BoxDecoration(shape: BoxShape.circle)
        Rectangle {
            id: hoverBackground
            anchors.centerIn: parent
            width: 50
            height: 50
            radius: 25  // Fully circular
            color: logoMouse.containsMouse
                ? hoverColor
                : (isSelected ? selectedColor : "transparent")

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }

        Item {
            id: logoContainer
            anchors.centerIn: parent
            width: logoSize
            height: logoSize
            scale: logoMouse.containsMouse ? 1.05 : 1.0

            Behavior on scale {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }

            // Logo container with rounded corners - Flutter: ClipRRect
            Rectangle {
                id: logoClip
                anchors.fill: parent
                radius: cornerRadius
                color: "transparent"
                clip: true

                // Logo image - use same path as SplashScreen
                Image {
                    id: logoImage
                    anchors.fill: parent
                    source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                    mipmap: true
                    asynchronous: false  // Ensure sync loading
                    cache: true
                }

                // Fallback gradient if logo fails
                Rectangle {
                    anchors.fill: parent
                    radius: cornerRadius
                    visible: logoImage.status !== Image.Ready
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#E8C547" }
                        GradientStop { position: 0.5; color: "#E8A090" }
                        GradientStop { position: 1.0; color: "#90D090" }
                    }

                    // Center M letter as ultimate fallback
                    Text {
                        anchors.centerIn: parent
                        text: "M"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: "white"
                    }
                }
            }
        }

        MouseArea {
            id: logoMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }

        ToolTip {
            visible: logoMouse.containsMouse
            text: "Ana Menü"
            delay: 500
        }
    }

    // =========================================================================
    // AIToggleButton Component - Flutter: _AIToggleButton
    // Brand gradient colors for text animation
    // =========================================================================
    component AIToggleButton: Item {
        property bool isActive: false
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: aiToggleContent.width
        Layout.preferredHeight: 50

        // Brand gradient colors (from Theme)
        readonly property var brandColors: Theme.brandGradient

        // Animated gradient phase
        property real gradientPhase: 0.0
        property int colorIndex: 0

        // Color cycle timer
        Timer {
            interval: 500
            repeat: true
            running: root.animationsEnabled
            onTriggered: colorIndex = (colorIndex + 1) % brandColors.length
        }

        NumberAnimation on gradientPhase {
            from: 0.0
            to: 1.0
            duration: 500
            loops: Animation.Infinite
            running: root.animationsEnabled
        }

        // Animated colors from brand palette
        property color currentColor: brandColors[colorIndex]
        property color nextColor: brandColors[(colorIndex + 1) % brandColors.length]

        property color color1: Qt.rgba(
            currentColor.r * (1 - gradientPhase) + nextColor.r * gradientPhase,
            currentColor.g * (1 - gradientPhase) + nextColor.g * gradientPhase,
            currentColor.b * (1 - gradientPhase) + nextColor.b * gradientPhase,
            1
        )
        property color color2: Qt.rgba(
            nextColor.r * (1 - gradientPhase) + currentColor.r * gradientPhase,
            nextColor.g * (1 - gradientPhase) + currentColor.g * gradientPhase,
            nextColor.b * (1 - gradientPhase) + currentColor.b * gradientPhase,
            1
        )

        Column {
            id: aiToggleContent
            anchors.centerIn: parent
            spacing: 0

            Item {
                width: toggleText.width + 24
                height: 36

                // Gradient text - Flutter: ShaderMask gradient
                Text {
                    id: toggleText
                    anchors.centerIn: parent
                    text: isActive ? "Kapat" : "Turkce Yama"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold

                    // Gradient text using layer
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: parent.parent.parent.parent.color1
                    }
                }
            }

            // Underline - Flutter: AnimatedContainer
            Rectangle {
                id: aiUnderline
                anchors.horizontalCenter: parent.horizontalCenter
                height: 2
                width: aiToggleMouse.containsMouse ? 70 : 0
                radius: 1

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: parent.parent.color1 }
                    GradientStop { position: 1.0; color: parent.parent.color2 }
                }

                Behavior on width {
                    NumberAnimation { duration: Dimensions.hoverDuration }
                }
            }
        }

        MouseArea {
            id: aiToggleMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    // =========================================================================
    // NavBarItem Component - Flutter: NavBarItem
    // =========================================================================
    component NavBarItem: Item {
        property string icon: ""
        property string label: ""
        property bool isSelected: false
        property bool isDark: true
        property bool showLabel: true

        signal clicked()

        Layout.preferredWidth: navItemRow.width + 24
        Layout.preferredHeight: 50

        Column {
            id: navItemColumn
            anchors.centerIn: parent
            spacing: 0

            Item {
                width: navItemRow.width + 24
                height: 36

                Row {
                    id: navItemRow
                    anchors.centerIn: parent
                    spacing: showLabel ? 8 : 0

                    // Icon
                    Text {
                        text: icon
                        font.pixelSize: Dimensions.navbarIconSize  // 18
                        color: {
                            if (isSelected) return Theme.primary
                            if (navItemMouse.containsMouse) {
                                return isDark ? Theme.textPrimary : Theme.lightTextPrimary
                            }
                            return isDark ? Theme.textMuted : Theme.lightTextMuted
                        }

                        Behavior on color {
                            ColorAnimation { duration: Dimensions.hoverDuration }
                        }
                    }

                    // Label
                    Text {
                        visible: showLabel
                        text: label
                        font.pixelSize: 13
                        font.weight: isSelected ? Font.DemiBold : Font.Medium
                        color: {
                            if (isSelected) return Theme.primary
                            if (navItemMouse.containsMouse) {
                                return isDark ? Theme.textPrimary : Theme.lightTextPrimary
                            }
                            return isDark ? Theme.textSecondary : Theme.lightTextSecondary
                        }

                        Behavior on color {
                            ColorAnimation { duration: Dimensions.hoverDuration }
                        }
                    }
                }
            }

            // Underline indicator - Flutter: AnimatedContainer
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                height: 2
                width: isSelected ? 24 : (navItemMouse.containsMouse ? 16 : 0)
                radius: 1
                color: isSelected
                    ? Theme.primary
                    : (isDark ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(0, 0, 0, 0.3))

                Behavior on width {
                    NumberAnimation { duration: Dimensions.hoverDuration }
                }
                Behavior on color {
                    ColorAnimation { duration: Dimensions.hoverDuration }
                }
            }
        }

        MouseArea {
            id: navItemMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    // =========================================================================
    // DonateButton Component - Flutter: DonateButton
    // =========================================================================
    component DonateButton: Rectangle {
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: donateRow.width + 24
        Layout.preferredHeight: 36
        radius: Dimensions.radiusMD  // 8
        color: donateMouse.containsMouse
            ? (isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08))
            : (isDark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04))

        border.color: isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: Dimensions.hoverDuration }
        }

        Row {
            id: donateRow
            anchors.centerIn: parent
            spacing: 6

            // Heart icon
            Text {
                text: "\u2764"  // Heart
                font.pixelSize: 14
                color: "#FF6B6B"
            }

            Text {
                text: "Destek Ol"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
            }
        }

        MouseArea {
            id: donateMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
