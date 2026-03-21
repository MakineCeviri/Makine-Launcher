import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0

/**
 * LoginScreen.qml — Apple-quality auth gate with frosted glass aesthetic
 */
Item {
    id: root

    // Window drag area (full screen since TitleBar is hidden)
    DragHandler {
        target: null
        onActiveChanged: if (active) root.Window.window?.startSystemMove()
    }

    // Close button (top-right)
    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 32; height: 32
        z: 10
        background: Rectangle {
            radius: 16
            color: parent.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
        }
        contentItem: Text {
            text: "\u2715"
            color: Theme.textSecondary
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: Qt.quit()
    }

    // Background with subtle gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgPrimary }
            GradientStop { position: 0.5; color: Theme.bgSecondary }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }
    }

    // Subtle ambient glow behind logo
    Rectangle {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -80
        width: 280; height: 280
        radius: 140
        color: "transparent"
        opacity: 0.15
        gradient: RadialGradient {
            centerX: 140; centerY: 140
            centerRadius: 140
            focalX: centerX; focalY: centerY
            GradientStop { position: 0.0; color: Theme.accent }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Main content
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(360, parent.width - 80)

        // Logo with entrance animation
        Image {
            id: logo
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 20
            source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
            sourceSize: Qt.size(72, 72)
            fillMode: Image.PreserveAspectFit
            opacity: 0
            scale: 0.8

            NumberAnimation on opacity { to: 1; duration: 600; easing.type: Easing.OutCubic }
            NumberAnimation on scale { to: 1; duration: 600; easing.type: Easing.OutBack }
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 6
            text: "Makine \u00C7eviri"
            font.pixelSize: 26
            font.weight: Font.DemiBold
            font.letterSpacing: -0.5
            color: Theme.textPrimary
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 500; easing.type: Easing.OutCubic }
        }

        // Subtitle
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 36
            text: qsTr("T\u00FCrk\u00E7e oyun \u00E7eviri platformu")
            font.pixelSize: 13
            font.weight: Font.Normal
            font.letterSpacing: 0.2
            color: Theme.textMuted
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 500; easing.type: Easing.OutCubic }
        }

        // Card container
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: cardContent.implicitHeight + 48
            radius: 16
            color: Qt.rgba(Theme.bgSecondary.r, Theme.bgSecondary.g, Theme.bgSecondary.b, 0.6)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.06)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 400; easing.type: Easing.OutCubic }

            ColumnLayout {
                id: cardContent
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                // Card title
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Devam etmek i\u00E7in giri\u015F yap\u0131n")
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }

                // Login button
                Button {
                    id: loginBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    enabled: AuthService.state === AuthService.Unauthenticated
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 10
                        color: {
                            if (!loginBtn.enabled) return Theme.bgTertiary
                            if (loginBtn.pressed) return Theme.accentDark
                            if (loginBtn.hovered) return Theme.accentHover
                            return Theme.accent
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    contentItem: Row {
                        spacing: 8
                        anchors.centerIn: parent
                        Text {
                            text: "\uD83C\uDF10"
                            font.pixelSize: 15
                            anchors.verticalCenter: parent.verticalCenter
                            visible: AuthService.state === AuthService.Unauthenticated
                        }
                        Text {
                            text: AuthService.state === AuthService.WaitingForBrowser
                                  ? qsTr("Taray\u0131c\u0131dan yan\u0131t bekleniyor...")
                                  : AuthService.state === AuthService.Exchanging
                                    ? qsTr("Do\u011Frulan\u0131yor...")
                                    : qsTr("Taray\u0131c\u0131 ile Giri\u015F Yap")
                            color: Theme.textOnColor
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            font.letterSpacing: 0.1
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    onClicked: AuthService.startLogin()
                }

                // Spinner
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    running: AuthService.state === AuthService.WaitingForBrowser
                             || AuthService.state === AuthService.Exchanging
                             || AuthService.state === AuthService.Checking
                    visible: running
                }

                // Error
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: errorCol.implicitHeight + 20
                    visible: errorText.text.length > 0
                    radius: 8
                    color: Theme.errorBg
                    border.width: 1
                    border.color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.2)

                    ColumnLayout {
                        id: errorCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Text {
                            id: errorText
                            Layout.fillWidth: true
                            color: Theme.error
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Tekrar Dene")
                            color: Theme.error
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
            Layout.topMargin: 24
            Layout.bottomMargin: 16
            Layout.preferredWidth: 40
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.08)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600 }
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Hesab\u0131n\u0131z yok mu? <a href='https://makineceviri.org/hesap' style='color:%1;text-decoration:none'>Kay\u0131t olun</a>").arg(Theme.accent)
            color: Theme.textMuted
            font.pixelSize: 12
            font.letterSpacing: 0.1
            textFormat: Text.RichText
            onLinkActivated: link => Qt.openUrlExternally(link)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 500 }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        // Version
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            text: "v0.1.0-alpha"
            font.pixelSize: 10
            color: Qt.rgba(Theme.textMuted.r, Theme.textMuted.g, Theme.textMuted.b, 0.5)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 800 }
        }
    }

    // Error signal
    Connections {
        target: AuthService
        function onLoginError(message) { errorText.text = message }
    }
}
