import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0

/**
 * WelcomeLoginStep.qml — Welcome screen (Step 1 of onboarding)
 *
 * Preserves premium neon design as a wizard step.
 * Background and window controls are provided by OnboardingWizard parent.
 */
Item {
    id: root

    signal loginSuccess()

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

        // === CONTINUE BUTTON — neon pink ===
        Button {
            id: continueBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            hoverEnabled: true

            background: Rectangle {
                radius: 10
                color: {
                    if (continueBtn.pressed) return "#B03070"
                    if (continueBtn.hovered) return "#E04898"
                    return "#D63D8C"
                }
            }
            contentItem: Text {
                text: qsTr("Ba\u015Fla")
                color: "#FFFFFF"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.loginSuccess()
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

        // Version
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "v" + Qt.application.version
            font.pixelSize: 10
            font.letterSpacing: 0.5
            color: Qt.rgba(1, 1, 1, 0.12)
            opacity: 0
            NumberAnimation on opacity { to: 1; duration: 800 }
        }
    } // ColumnLayout
}
