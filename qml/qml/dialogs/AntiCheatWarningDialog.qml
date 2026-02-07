import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

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
    height: Math.min(500, contentColumn.implicitHeight + 220)

    // Center in parent
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    // Entry animation
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 }
            NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: 150 }
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
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

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
                        NumberAnimation { to: 1.2; duration: 800; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                    }

                    SequentialAnimation on opacity {
                        running: root.animationsEnabled && pulseGlow.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
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
                        anchors.centerIn: parent
                        text: root.highestSeverity === "critical" ? "\u2620" : "\u26A0"
                        font.pixelSize: 26
                        color: getSeverityColor(root.highestSeverity)
                    }

                    // Shake animation for critical
                    SequentialAnimation on x {
                        running: root.animationsEnabled && root.highestSeverity === "critical"
                        loops: Animation.Infinite
                        NumberAnimation { to: 3; duration: 50 }
                        NumberAnimation { to: -3; duration: 50 }
                        NumberAnimation { to: 2; duration: 50 }
                        NumberAnimation { to: -2; duration: 50 }
                        NumberAnimation { to: 0; duration: 50 }
                        PauseAnimation { duration: 2000 }
                    }
                }
            }

            ColumnLayout {
                spacing: 6

                Text {
                    text: qsTr("Koruma Sistemi Tespit Edildi")
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                RowLayout {
                    spacing: 8

                    Text {
                        text: root.gameName
                        font.pixelSize: 13
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
                            id: overallSeverityText
                            anchors.centerIn: parent
                            text: getSeverityLabel(root.highestSeverity)
                            font.pixelSize: 11
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
                color: closeDialogMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.04)
                border.color: closeDialogMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
                border.width: 1
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }
                Keys.onSpacePressed: { root.cancelled(); root.close() }

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    font.pixelSize: 22
                    color: closeDialogMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
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
        spacing: 16

        // Warning message
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
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
                id: warningText
                anchors.fill: parent
                anchors.margins: 12
                text: qsTr("Bu oyunda aktif koruma sistemi tespit edildi. Çeviri uygulamak oyunun çalışmasını engelleyebilir veya online ban'a neden olabilir.")
                font.pixelSize: 13
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        // Detected systems list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            clip: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                background: Rectangle { color: "transparent" }
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                }
            }

            ListView {
                id: systemsList
                model: root.detectedSystems
                spacing: 10

                // Staggered entry animation
                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
                    NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 200; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: systemDelegate
                    width: systemsList.width
                    height: 80
                    radius: Dimensions.radiusStandard
                    color: delegateMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                          getSeverityColor(modelData.severity).g,
                                          getSeverityColor(modelData.severity).b,
                                          delegateMouse.containsMouse ? 0.5 : 0.25)
                    border.width: delegateMouse.containsMouse ? 2 : 1
                    scale: delegateMouse.containsMouse ? 1.01 : 1.0

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                    Behavior on border.width { NumberAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    // Left accent bar
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        width: 4
                        radius: 2
                        color: getSeverityColor(modelData.severity)
                        opacity: delegateMouse.containsMouse ? 1.0 : 0.6

                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        anchors.topMargin: 12
                        anchors.bottomMargin: 12
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
                                    anchors.centerIn: parent
                                    text: getSeverityIcon(modelData.severity)
                                    font.pixelSize: 20
                                    color: getSeverityColor(modelData.severity)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            RowLayout {
                                spacing: 10

                                Text {
                                    text: modelData.name || modelData.shortName
                                    font.pixelSize: 15
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
                                        id: severityLabelText
                                        anchors.centerIn: parent
                                        text: getSeverityLabel(modelData.severity)
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: getSeverityColor(modelData.severity)
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.warning || modelData.description
                                font.pixelSize: 12
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
                GradientStop { position: 0.2; color: Qt.rgba(1, 1, 1, 0.1) }
                GradientStop { position: 0.8; color: Qt.rgba(1, 1, 1, 0.1) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 12
            anchors.bottomMargin: 20
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
                color: cancelBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.03)
                border.color: cancelBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.15)
                border.width: 1.5
                scale: cancelBtnMouse.pressed ? 0.97 : 1.0

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 100 } }

                Row {
                    id: cancelBtnContent
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "\u2715"  // X mark
                        font.pixelSize: 14
                        color: cancelBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    Text {
                        text: qsTr("İptal Et")
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: cancelBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
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

                    Behavior on border.color { ColorAnimation { duration: 150 } }
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

                    Behavior on scale { NumberAnimation { duration: 100 } }

                    // Hover highlight overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: continueBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    // Top highlight
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: parent.height * 0.5
                        radius: parent.radius

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.15) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Row {
                        id: continueBtnContent
                        anchors.centerIn: parent
                        spacing: 10

                        Text {
                            text: "\u26A0"
                            font.pixelSize: 18
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: qsTr("Yine de Devam Et")
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
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
