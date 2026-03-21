import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0

/**
 * LoginScreen.qml — Premium auth gate
 */
Item {
    id: root

    // Window drag
    DragHandler {
        target: null
        onActiveChanged: if (active) root.Window.window?.startSystemMove()
    }

    // Close button
    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 32; height: 32
        z: 10
        background: Rectangle {
            radius: 16
            color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
        }
        contentItem: Text {
            text: "\u2715"
            color: Qt.rgba(1, 1, 1, 0.4)
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: Qt.quit()
    }

    // === BACKGROUND ===
    // Deep dark base
    Rectangle {
        anchors.fill: parent
        color: "#050508"
    }

    // Top-left accent blob
    Rectangle {
        x: -120; y: -80
        width: 400; height: 400
        radius: 200
        color: Theme.accent
        opacity: 0.04
    }

    // Bottom-right accent blob
    Rectangle {
        x: parent.width - 200; y: parent.height - 180
        width: 350; height: 350
        radius: 175
        color: "#8B5CF6"
        opacity: 0.03
    }

    // Subtle noise texture via grid
    Grid {
        anchors.fill: parent
        columns: Math.ceil(parent.width / 3)
        opacity: 0.015
        Repeater {
            model: Math.ceil(root.width / 3) * Math.ceil(root.height / 3)
            Rectangle {
                width: 3; height: 3
                color: Math.random() > 0.5 ? "#ffffff" : "transparent"
            }
        }
    }

    // === CONTENT ===
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(380, parent.width - 60)

        // White logo
        Image {
            id: logoImg
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 28
            source: "qrc:/qt/qml/MakineLauncher/resources/images/logo_white.png"
            sourceSize: Qt.size(96, 96)
            fillMode: Image.PreserveAspectFit
            opacity: 0
            scale: 0.85

            NumberAnimation on opacity { to: 1; duration: 700; easing.type: Easing.OutCubic }
            NumberAnimation on scale { to: 1; duration: 700; easing.type: Easing.OutBack; overshoot: 1.2 }
        }

        // Glow behind logo
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 160
            Layout.preferredHeight: 160
            Layout.topMargin: -160 - 28
            Layout.bottomMargin: -160 + 28
            radius: 80
            color: Theme.accent
            opacity: 0.07
            z: -1
        }

        // MAKİNE ÇEVİRİ title with glow
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: titleText.implicitWidth
            Layout.preferredHeight: titleText.implicitHeight
            Layout.bottomMargin: 6

            // Glow layer
            Text {
                anchors.centerIn: parent
                text: "MAK\u0130NE \u00C7EV\u0130R\u0130"
                font.pixelSize: 28
                font.weight: Font.Black
                font.letterSpacing: 3
                color: Theme.accent
                opacity: 0.4
                layer.enabled: true
                layer.smooth: true
            }

            // Main text
            Text {
                id: titleText
                anchors.centerIn: parent
                text: "MAK\u0130NE \u00C7EV\u0130R\u0130"
                font.pixelSize: 28
                font.weight: Font.Black
                font.letterSpacing: 3
                color: "#FFFFFF"
            }

            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600; easing.type: Easing.OutCubic }
        }

        // Tagline
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 40
            text: qsTr("T\u00FCrk\u00E7e Oyun \u00C7eviri Platformu")
            font.pixelSize: 12
            font.weight: Font.Normal
            font.letterSpacing: 1.5
            color: Qt.rgba(1, 1, 1, 0.35)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600 }
        }

        // === CARD ===
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: cardContent.implicitHeight + 56
            radius: 14
            color: Qt.rgba(1, 1, 1, 0.04)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 500 }

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
                    color: Qt.rgba(1, 1, 1, 0.5)
                }

                // Login button
                Button {
                    id: loginBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    enabled: AuthService.state === AuthService.Unauthenticated
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 10
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: loginBtn.pressed ? "#0E7490" : loginBtn.hovered ? "#0891B2" : Theme.accent }
                            GradientStop { position: 1.0; color: loginBtn.pressed ? "#0369A1" : loginBtn.hovered ? "#0284C7" : "#0EA5E9" }
                        }
                        opacity: loginBtn.enabled ? 1.0 : 0.3
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }
                    contentItem: Text {
                        text: AuthService.state === AuthService.WaitingForBrowser
                              ? qsTr("Taray\u0131c\u0131dan yan\u0131t bekleniyor...")
                              : AuthService.state === AuthService.Exchanging
                                ? qsTr("Do\u011Frulan\u0131yor...")
                                : qsTr("Giri\u015F Yap")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.3
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: AuthService.startLogin()
                }

                // Spinner
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    running: AuthService.state === AuthService.WaitingForBrowser
                             || AuthService.state === AuthService.Exchanging
                             || AuthService.state === AuthService.Checking
                    visible: running
                    palette.dark: Theme.accent
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
            color: Qt.rgba(1, 1, 1, 0.08)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 700 }
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Hesab\u0131n\u0131z yok mu?  ") + "<a href='https://makineceviri.org/hesap' style='color:" + Theme.accent + ";text-decoration:none'>" + qsTr("Kay\u0131t olun") + "</a>"
            color: Qt.rgba(1, 1, 1, 0.35)
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
            color: Qt.rgba(1, 1, 1, 0.15)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 800 }
        }
    }

    Connections {
        target: AuthService
        function onLoginError(message) { errorText.text = message }
    }
}
