import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import MakineAI 1.0

/**
 * NavBar.qml - Top navigation bar with glassmorphism and animated elements
 */
Rectangle {
    id: root

    property int currentIndex: 0
    property bool isAIActive: false
    property bool isDark: true
    property bool animationsEnabled: true

    signal navItemClicked(int index)
    signal aiToggleClicked()
    signal donateClicked()
    signal websiteClicked()
    signal settingsClicked()

    implicitHeight: Dimensions.navbarHeight
    color: "transparent"

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

    property color currentGlowColor: Theme.brandGradient[glowColorIndex]
    property color nextGlowColor: Theme.brandGradient[(glowColorIndex + 1) % Theme.brandGradient.length]
    property color animatedGlowColor: Qt.rgba(
        currentGlowColor.r * (1 - glowPhase) + nextGlowColor.r * glowPhase,
        currentGlowColor.g * (1 - glowPhase) + nextGlowColor.g * glowPhase,
        currentGlowColor.b * (1 - glowPhase) + nextGlowColor.b * glowPhase,
        1.0
    )

    Rectangle {
        id: blurBackground
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            color: isDark
                ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.88)
                : Qt.rgba(Theme.lightSurface.r, Theme.lightSurface.g, Theme.lightSurface.b, 0.92)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.3)
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, isDark ? 0.04 : 0.1) }
                GradientStop { position: 0.3; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, isDark ? 0.06 : 0.02) }
            }
        }

        // Bottom border removed for seamless container look
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginXL
        anchors.rightMargin: Dimensions.marginXL
        spacing: Dimensions.marginMD

        LogoHomeButton {
            id: logoButton
            isSelected: root.currentIndex === 0
            isDark: root.isDark
            onClicked: root.navItemClicked(0)
        }

        AIToggleButton {
            id: aiToggle
            isActive: root.isAIActive
            isDark: root.isDark
            onClicked: root.aiToggleClicked()
        }

        NavBarItem {
            icon: "\uD83D\uDCE2"
            label: "Projelerimiz"
            isSelected: root.currentIndex === 1
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.navItemClicked(1)
        }

        NavBarItem {
            visible: root.width > 600
            icon: "\uD83C\uDF10"
            label: "Web"
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.websiteClicked()
        }

        NavBarItem {
            icon: "\u2699"
            label: "Ayarlar"
            isDark: root.isDark
            showLabel: root.width > 750
            onClicked: root.settingsClicked()
        }

        Item { Layout.fillWidth: true }

        DonateButton {
            visible: root.width > 900
            isDark: root.isDark
            onClicked: root.donateClicked()
        }
    }

    // =========================================================================
    // LogoHomeButton Component
    // Circular black hover effect
    // =========================================================================
    component LogoHomeButton: Item {
        property bool isSelected: false
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: 50
        Layout.preferredHeight: 50

        readonly property real logoSize: Dimensions.navbarIconSizeLogo
        readonly property real cornerRadius: logoSize * 0.25

        readonly property color hoverColor: isDark
            ? Qt.rgba(0, 0, 0, 0.85)
            : Qt.rgba(0, 0, 0, 0.12)

        readonly property color selectedColor: isDark
            ? Qt.rgba(0, 0, 0, 0.5)
            : Qt.rgba(0, 0, 0, 0.06)

        Rectangle {
            id: hoverBackground
            anchors.centerIn: parent
            width: 50
            height: 50
            radius: 25
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

            Rectangle {
                id: logoClip
                anchors.fill: parent
                radius: cornerRadius
                color: "transparent"
                clip: true

                Image {
                    id: logoImage
                    anchors.fill: parent
                    source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                    mipmap: true
                    asynchronous: false
                    cache: true
                }

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
    // AIToggleButton Component
    // Brand gradient colors for text animation
    // =========================================================================
    component AIToggleButton: Item {
        property bool isActive: false
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: aiToggleContent.width
        Layout.preferredHeight: 50

        readonly property var brandColors: Theme.brandGradient

        property real gradientPhase: 0.0
        property int colorIndex: 0

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

                Text {
                    id: toggleText
                    anchors.centerIn: parent
                    text: isActive ? "Kapat" : "Türkçe Yama"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: parent.parent.parent.parent.color1
                    }
                }
            }

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
    // NavBarItem Component
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

                    Text {
                        text: icon
                        font.pixelSize: Dimensions.navbarIconSize
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
    // DonateButton Component
    // =========================================================================
    component DonateButton: Rectangle {
        property bool isDark: true

        signal clicked()

        Layout.preferredWidth: donateRow.width + 24
        Layout.preferredHeight: 36
        radius: Dimensions.radiusMD
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

            Text {
                text: "\u2764"
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
