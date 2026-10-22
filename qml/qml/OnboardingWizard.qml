// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
import "screens/onboarding"
import "components"
pragma ComponentBehavior: Bound

/**
 * OnboardingWizard.qml — First-launch onboarding experience
 *
 * 3 steps: WelcomeStep (what this is) → GamesStep (which of your games have
 * a Turkish patch) → PreferencesStep (accent, watcher, autostart, reports).
 * Enter runs the current step's main action.
 * Background: the brand colours drifting slowly behind a near-black stage.
 */
Item {
    id: root

    signal wizardFinished()

    property int currentStep: 0
    readonly property int totalSteps: 3
    readonly property bool isLastStep: currentStep === totalSteps - 1
    readonly property var stepTitles: [qsTr("Hoş geldin"), qsTr("Oyunların"), qsTr("Tercihler")]

    focus: true
    Keys.onReturnPressed: stepStack.children[root.currentStep].primaryAction()
    Keys.onEnterPressed: stepStack.children[root.currentStep].primaryAction()
    // Pause idle animations when window is minimized/hidden to save GPU
    property bool animationsEnabled: true

    // =========================================================================
    // BACKGROUND — brand gradient strip, dimmed
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        color: "#0A0A0C"
        clip: true
    }

    // Scrolling gradient — pre-rendered 3840x1 strip stretched to fill
    // Replaces 6x-wide runtime gradient (380 bytes vs per-frame GPU render)
    Image {
        width: root.width * 6
        height: root.height
        source: "qrc:/qt/qml/MakineLauncher/resources/images/onboarding_gradient.png"
        fillMode: Image.Stretch
        // A glow of the brand colours, not a wash of them
        opacity: 0.55

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
    // WINDOW DRAG AREA — stops before the window buttons (Main.qml's)
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

        WelcomeStep {
            animate: root.animationsEnabled && root.currentStep === 0
            onNext: root.currentStep = 1
        }

        GamesStep {
            onNext: root.currentStep = 2
            onBack: root.currentStep = 0
        }

        PreferencesStep {
            onFinish: root.wizardFinished()
            onBack: root.currentStep = 1
        }
    }

    // =========================================================================
    // BOTTOM: where you are — numbered, named steps. Not on the welcome,
    // which says "3 kısa adım" itself and keeps the stage to the wall.
    // =========================================================================
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 22
        spacing: 14
        z: 6
        visible: root.currentStep > 0

        Repeater {
            model: root.totalSteps

            Row {
                id: stepItem
                required property int index
                readonly property bool active: stepItem.index === root.currentStep
                readonly property bool done: stepItem.index < root.currentStep
                spacing: 14
                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("Adım %1/%2: %3").arg(stepItem.index + 1).arg(root.totalSteps)
                                 .arg(root.stepTitles[stepItem.index])

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22; height: 22; radius: 11
                    color: stepItem.active ? Theme.accentBase
                         : stepItem.done ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.2)
                         : Qt.rgba(1, 1, 1, 0.08)
                    Behavior on color { ColorAnimation { duration: 200 } }

                    Text {
                        anchors.centerIn: parent
                        visible: !stepItem.done
                        textFormat: Text.PlainText
                        text: String(stepItem.index + 1)
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: stepItem.active ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.5)
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: stepItem.done
                        glyph: Icons.check
                        size: 12
                        color: Theme.success
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    text: root.stepTitles[stepItem.index]
                    font.pixelSize: 13
                    font.weight: stepItem.active ? Font.DemiBold : Font.Medium
                    color: stepItem.active ? "#FFFFFF" : Qt.rgba(1, 1, 1, stepItem.done ? 0.7 : 0.45)
                }
                // Connector to the next step
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: stepItem.index < root.totalSteps - 1
                    width: 28; height: 1
                    color: Qt.rgba(1, 1, 1, 0.15)
                }
            }
        }
    }

    // =========================================================================
    // BOTTOM: Skip — after the welcome, before the last step
    // =========================================================================
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 40
        anchors.bottomMargin: 24
        visible: root.currentStep > 0 && !root.isLastStep
        textFormat: Text.PlainText
        text: qsTr("Atla, ayarları sonra yaparım")
        font.pixelSize: 13
        color: skipHover.hovered ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.45)
        Accessible.role: Accessible.Button
        Accessible.name: text

        HoverHandler { id: skipHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.wizardFinished() }
    }
}
