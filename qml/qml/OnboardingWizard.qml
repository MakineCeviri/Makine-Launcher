// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
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
 *
 * Made of the launcher itself, so the first screen already looks like what
 * follows: the window's frame and in it the lit page panel (AppBackground,
 * hairline, lit top and foot), the app's name on the top left as the sidebar
 * has it, the steps in the middle of the top bar on a sliding accent knob,
 * and the buttons where a setup assistant keeps them — back on the left of
 * the foot, the step's one action on the right. Steps slide in from the side
 * they come from. Enter runs the current step's main action.
 */
Item {
    id: root

    signal wizardFinished()

    property int currentStep: 0
    readonly property int totalSteps: 3
    readonly property bool isLastStep: currentStep === totalSteps - 1
    readonly property var stepTitles: [qsTr("Hoş geldin"), qsTr("Oyunların"), qsTr("Tercihler")]
    readonly property var _steps: [welcome, games, prefs]
    // The furthest step reached: the track lets you go back to any before it
    property int _reached: 0

    focus: true
    Keys.onReturnPressed: root._steps[root.currentStep].primaryAction()
    Keys.onEnterPressed: root._steps[root.currentStep].primaryAction()
    // Pause idle animations when window is minimized/hidden to save GPU
    property bool animationsEnabled: true

    readonly property real _r: Dimensions.radiusSection

    // =========================================================================
    // THE WINDOW: its frame, and the lit page panel in it
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        color: Theme.chrome
    }

    // Nothing reaches the app behind the wizard
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    Item {
        id: panel
        anchors.fill: parent
        anchors.margins: 8

        AppBackground { anchors.fill: parent }

        // Rounded by painting the frame over the page's corners (as Main.qml)
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: -1
                fillColor: Theme.chrome
                fillRule: ShapePath.OddEvenFill
                PathRectangle { x: -1; y: -1; width: panel.width + 2; height: panel.height + 2 }
                PathRectangle { x: 0; y: 0; width: panel.width; height: panel.height; radius: root._r }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: root._r
            color: "transparent"
            border.width: 1
            border.color: Theme.containerBorder
        }
        Repeater {
            model: Theme.darkMode ? [0, 1] : []
            Rectangle {
                required property int modelData
                x: root._r
                y: modelData === 0 ? 0 : panel.height - 1
                width: panel.width - root._r * 2
                height: 1
                opacity: modelData === 0 ? 1 : 0.7
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.3; color: Theme.containerHighlight }
                    GradientStop { position: 0.7; color: Theme.containerHighlight }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }
    }

    // =========================================================================
    // TOP BAR — the app's name, the steps; it drags the window
    // =========================================================================
    Item {
        id: topBar
        x: panel.x
        y: panel.y
        width: panel.width
        height: 72
        z: 5

        // Drags the window; the window buttons (Main.qml) sit on the right
        Item {
            anchors.fill: parent
            anchors.rightMargin: 150
            DragHandler {
                target: null
                onActiveChanged: if (active) root.Window.window?.startSystemMove()
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 11
            AppLogo {
                anchors.verticalCenter: parent.verticalCenter
                size: 32
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Text {
                    textFormat: Text.PlainText
                    text: "Makine Çeviri"
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.Bold
                    font.letterSpacing: Dimensions.trackingHeadline
                    color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Türkçe yama başlatıcısı")
                    font.pixelSize: Dimensions.caption1
                    font.family: Dimensions.fontText
                    color: Theme.textMuted
                }
            }
        }

        // The steps: done ones carry a check, the current one rides the knob
        Rectangle {
            id: track
            anchors.centerIn: parent
            width: trackRow.width + 8
            height: 40
            radius: height / 2
            color: Theme.fillTertiary
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.05 : 0)

            property Item _at: null
            Rectangle {
                id: knob
                visible: track._at !== null
                x: track._at ? trackRow.x + track._at.x : 0
                y: 4
                width: track._at ? track._at.width : 0
                height: track.height - 8
                radius: height / 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.tint(Theme.primary, Qt.rgba(1, 1, 1, 0.14)) }
                    GradientStop { position: 1.0; color: Theme.primary }
                }
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)
                Behavior on x { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
                Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            }

            Row {
                id: trackRow
                x: 4
                anchors.verticalCenter: parent.verticalCenter
                Repeater {
                    model: root.totalSteps
                    Item {
                        id: stepPill
                        required property int index
                        readonly property bool current: stepPill.index === root.currentStep
                        readonly property bool done: stepPill.index < root.currentStep
                        readonly property bool reachable: stepPill.index <= root._reached && !stepPill.current
                        width: pillRow.implicitWidth + 30
                        height: track.height - 8

                        onCurrentChanged: if (current) track._at = stepPill
                        Component.onCompleted: if (current) track._at = stepPill

                        Accessible.role: Accessible.PageTab
                        Accessible.name: qsTr("Adım %1/%2: %3").arg(stepPill.index + 1).arg(root.totalSteps)
                                         .arg(root.stepTitles[stepPill.index])
                        Accessible.checked: stepPill.current

                        Row {
                            id: pillRow
                            anchors.centerIn: parent
                            spacing: 7
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20; height: 20; radius: 10
                                color: stepPill.current ? Qt.rgba(1, 1, 1, 0.22)
                                     : stepPill.done ? Theme.withAlpha(Theme.success, 0.18)
                                     : Theme.fillSecondary
                                Behavior on color { ColorAnimation { duration: Dimensions.animNormal } }
                                Text {
                                    anchors.centerIn: parent
                                    visible: !stepPill.done
                                    textFormat: Text.PlainText
                                    text: String(stepPill.index + 1)
                                    font.pixelSize: Dimensions.caption1
                                    font.family: Dimensions.fontText
                                    font.weight: Font.Bold
                                    color: stepPill.current ? "#FFFFFF" : Theme.textSecondary
                                }
                                MotionIcon {
                                    id: doneMark
                                    anchors.centerIn: parent
                                    visible: stepPill.done
                                    glyph: Icons.check
                                    size: 12
                                    color: Theme.success
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                                text: root.stepTitles[stepPill.index]
                                font.pixelSize: Dimensions.callout
                                font.weight: stepPill.current ? Font.DemiBold : Font.Medium
                                color: stepPill.current ? "#FFFFFF"
                                     : pillHover.hovered && stepPill.reachable ? Theme.textPrimary : Theme.textSecondary
                            }
                        }
                        onDoneChanged: if (done) doneMark.play()

                        HoverHandler {
                            id: pillHover
                            enabled: stepPill.reachable
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            enabled: stepPill.reachable
                            onTapped: root.currentStep = stepPill.index
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // THE STEPS — the one shown slides in from the side it comes from
    // =========================================================================
    Item {
        id: stepArea
        x: panel.x
        y: topBar.y + topBar.height
        width: panel.width
        height: footBar.y - y
        clip: true

        WelcomeStep {
            id: welcome
            anchors.fill: parent
            animate: root.animationsEnabled && root.currentStep === 0
            onNext: root.currentStep = 1
        }
        GamesStep {
            id: games
            anchors.fill: parent
            onNext: root.currentStep = 2
        }
        PreferencesStep {
            id: prefs
            anchors.fill: parent
            onFinish: root.wizardFinished()
        }
    }

    property int _shown: 0
    Component.onCompleted: {
        for (let i = 0; i < root._steps.length; ++i) {
            root._steps[i].visible = i === root.currentStep
            root._steps[i].opacity = i === root.currentStep ? 1 : 0
        }
    }
    onCurrentStepChanged: {
        if (root.currentStep > root._reached) root._reached = root.currentStep
        if (root._shown === root.currentStep) return
        const dir = root.currentStep > root._shown ? 1 : -1
        const outgoing = root._steps[root._shown]
        const incoming = root._steps[root.currentStep]
        root._shown = root.currentStep
        outAnim.stop(); inAnim.stop()
        outAnim.target = outgoing
        outAnim.toX = -dir * 60
        inAnim.target = incoming
        incoming.x = dir * 80
        incoming.opacity = 0
        incoming.visible = true
        outAnim.start()
        inAnim.start()
    }
    ParallelAnimation {
        id: outAnim
        property Item target: null
        property real toX: 0
        NumberAnimation { target: outAnim.target; property: "opacity"; to: 0; duration: Dimensions.animFast; easing.type: Easing.OutCubic }
        NumberAnimation { target: outAnim.target; property: "x"; to: outAnim.toX; duration: Dimensions.animNormal; easing.type: Easing.InCubic }
        onFinished: if (outAnim.target && outAnim.target !== root._steps[root.currentStep]) {
            outAnim.target.visible = false
            outAnim.target.x = 0
        }
    }
    ParallelAnimation {
        id: inAnim
        property Item target: null
        SequentialAnimation {
            PauseAnimation { duration: 80 }
            ParallelAnimation {
                NumberAnimation { target: inAnim.target; property: "opacity"; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
                NumberAnimation { target: inAnim.target; property: "x"; to: 0; duration: Dimensions.animSlow * 1.3; easing.type: Easing.OutExpo }
            }
        }
    }

    // =========================================================================
    // FOOT — back on the left, the step's one action on the right
    // =========================================================================
    Item {
        id: footBar
        x: panel.x
        y: panel.y + panel.height - height
        width: panel.width
        height: 78
        z: 5

        FadeLine {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            lit: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 10

            AppButton {
                visible: root.currentStep > 0
                style: "gray"
                size: "large"
                icon: Icons.chevronLeft
                text: qsTr("Geri")
                onClicked: root.currentStep = root.currentStep - 1
            }
            // On the welcome: the version and the site, quietly
            Text {
                visible: root.currentStep === 0
                textFormat: Text.PlainText
                text: "v" + Qt.application.version + "  ·  makineceviri.org"
                font.pixelSize: Dimensions.footnote
                font.family: Dimensions.fontText
                color: Theme.textMuted
            }
            Item { Layout.fillWidth: true }
            AppButton {
                visible: root.currentStep > 0 && !root.isLastStep
                style: "plain"
                text: qsTr("Atla, ayarları sonra yaparım")
                onClicked: root.wizardFinished()
            }
            AppButton {
                style: "filled"
                size: "large"
                trailingIcon: root.isLastStep ? Icons.check : Icons.arrowRight
                text: root.currentStep === 0 ? qsTr("Başlayalım")
                    : root.isLastStep ? qsTr("Makine Launcher'ı aç") : qsTr("Devam")
                onClicked: root._steps[root.currentStep].primaryAction()
            }
        }
    }
}
