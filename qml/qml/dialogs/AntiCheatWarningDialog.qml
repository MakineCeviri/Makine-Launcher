import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AntiCheatWarningDialog.qml - Anti-cheat detection warning dialog
 *
 * Features:
 * - Tespit edilen anti-cheat sistemlerini listeler
 * - Severity bazlı renk kodlaması (low/medium/high/critical)
 * - Risk açıklaması
 * - "Yine de Devam" / "İptal" butonları
 * - Animated warning icon with pulse/shake
 * - Premium glassmorphism effects
 */
Dialog {
    id: root

    // Anti-cheat detection result
    property var detectedSystems: []  // [{name, shortName, description, severity, warning}]
    property string gameName: ""
    property bool animationsEnabled: true

    signal continueAnyway()
    signal cancelled()

    title: qsTr("Koruma Sistemi Tespit Edildi")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 520
    contentHeight: contentColumn.implicitHeight

    // Center in parent
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    // Entry animation
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animFast }
            NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: Dimensions.animFast }
        }
    }

    // Severity colors
    readonly property color severityLow: Theme.severityLow
    readonly property color severityMedium: Theme.severityMedium
    readonly property color severityHigh: Theme.destructive
    readonly property color severityCritical: Theme.severityCritical

    function getSeverityColor(severity) {
        switch (severity) {
            case "low": return severityLow
            case "medium": return severityMedium
            case "high": return severityHigh
            case "critical": return severityCritical
            default: return severityMedium
        }
    }

    function getSeverityIcon(severity) {
        switch (severity) {
            case "low": return "\u2139"         // info
            case "medium": return "\u26A0"      // warning
            case "high": return "\u2716"        // error
            case "critical": return "\u2620"    // skull (dangerous)
            default: return "\u26A0"
        }
    }

    function getSeverityLabel(severity) {
        switch (severity) {
            case "low": return qsTr("Düşük Risk")
            case "medium": return qsTr("Orta Risk")
            case "high": return qsTr("Yüksek Risk")
            case "critical": return qsTr("Kritik Risk")
            default: return qsTr("Bilinmiyor")
        }
    }

    // Get highest severity
    readonly property string highestSeverity: {
        var severities = ["low", "medium", "high", "critical"]
        var maxIndex = 0
        for (var i = 0; i < detectedSystems.length; i++) {
            var idx = severities.indexOf(detectedSystems[i].severity)
            if (idx > maxIndex) maxIndex = idx
        }
        return severities[maxIndex]
    }

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                              getSeverityColor(root.highestSeverity).g,
                              getSeverityColor(root.highestSeverity).b, 0.4)
        border.width: 2

        // Outer glow effect
        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            radius: parent.radius + 8
            color: "transparent"
            border.color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                  getSeverityColor(root.highestSeverity).g,
                                  getSeverityColor(root.highestSeverity).b, 0.15)
            border.width: 4
            z: -1
        }

        // Gradient overlay for depth
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                                             getSeverityColor(root.highestSeverity).g,
                                                             getSeverityColor(root.highestSeverity).b, 0.05) }
                GradientStop { position: 0.3; color: "transparent" }
            }
        }
    }

    // Custom header with animated warning icon
    header: Rectangle {
        height: 90
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginLG
            anchors.rightMargin: Dimensions.marginLG
            spacing: Dimensions.spacingXL

            // Animated warning icon container
            Item {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56

                // Outer pulse glow (for critical/high)
                Rectangle {
                    id: pulseGlow
                    anchors.centerIn: parent
                    width: 64
                    height: 64
                    radius: 32
                    color: "transparent"
                    border.color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                          getSeverityColor(root.highestSeverity).g,
                                          getSeverityColor(root.highestSeverity).b, 0.3)
                    border.width: 3
                    visible: root.highestSeverity === "critical" || root.highestSeverity === "high"

                    SequentialAnimation on scale {
                        running: root.animationsEnabled && pulseGlow.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.2; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                        onRunningChanged: {
                            if (typeof SceneProfiler !== "undefined")
                                SceneProfiler.registerAnimation("antiCheatPulseScale", running)
                        }
                    }

                    SequentialAnimation on opacity {
                        running: root.animationsEnabled && pulseGlow.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: Dimensions.animVerySlow; easing.type: Easing.InOutSine }
                    }
                }

                // Main icon circle
                Rectangle {
                    id: warningIconCircle
                    anchors.centerIn: parent
                    width: 52
                    height: 52
                    radius: 26
                    color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                   getSeverityColor(root.highestSeverity).g,
                                   getSeverityColor(root.highestSeverity).b, 0.2)
                    border.color: getSeverityColor(root.highestSeverity)
                    border.width: 2

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: root.highestSeverity === "critical" ? "\u2620" : "\u26A0"
                        font.pixelSize: Dimensions.headlineXL
                        color: getSeverityColor(root.highestSeverity)
                    }

                    // Shake animation for critical
                    SequentialAnimation on x {
                        running: root.animationsEnabled && root.highestSeverity === "critical"
                        loops: Animation.Infinite
                        NumberAnimation { to: 3; duration: Dimensions.animInstant }
                        NumberAnimation { to: -3; duration: Dimensions.animInstant }
                        NumberAnimation { to: 2; duration: Dimensions.animInstant }
                        NumberAnimation { to: -2; duration: Dimensions.animInstant }
                        NumberAnimation { to: 0; duration: Dimensions.animInstant }
                        PauseAnimation { duration: Dimensions.animGradient }
                        onRunningChanged: {
                            if (typeof SceneProfiler !== "undefined")
                                SceneProfiler.registerAnimation("antiCheatCriticalShake", running)
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: Dimensions.spacingSM

                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Koruma Sistemi Tespit Edildi")
                    font.pixelSize: Dimensions.fontXL
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                RowLayout {
                    spacing: Dimensions.spacingMD

                    Text {
                        textFormat: Text.PlainText
                        text: root.gameName
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textSecondary
                        visible: root.gameName !== ""
                    }

                    // Overall severity badge
                    Rectangle {
                        width: overallSeverityText.width + 16
                        height: 22
                        radius: Dimensions.radiusStandard
                        color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                       getSeverityColor(root.highestSeverity).g,
                                       getSeverityColor(root.highestSeverity).b, 0.2)

                        Text {
                            textFormat: Text.PlainText
                            id: overallSeverityText
                            anchors.centerIn: parent
                            text: getSeverityLabel(root.highestSeverity)
                            font.pixelSize: Dimensions.fontXS
                            font.weight: Font.DemiBold
                            color: getSeverityColor(root.highestSeverity)
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Close button with hover effect
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: closeDialogMouse.containsMouse ? Theme.textPrimary12 : Theme.textPrimary04
                border.color: closeDialogMouse.containsMouse ? Theme.textPrimary20 : "transparent"
                border.width: 1
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }
                Keys.onSpacePressed: { root.cancelled(); root.close() }

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: "\u00D7"
                    font.pixelSize: Dimensions.fontHeadline
                    color: closeDialogMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.primary60
                    border.width: 2
                    visible: parent.activeFocus
                }

                MouseArea {
                    id: closeDialogMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.cancelled()
                        root.close()
                    }
                }
            }
        }

        // Bottom border with gradient
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                                             getSeverityColor(root.highestSeverity).g,
                                                             getSeverityColor(root.highestSeverity).b, 0.3) }
                GradientStop { position: 0.7; color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                                             getSeverityColor(root.highestSeverity).g,
                                                             getSeverityColor(root.highestSeverity).b, 0.3) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingXL

        // Warning message
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.marginLG
            Layout.rightMargin: Dimensions.marginLG
            Layout.preferredHeight: warningText.implicitHeight + 24
            radius: Dimensions.radiusStandard
            color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                           getSeverityColor(root.highestSeverity).g,
                           getSeverityColor(root.highestSeverity).b, 0.1)
            border.color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                  getSeverityColor(root.highestSeverity).g,
                                  getSeverityColor(root.highestSeverity).b, 0.2)
            border.width: 1

            Text {
                textFormat: Text.PlainText
                id: warningText
                anchors.fill: parent
                anchors.margins: Dimensions.marginMS
                text: qsTr("Bu oyunda aktif koruma sistemi tespit edildi. Çeviri uygulamak oyunun çalışmasını engelleyebilir veya online ban'a neden olabilir.")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        // Detected systems list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Dimensions.marginLG
            Layout.rightMargin: Dimensions.marginLG
            clip: true

            ScrollBar.vertical: StyledScrollBar {}

            ListView {
                id: systemsList
                model: root.detectedSystems
                spacing: Dimensions.spacingBase

                // Staggered entry animation
                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.transitionDuration }
                    NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: systemDelegate
                    width: systemsList.width
                    height: 80
                    radius: Dimensions.radiusStandard
                    color: delegateMouse.containsMouse ? Theme.textPrimary06 : Theme.textPrimary03
                    border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                          getSeverityColor(modelData.severity).g,
                                          getSeverityColor(modelData.severity).b,
                                          delegateMouse.containsMouse ? 0.5 : 0.25)
                    border.width: delegateMouse.containsMouse ? 2 : 1
                    scale: delegateMouse.containsMouse ? 1.01 : 1.0

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on border.width { NumberAnimation { duration: Dimensions.animFast } }
                    Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

                    // Left accent bar
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: Dimensions.marginXXS
                        width: 4
                        radius: 2
                        color: getSeverityColor(modelData.severity)
                        opacity: delegateMouse.containsMouse ? 1.0 : 0.6

                        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Dimensions.marginMD
                        anchors.rightMargin: Dimensions.marginMS
                        anchors.topMargin: Dimensions.marginMS
                        anchors.bottomMargin: Dimensions.marginMS
                        spacing: 14

                        // Severity icon with glow
                        Item {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44

                            // Glow layer
                            Rectangle {
                                anchors.centerIn: parent
                                width: 48
                                height: 48
                                radius: 24
                                color: "transparent"
                                border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                                      getSeverityColor(modelData.severity).g,
                                                      getSeverityColor(modelData.severity).b, 0.2)
                                border.width: 2
                                visible: modelData.severity === "critical" || modelData.severity === "high"
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: 42
                                height: 42
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                               getSeverityColor(modelData.severity).g,
                                               getSeverityColor(modelData.severity).b, 0.15)
                                border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                                      getSeverityColor(modelData.severity).g,
                                                      getSeverityColor(modelData.severity).b, 0.3)
                                border.width: 1

                                Text {
                                    textFormat: Text.PlainText
                                    anchors.centerIn: parent
                                    text: getSeverityIcon(modelData.severity)
                                    font.pixelSize: Dimensions.fontXL
                                    color: getSeverityColor(modelData.severity)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingSM

                            RowLayout {
                                spacing: Dimensions.spacingBase

                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.name || modelData.shortName
                                    font.pixelSize: Dimensions.fontSubtitle
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }

                                // Severity badge
                                Rectangle {
                                    width: severityLabelText.width + 14
                                    height: 22
                                    radius: Dimensions.radiusStandard
                                    color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                                   getSeverityColor(modelData.severity).g,
                                                   getSeverityColor(modelData.severity).b, 0.15)
                                    border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                                          getSeverityColor(modelData.severity).g,
                                                          getSeverityColor(modelData.severity).b, 0.3)
                                    border.width: 1

                                    Text {
                                        textFormat: Text.PlainText
                                        id: severityLabelText
                                        anchors.centerIn: parent
                                        text: getSeverityLabel(modelData.severity)
                                        font.pixelSize: Dimensions.fontCaption
                                        font.weight: Font.DemiBold
                                        color: getSeverityColor(modelData.severity)
                                    }
                                }
                            }

                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: modelData.warning || modelData.description
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MouseArea {
                        id: delegateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }
        }
    }

    // Custom footer with premium buttons
    footer: Rectangle {
        height: 88
        color: "transparent"

        // Top border with gradient
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.2; color: Theme.textPrimary10 }
                GradientStop { position: 0.8; color: Theme.textPrimary10 }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginLG
            anchors.rightMargin: Dimensions.marginLG
            anchors.topMargin: Dimensions.marginMS
            anchors.bottomMargin: Dimensions.marginML
            spacing: 14

            Item { Layout.fillWidth: true }

            // Cancel button - outlined style
            Rectangle {
                id: cancelBtn
                Layout.preferredWidth: cancelBtnContent.width + 40
                Layout.preferredHeight: 48
                radius: Dimensions.radiusStandard
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Cancel")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }
                Keys.onSpacePressed: { root.cancelled(); root.close() }
                color: cancelBtnMouse.containsMouse ? Theme.textPrimary08 : Theme.textPrimary03
                border.color: cancelBtnMouse.containsMouse ? Theme.textPrimary25 : Theme.textPrimary15
                border.width: 1.5
                scale: cancelBtnMouse.pressed ? 0.97 : 1.0

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                Row {
                    id: cancelBtnContent
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingMD

                    Text {
                        textFormat: Text.PlainText
                        text: "\u2715"  // X mark
                        font.pixelSize: Dimensions.fontMD
                        color: cancelBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter

                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("İptal Et")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: cancelBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter

                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.primary60
                    border.width: 2
                    visible: parent.activeFocus
                }

                MouseArea {
                    id: cancelBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.cancelled()
                        root.close()
                    }
                }
            }

            // Continue anyway button - danger style with glow
            Item {
                Layout.preferredWidth: continueBtn.width + 8
                Layout.preferredHeight: continueBtn.height + 8

                // Glow effect
                Rectangle {
                    anchors.centerIn: continueBtn
                    width: continueBtn.width + 12
                    height: continueBtn.height + 12
                    radius: continueBtn.radius + 4
                    color: "transparent"
                    border.color: Qt.rgba(getSeverityColor(root.highestSeverity).r,
                                          getSeverityColor(root.highestSeverity).g,
                                          getSeverityColor(root.highestSeverity).b,
                                          continueBtnMouse.containsMouse ? 0.4 : 0.2)
                    border.width: 3
                    z: -1

                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                }

                Rectangle {
                    id: continueBtn
                    anchors.centerIn: parent
                    width: continueBtnContent.width + 40
                    height: 48
                    radius: Dimensions.radiusStandard
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Continue anyway")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { root.continueAnyway(); root.close() }
                    Keys.onSpacePressed: { root.continueAnyway(); root.close() }
                    scale: continueBtnMouse.pressed ? 0.97 : 1.0

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: getSeverityColor(root.highestSeverity) }
                        GradientStop { position: 1.0; color: Qt.darker(getSeverityColor(root.highestSeverity), 1.2) }
                    }

                    Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }

                    // Hover highlight overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: continueBtnMouse.containsMouse ? Theme.textPrimary10 : "transparent"

                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    // Top highlight
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: parent.height * 0.5
                        radius: parent.radius

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.textPrimary15 }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Row {
                        id: continueBtnContent
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingBase

                        Text {
                            textFormat: Text.PlainText
                            text: "\u26A0"
                            font.pixelSize: Dimensions.fontTitle
                            color: Theme.textOnColor
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Yine de Devam Et")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Bold
                            color: Theme.textOnColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -2
                        radius: parent.radius + 2
                        color: "transparent"
                        border.color: Theme.warning60
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: continueBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.continueAnyway()
                            root.close()
                        }
                    }
                }
            }
        }
    }
}
