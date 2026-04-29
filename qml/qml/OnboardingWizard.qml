import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
import "screens/onboarding"
pragma ComponentBehavior: Bound

/**
 * OnboardingWizard.qml — First-launch onboarding experience
 *
 * 3 steps: ThemeStep → ScanStep → ReadyStep
 * Background: Premium neon gradient (preserved pixel-perfect).
 */
Item {
    id: root

    signal wizardFinished()

    property int currentStep: 0
    readonly property int totalSteps: 4
    readonly property bool isLastStep: currentStep === totalSteps - 1
    // Pause idle animations when window is minimized/hidden to save GPU
    property bool animationsEnabled: true

    // =========================================================================
    // NEON GRADIENT BACKGROUND (from LoginScreen — DO NOT MODIFY)
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        color: "#0d1117"
        clip: true
    }

    // Scrolling gradient — pre-rendered 3840x1 strip stretched to fill
    // Replaces 6x-wide runtime gradient (380 bytes vs per-frame GPU render)
    Image {
        width: root.width * 6
        height: root.height
        source: "qrc:/qt/qml/MakineLauncher/resources/images/onboarding_gradient.png"
        fillMode: Image.Stretch

        SequentialAnimation on x {
            running: root.visible && root.animationsEnabled
            loops: Animation.Infinite
            NumberAnimation { to: -root.width * 5; duration: 25000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0; duration: 25000; easing.type: Easing.InOutSine }
        }
    }

    // =========================================================================
    // BLOCK MOUSE EVENTS from passing through to content behind
    // =========================================================================
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    // =========================================================================
    // WINDOW DRAG AREA — stops before window buttons
    // =========================================================================
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

    // =========================================================================
    // WINDOW CONTROLS — tray, minimize, close
    // =========================================================================
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        z: 10

        // Tray — disabled during onboarding (system tray not yet initialized)
        Rectangle {
            width: 46; height: 32
            visible: false
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

    // =========================================================================
    // STEP TRANSITION ANIMATIONS — crossfade + subtle slide
    // =========================================================================
    property int _previousStep: 0

    onCurrentStepChanged: {
        if (_previousStep === currentStep) return
        var outgoing = stepStack.children[_previousStep]
        var incoming = stepStack.children[currentStep]
        if (outgoing && incoming) {
            outgoingAnim.target = outgoing
            outgoingAnim.start()
            incoming.opacity = 0
            incoming.y = 16
            incomingAnim.target = incoming
            incomingAnim.start()
        }
        _previousStep = currentStep
    }

    ParallelAnimation {
        id: outgoingAnim
        property var target: null
        NumberAnimation {
            target: outgoingAnim.target; property: "opacity"
            to: 0; duration: 180; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: outgoingAnim.target; property: "y"
            to: -12; duration: 180; easing.type: Easing.InCubic
        }
    }

    ParallelAnimation {
        id: incomingAnim
        property var target: null
        NumberAnimation {
            target: incomingAnim.target; property: "opacity"
            to: 1; duration: 250; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: incomingAnim.target; property: "y"
            to: 0; duration: 250; easing.type: Easing.OutCubic
        }
    }

    // =========================================================================
    // STEP CONTENT
    // =========================================================================
    StackLayout {
        id: stepStack
        anchors.fill: parent
        anchors.topMargin: 40
        anchors.bottomMargin: 56
        currentIndex: root.currentStep

        WelcomeLoginStep {
            onLoginSuccess: root.currentStep = 1
        }

        ThemeStep {
            onNextStep: root.currentStep = 2
        }

        ScanStep {
            onNextStep: root.currentStep = 3
            onPreviousStep: root.currentStep = 1
        }

        ReadyStep {
            onFinished: root.wizardFinished()
        }
    }

    // =========================================================================
    // BOTTOM: Step dots + Skip (first launch only)
    // =========================================================================
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 60
        anchors.rightMargin: 60
        height: 32
        visible: true

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
                    color: index === root.currentStep
                        ? Theme.accentBase
                        : index < root.currentStep
                            ? Theme.success60
                            : Qt.rgba(1, 1, 1, 0.15)

                    Behavior on width {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Skip link — visible on steps 1-2 (after login, before ready)
        Text {
            textFormat: Text.PlainText
            visible: root.currentStep > 0 && !root.isLastStep
            text: qsTr("Atla")
            font.pixelSize: 13
            color: skipMa.containsMouse
                ? Qt.rgba(1, 1, 1, 0.6)
                : Qt.rgba(1, 1, 1, 0.3)

            MouseArea {
                id: skipMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.wizardFinished()
            }
        }
    }
}
