import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0

/**
 * LoginScreen.qml — Vice City neon auth gate
 */
Item {
    id: root

    // Block all mouse events from passing through to content behind
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    // Window drag area — stops before window buttons
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 140
        anchors.top: parent.top
        height: 40
        z: 5
        DragHandler {
            target: null
            onActiveChanged: if (active) root.Window.window?.startSystemMove()
        }
    }

    // Window controls — tray, minimize, close
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        z: 10

        // Tray
        Rectangle {
            width: 46; height: 32
            color: trayMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
            Text {
                anchors.centerIn: parent
                text: "\uE70D"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 11
                color: Qt.rgba(1, 1, 1, trayMa.containsMouse ? 0.7 : 0.35)
            }
            MouseArea {
                id: trayMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: root.Window.window?.hide()
            }
        }

        // Minimize
        Rectangle {
            width: 46; height: 32
            color: minMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
            Text {
                anchors.centerIn: parent
                text: "\uE921"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 11
                color: Qt.rgba(1, 1, 1, minMa.containsMouse ? 0.7 : 0.35)
            }
            MouseArea {
                id: minMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: root.Window.window?.showMinimized()
            }
        }

        // Close
        Rectangle {
            width: 46; height: 32
            color: closeMa.containsMouse ? "#E81123" : "transparent"
            Text {
                anchors.centerIn: parent
                text: "\uE8BB"
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 11
                color: closeMa.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.35)
            }
            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: root.Window.window?.close()
            }
        }
    }

    // Full-window dimensions (bypass contentItem layout issues)
    readonly property real winW: Window.window ? Window.window.width : width
    readonly property real winH: Window.window ? Window.window.height : height

    // === BACKGROUND — extended wave-bg with rich color palette ===
    Rectangle {
        x: 0; y: 0
        width: root.winW
        height: root.winH
        color: "#0d1117"
        clip: true

        Rectangle {
            width: root.winW * 6
            height: root.winH

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.000; color: "#0a1628" }
                GradientStop { position: 0.080; color: "#0e1a30" }
                GradientStop { position: 0.160; color: "#150f2a" }
                GradientStop { position: 0.240; color: "#1a0f2e" }
                GradientStop { position: 0.320; color: "#1e0e30" }
                GradientStop { position: 0.400; color: "#170d2a" }
                GradientStop { position: 0.480; color: "#0f2a2e" }
                GradientStop { position: 0.560; color: "#0a2428" }
                GradientStop { position: 0.640; color: "#0d1820" }
                GradientStop { position: 0.720; color: "#0d1117" }
                GradientStop { position: 0.800; color: "#10131c" }
                GradientStop { position: 0.880; color: "#0c1522" }
                GradientStop { position: 1.000; color: "#0a1628" }
            }

            SequentialAnimation on x {
                loops: Animation.Infinite
                NumberAnimation { to: -root.winW * 5; duration: 25000; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0; duration: 25000; easing.type: Easing.InOutSine }
            }
        }
    }

    // === CONTENT — explicit center using window dimensions ===
    ColumnLayout {
        width: Math.min(Math.max(320, root.winW * 0.28), 400)
        x: (root.winW - width) / 2
        y: (root.winH - height) / 2
        spacing: 0

        // Logo with dual neon glow
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            Layout.bottomMargin: 28

            Image {
                anchors.fill: parent
                source: "qrc:/qt/qml/MakineLauncher/resources/images/logo_white.png"
                sourceSize: Qt.size(72, 72)
                fillMode: Image.PreserveAspectFit
            }

            opacity: 0
            scale: 0.85
            NumberAnimation on opacity { to: 1; duration: 700; easing.type: Easing.OutCubic }
            NumberAnimation on scale { to: 1; duration: 700; easing.type: Easing.OutBack }
        }

        // MAKİNE ÇEVİRİ — split brand title (web style)
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: titleRow.implicitWidth
            Layout.preferredHeight: titleRow.implicitHeight
            Layout.bottomMargin: 6

            // Glow layer
            Row {
                anchors.centerIn: parent
                spacing: 0
                Text {
                    text: "MAK\u0130NE "
                    font.pixelSize: 22
                    font.family: "Inter"
                    font.weight: Font.Bold
                    font.letterSpacing: 6
                    color: "#FFFFFF"
                    opacity: 0.25
                    layer.enabled: true
                    layer.smooth: true
                }
                Text {
                    text: "\u00C7EV\u0130R\u0130"
                    font.pixelSize: 22
                    font.family: "Inter"
                    font.weight: Font.Normal
                    font.letterSpacing: 6
                    color: "#FFFFFF"
                    opacity: 0.15
                    layer.enabled: true
                    layer.smooth: true
                }
            }

            // Main text
            Row {
                id: titleRow
                anchors.centerIn: parent
                spacing: 0
                Text {
                    text: "MAK\u0130NE "
                    font.pixelSize: 22
                    font.family: "Inter"
                    font.weight: Font.Bold
                    font.letterSpacing: 6
                    color: "#FFFFFF"
                }
                Text {
                    text: "\u00C7EV\u0130R\u0130"
                    font.pixelSize: 22
                    font.family: "Inter"
                    font.weight: Font.Normal
                    font.letterSpacing: 6
                    color: Qt.rgba(1, 1, 1, 0.5)
                }
            }

            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600; easing.type: Easing.OutCubic }
        }

        // Tagline
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 40
            text: qsTr("K\u00E2r Amac\u0131 G\u00FCtmeyen T\u00FCrk\u00E7e Oyun Yerelle\u015Ftirme Platformu")
            font.pixelSize: 11
            font.family: "Inter"
            font.weight: Font.Normal
            font.letterSpacing: 3
            color: Qt.rgba(1, 1, 1, 0.25)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600 }
        }

        // === CARD ===
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: cardContent.implicitHeight + 56
            radius: 14
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 0.42, 0.62, 0.12)

            ColumnLayout {
                id: cardContent
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Devam etmek i\u00E7in giri\u015F yap\u0131n")
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Qt.rgba(1, 1, 1, 0.45)
                }

                // Login button — neon pink
                Button {
                    id: loginBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    enabled: AuthService.state === AuthServiceType.Unauthenticated
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 10
                        color: {
                            if (!loginBtn.enabled) return Qt.rgba(0.9, 0.4, 0.6, 0.2)
                            if (loginBtn.pressed) return "#B03070"
                            if (loginBtn.hovered) return "#E04898"
                            return "#D63D8C"
                        }
                    }
                    contentItem: Text {
                        text: AuthService.state === AuthServiceType.WaitingForBrowser
                              ? qsTr("Taray\u0131c\u0131dan yan\u0131t bekleniyor...")
                              : AuthService.state === AuthServiceType.Exchanging
                                ? qsTr("Do\u011Frulan\u0131yor...")
                                : qsTr("Giri\u015F Yap")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.3
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        console.log("[LoginScreen] Login button clicked, state:", AuthService.state)
                        AuthService.startLogin()
                    }
                }

                // Spinner
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    running: AuthService.state === AuthServiceType.WaitingForBrowser
                             || AuthService.state === AuthServiceType.Exchanging
                             || AuthService.state === AuthServiceType.Checking
                    visible: running
                    palette.dark: "#22D3EE"
                }

                // Error
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: errorCol.implicitHeight + 20
                    visible: errorText.text.length > 0
                    radius: 8
                    color: Qt.rgba(0.94, 0.27, 0.27, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(0.94, 0.27, 0.27, 0.15)

                    ColumnLayout {
                        id: errorCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Text {
                            id: errorText
                            Layout.fillWidth: true
                            color: "#F87171"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Tekrar Dene")
                            color: "#F87171"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: retryMa.containsMouse ? 1.0 : 0.7

                            MouseArea {
                                id: retryMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    errorText.text = ""
                                    AuthService.retryLogin()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Divider
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 28
            Layout.bottomMargin: 18
            Layout.preferredWidth: 32
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 0.42, 0.62, 0.15)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 700 }
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Hesab\u0131n\u0131z yok mu?  ") + "<a href='https://makineceviri.org/hesap' style='color:#22D3EE;text-decoration:none'>" + qsTr("Kay\u0131t olun") + "</a>"
            color: Qt.rgba(1, 1, 1, 0.30)
            font.pixelSize: 12
            textFormat: Text.RichText
            onLinkActivated: link => Qt.openUrlExternally(link)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600 }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        // Version
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            text: "v0.1.0-alpha"
            font.pixelSize: 10
            font.letterSpacing: 0.5
            color: Qt.rgba(1, 1, 1, 0.12)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 800 }
        }
    } // ColumnLayout

    Connections {
        target: AuthService
        function onLoginError(message) { errorText.text = message }
        function onStateChanged() {
            console.log("[LoginScreen] AuthService state:", AuthService.state,
                        "isAuthenticated:", AuthService.isAuthenticated)
        }
    }

    Component.onCompleted: {
        console.log("[LoginScreen] Loaded. AuthService.state:", AuthService.state,
                    "enabled button:", AuthService.state === AuthServiceType.Unauthenticated)
    }
}
