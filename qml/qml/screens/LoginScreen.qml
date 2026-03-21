import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * LoginScreen.qml — Auth gate shown when user is not authenticated
 */
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: Math.min(400, parent.width - 60)

        // Logo
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/qt/qml/MakineLauncher/assets/logo.png"
            sourceSize: Qt.size(80, 80)
            fillMode: Image.PreserveAspectFit
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Makine \u00C7eviri"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        // Subtitle
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Devam etmek i\u00E7in giri\u015F yap\u0131n")
            font.pixelSize: 14
            color: Theme.textSecondary
        }

        Item { Layout.preferredHeight: 8 }

        // Login button
        Button {
            id: loginBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 240
            Layout.preferredHeight: 44
            text: AuthService.state === AuthService.WaitingForBrowser
                  ? qsTr("Taray\u0131c\u0131dan giri\u015F bekleniyor...")
                  : qsTr("Giri\u015F Yap")
            enabled: AuthService.state === AuthService.Unauthenticated

            background: Rectangle {
                radius: 8
                color: loginBtn.enabled
                       ? (loginBtn.hovered ? Theme.accentHover : Theme.accent)
                       : Theme.bgTertiary
            }
            contentItem: Text {
                text: loginBtn.text
                color: "white"
                font.pixelSize: 14
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: AuthService.startLogin()
        }

        // Waiting spinner
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: AuthService.state === AuthService.WaitingForBrowser
                     || AuthService.state === AuthService.Exchanging
                     || AuthService.state === AuthService.Checking
            visible: running
        }

        // Error message
        Text {
            id: errorText
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: parent.width
            visible: text.length > 0
            color: Theme.error
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        // Retry button
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: errorText.visible
            text: qsTr("Tekrar Dene")
            flat: true
            onClicked: {
                errorText.text = ""
                AuthService.retryLogin()
            }
            contentItem: Text {
                text: parent.text
                color: Theme.accent
                font.pixelSize: 13
                font.underline: true
            }
            background: Item {}
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Hesab\u0131n\u0131z yok mu? <a href='https://makineceviri.org/hesap'>Kay\u0131t olun</a>")
            color: Theme.textSecondary
            font.pixelSize: 12
            textFormat: Text.RichText
            onLinkActivated: link => Qt.openUrlExternally(link)

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // Error signal connection
    Connections {
        target: AuthService
        function onLoginError(message) {
            errorText.text = message
        }
    }
}
