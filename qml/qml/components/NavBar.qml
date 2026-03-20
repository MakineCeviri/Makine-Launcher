import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * NavBar - Minimal navigation bar: logo, page links, discord, settings.
 */
Item {
    id: navBarRoot

    property int currentIndex: 0
    property bool animationsEnabled: true
    property bool showBottomLine: false

    signal homeClicked()
    signal libraryClicked()
    signal settingsClicked()

    readonly property color _bgColor: Theme.bgPrimary90

    // Main background
    Rectangle {
        anchors.fill: parent
        color: navBarRoot._bgColor
    }

    // Bottom-left outward curve
    Canvas {
        x: 0; y: parent.height
        width: Dimensions.radiusSection; height: Dimensions.radiusSection
        Connections { target: navBarRoot; function on_BgColorChanged() { Qt.callLater(requestPaint) } }
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = navBarRoot._bgColor.toString()
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(0, height)
            ctx.quadraticCurveTo(0, 0, width, 0)
            ctx.closePath()
            ctx.fill()
        }
    }

    // Bottom-right outward curve
    Canvas {
        x: parent.width - Dimensions.radiusSection; y: parent.height
        width: Dimensions.radiusSection; height: Dimensions.radiusSection
        Connections { target: navBarRoot; function on_BgColorChanged() { Qt.callLater(requestPaint) } }
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = navBarRoot._bgColor.toString()
            ctx.beginPath()
            ctx.moveTo(width, 0)
            ctx.lineTo(width, height)
            ctx.quadraticCurveTo(width, 0, 0, 0)
            ctx.closePath()
            ctx.fill()
        }
    }

    // Bottom separator line — visible only in Settings
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.textPrimary08
        opacity: navBarRoot.showBottomLine ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginLG
        anchors.rightMargin: Dimensions.marginLG
        spacing: Dimensions.spacingXL

        // Logo
        Item {
            Layout.preferredWidth: 44; Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignVCenter
            scale: logoMouse.containsMouse ? 1.05 : 1.0
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            // Radial glow behind logo
            Canvas {
                id: logoGlow
                anchors.centerIn: parent
                width: 72; height: 72
                opacity: logoMouse.containsMouse ? 0.9 : 0.5
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                renderStrategy: Canvas.Cooperative
                property color _base: Theme.accentBase
                property color _dark: Theme.accentDark
                on_BaseChanged: Qt.callLater(requestPaint)
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var cx = width / 2, cy = height / 2
                    var b = _base, d = _dark
                    var br = Math.round(b.r * 255), bg = Math.round(b.g * 255), bb = Math.round(b.b * 255)
                    var dr = Math.round(d.r * 255), dg = Math.round(d.g * 255), db = Math.round(d.b * 255)
                    var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, width / 2)
                    grad.addColorStop(0.0, "rgba(" + br + "," + bg + "," + bb + ",0.35)")
                    grad.addColorStop(0.4, "rgba(" + br + "," + bg + "," + bb + ",0.15)")
                    grad.addColorStop(0.7, "rgba(" + dr + "," + dg + "," + db + ",0.06)")
                    grad.addColorStop(1.0, "rgba(" + dr + "," + dg + "," + db + ",0.0)")
                    ctx.fillStyle = grad
                    ctx.beginPath()
                    ctx.arc(cx, cy, width / 2, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: Dimensions.navbarIconSizeLogo; height: Dimensions.navbarIconSizeLogo
                radius: Dimensions.navbarIconSizeLogo * 0.25
                color: "transparent"

                Image {
                    anchors.fill: parent
                    source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }
            }

            MouseArea {
                id: logoMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.homeClicked()
            }
        }

        NavItem {
            text: qsTr("Kütüphanem")
            selected: navBarRoot.currentIndex === 1
            onClicked: navBarRoot.libraryClicked()
        }

        Item { Layout.fillWidth: true }

        // Update status badge — premium dot + label, all driven by C++
        Rectangle {
            id: updateBadge
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: _badgeRow.width + 16
            Layout.preferredHeight: 26
            radius: 13
            color: UpdateService.indicatorVisible
                   ? (_statusMouse.containsMouse ? Theme.primary15 : Theme.primary06)
                   : "transparent"
            border.width: UpdateService.indicatorVisible ? 1 : 0
            border.color: Theme.primary20

            Behavior on color { ColorAnimation { duration: 150 } }

            // Subtle pulse for Available state
            property real _pulse: 1.0
            opacity: UpdateService.state === UpdateService.Available ? _pulse : 1.0
            SequentialAnimation on _pulse {
                running: UpdateService.state === UpdateService.Available
                loops: Animation.Infinite
                NumberAnimation { to: 0.65; duration: 1200; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
            }

            Row {
                id: _badgeRow
                anchors.centerIn: parent
                spacing: 6

                // Status dot
                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !UpdateService.busy
                    color: UpdateService.indicatorVisible ? Theme.primary : Theme.success
                }

                // Spinning indicator for busy states
                BusyIndicator {
                    width: 14; height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    running: UpdateService.busy
                    visible: running
                    palette.dark: Theme.primary
                }

                // Label — directly from C++
                Text {
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                    text: UpdateService.navLabel
                    font.pixelSize: 11
                    font.weight: UpdateService.indicatorVisible ? Font.DemiBold : Font.Medium
                    color: UpdateService.indicatorVisible ? Theme.primary : Theme.textMuted
                }

                // Action icon for actionable states
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 13
                    visible: UpdateService.navIcon !== ""
                    text: UpdateService.navIcon
                    color: UpdateService.state === UpdateService.Ready ? Theme.success : Theme.primary
                }

                // Mini progress ring
                Canvas {
                    id: progressRing
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16; height: 16
                    renderStrategy: Canvas.Cooperative
                    visible: UpdateService.state === UpdateService.Downloading
                             || UpdateService.state === UpdateService.Verifying
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var cx = width / 2, cy = height / 2, r = 6
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, Math.PI * 2)
                        ctx.strokeStyle = Theme.primary20
                        ctx.lineWidth = 2
                        ctx.stroke()
                        if (UpdateService.progress > 0) {
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, -Math.PI / 2,
                                    -Math.PI / 2 + UpdateService.progress * Math.PI * 2)
                            ctx.strokeStyle = Theme.primary
                            ctx.lineWidth = 2
                            ctx.stroke()
                        }
                    }
                    Connections {
                        target: UpdateService
                        function onProgressChanged() { Qt.callLater(progressRing.requestPaint) }
                    }
                }
            }

            StyledToolTip {
                visible: _statusMouse.containsMouse && UpdateService.indicatorVisible
                delay: 400
                text: UpdateService.statusText
            }

            MouseArea {
                id: _statusMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: UpdateService.actionable ? Qt.PointingHandCursor : Qt.ArrowCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (!UpdateService.actionable) return
                    if (mouse.button === Qt.RightButton) {
                        if (UpdateService.state === UpdateService.Available)
                            UpdateService.dismiss()
                        return
                    }
                    switch (UpdateService.state) {
                    case UpdateService.Available:
                        UpdateService.download(); break
                    case UpdateService.Downloading:
                    case UpdateService.Verifying:
                        UpdateService.cancel(); break
                    case UpdateService.Ready:
                        UpdateService.install(); break
                    }
                }
            }
        }

        // Settings
        Item {
            id: settingsItem
            Layout.preferredWidth: 36; Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter

            property bool hovered: settingsMouse.containsMouse
            property bool isSelected: navBarRoot.currentIndex === 2
            scale: hovered ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            rotation: hovered ? 30 : 0
            Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\uE713"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 17
                color: settingsItem.isSelected ? Theme.primary
                     : settingsItem.hovered    ? Theme.textPrimary
                     : Theme.textMuted
                opacity: settingsItem.isSelected ? 1.0
                       : settingsItem.hovered    ? 0.9
                       : 0.6
            }

            MouseArea {
                id: settingsMouse; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: navBarRoot.settingsClicked()
            }
        }
    }
}
