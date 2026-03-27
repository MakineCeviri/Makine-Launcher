import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0

/**
 * WelcomeLoginStep.qml — Merged welcome + login (Step 1 of onboarding)
 *
 * Preserves LoginScreen's premium neon design as a wizard step.
 * Background and window controls are provided by OnboardingWizard parent.
 *
 * Modes:
 *   firstLaunch: "Hoş Geldin" + full tagline
 *   returningUser: "Tekrar Hoş Geldin" + no tagline
 */
Item {
    id: root

    // Mode control — set by OnboardingWizard
    property bool returningUser: false

    signal loginSuccess()

    // Auth state helpers
    readonly property bool isChecking: typeof AuthService !== "undefined"
                                       && AuthService.state === AuthServiceType.Checking

    // === CONTENT ===
    ColumnLayout {
        anchors.centerIn: parent
        width: 380
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
                asynchronous: true
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

        // Tagline — hidden for returning users
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 40
            visible: !root.returningUser
            text: qsTr("K\u00E2r Amac\u0131 G\u00FCtmeyen T\u00FCrk\u00E7e Oyun Yerelle\u015Ftirme Platformu")
            font.pixelSize: 11
            font.family: "Inter"
            font.weight: Font.Normal
            font.letterSpacing: 3
            color: Qt.rgba(1, 1, 1, 0.25)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 600 }
        }

        // Spacer when tagline is hidden (returning user)
        Item {
            Layout.preferredHeight: 40
            visible: root.returningUser
        }

        // === CHECKING STATE — spinner while checkStoredToken() runs ===
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.bottomMargin: 16
            running: root.isChecking
            visible: root.isChecking
            palette.dark: "#22D3EE"
        }

        // === LOGIN CARD ===
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: cardContent.implicitHeight + 56
            radius: 14
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 0.42, 0.62, 0.12)
            visible: !root.isChecking
            opacity: root.isChecking ? 0 : 1

            Behavior on opacity {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                id: cardContent
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.returningUser
                          ? qsTr("Tekrar ho\u015F geldin")
                          : qsTr("Devam etmek i\u00E7in giri\u015F yap\u0131n")
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
            visible: !root.isChecking
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 700 }
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: !root.isChecking
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
            if (AuthService.isAuthenticated)
                root.loginSuccess()
        }
    }

}
