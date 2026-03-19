import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "screens/onboarding"
pragma ComponentBehavior: Bound

/**
 * OnboardingWizard.qml - First-launch experience
 *
 * 3-step wizard: Welcome → Library Scan → Ready
 * Emits wizardFinished() — parent handles settings persistence.
 */
Rectangle {
    id: root
    color: Theme.bgPrimary

    signal wizardFinished()

    property int currentStep: 0
    readonly property int totalSteps: 3
    readonly property bool isLastStep: currentStep === totalSteps - 1

    // ===== WINDOW DRAG (replaces TitleBar while onboarding is active) =====
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: closeBtn.left
        height: 40
        onPressed: root.Window.window?.startSystemMove()
    }

    // Close button (top-right)
    Rectangle {
        id: closeBtn
        anchors.top: parent.top; anchors.right: parent.right
        anchors.topMargin: 6; anchors.rightMargin: 6
        width: 28; height: 28; radius: 6
        color: closeMa.containsMouse ? Theme.danger20 : "transparent"

        Text {
            textFormat: Text.PlainText
            anchors.centerIn: parent
            text: "\u2715"
            font.pixelSize: 12; color: closeMa.containsMouse ? Theme.danger : Theme.textMuted
        }

        MouseArea {
            id: closeMa; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.quit()
        }
    }

    // ===== BACKGROUND =====

    // Subtle top gradient
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.primary03 }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Top brand line
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.brandGold }
            GradientStop { position: 0.25; color: Theme.brandCoral }
            GradientStop { position: 0.5; color: Theme.brandPurple }
            GradientStop { position: 0.75; color: Theme.brandBlue }
            GradientStop { position: 1.0; color: Theme.brandGreen }
        }
    }

    // ===== MAIN CONTENT =====
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 48
        anchors.bottomMargin: 40
        anchors.leftMargin: 60
        anchors.rightMargin: 60
        spacing: 0

        // ===== STEP CONTENT =====
        StackLayout {
            id: stepStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentStep

            WelcomeStep {
                onNextStep: root.currentStep = 1
            }

            ScanStep {
                onNextStep: root.currentStep = 2
                onPreviousStep: root.currentStep = 0
            }

            ReadyStep {
                onFinished: root.wizardFinished()
            }
        }

        // ===== BOTTOM: Step indicators + Skip =====
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.topMargin: 16

            Item { Layout.fillWidth: true }

            // Step dots
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 6

                Repeater {
                    model: root.totalSteps
                    Rectangle {
                        required property int index
                        width: index === root.currentStep ? 24 : 8
                        height: 6
                        radius: 3
                        color: index === root.currentStep ? Theme.primary
                             : index < root.currentStep ? Theme.success60
                             : Theme.surfaceActive50

                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Skip / continue link (right side)
            Text {
                textFormat: Text.PlainText
                visible: !root.isLastStep
                text: root.currentStep === 0 ? qsTr("Atla") : qsTr("Devam Et")
                font.pixelSize: 13
                color: skipMa.containsMouse ? Theme.textSecondary : Theme.textMuted

                MouseArea {
                    id: skipMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.currentStep === 0)
                            root.wizardFinished()  // Skip wizard entirely
                        else
                            root.currentStep++  // Navigate to next step
                    }
                }
            }
        }
    }
}
