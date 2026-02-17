import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "screens/onboarding"

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

    // ===== BACKGROUND =====

    // Subtle top gradient
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.03) }
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

            // Fade+slide transition when changing steps
            property int _prevStep: 0
            onCurrentIndexChanged: {
                stepTransition.target = stepStack.children[currentIndex]
                if (stepTransition.target) {
                    stepTransition.target.opacity = 0
                    stepTranslateAnim.from = currentIndex > _prevStep ? 20 : -20
                    stepTransition.restart()
                }
                _prevStep = currentIndex
            }

            ParallelAnimation {
                id: stepTransition
                property var target
                NumberAnimation {
                    target: stepTransition.target
                    property: "opacity"
                    from: 0; to: 1
                    duration: 280
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    id: stepTranslateAnim
                    target: stepTransition.target
                    property: "x"
                    from: 20; to: 0
                    duration: 280
                    easing.type: Easing.OutCubic
                }
            }

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
                             : index < root.currentStep ? Theme.withAlpha(Theme.success, 0.6)
                             : Theme.withAlpha(Theme.surfaceActive, 0.5)

                        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Skip / continue link (right side)
            Text {
                visible: !root.isLastStep
                text: root.currentStep === 0 ? qsTr("Atla") : qsTr("Devam Et")
                font.pixelSize: 13
                color: skipMa.containsMouse ? Theme.textSecondary : Theme.textMuted
                Behavior on color { ColorAnimation { duration: 150 } }

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
