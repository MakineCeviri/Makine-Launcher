import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * QAResultsDialog.qml - QA (Kalite Kontrol) sonuçları dialog'u
 *
 * Features:
 * - Issue listesi (Critical/Major/Warning/Info)
 * - Severity bazlı renk kodlaması
 * - Skor gösterimi
 * - "Yoksay ve Devam" / "Düzelt" butonları
 * - Issue details expand
 */
Dialog {
    id: root

    // QA result data
    property int score: 100
    property bool passed: true
    property bool hasCriticalIssues: false
    property var issues: []  // [{code, message, severity, penaltyPoints}]

    // Summary counts
    property int criticalCount: issues.filter(i => i.severity >= 4).length
    property int majorCount: issues.filter(i => i.severity === 3).length
    property int warningCount: issues.filter(i => i.severity === 2).length
    property int infoCount: issues.filter(i => i.severity === 1).length

    signal ignoreAndContinue()
    signal fixIssues()

    title: "Kalite Kontrol Sonuçları"
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 550
    height: Math.min(550, contentColumn.implicitHeight + 250)

    // Center in parent
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    // Severity colors
    readonly property color colorCritical: Theme.destructive
    readonly property color colorMajor: Theme.steamOrange
    readonly property color colorWarning: Theme.warning
    readonly property color colorInfo: Theme.primary
    readonly property color colorSuccess: Theme.statusOnline

    function getSeverityColor(severity) {
        if (severity >= 4) return colorCritical
        if (severity === 3) return colorMajor
        if (severity === 2) return colorWarning
        return colorInfo
    }

    function getSeverityIcon(severity) {
        if (severity >= 4) return "\u2716"    // X - critical
        if (severity === 3) return "\u26A0"   // warning
        if (severity === 2) return "\u2139"   // info
        return "\u2022"                        // bullet
    }

    function getSeverityLabel(severity) {
        if (severity >= 4) return "Kritik"
        if (severity === 3) return "Önemli"
        if (severity === 2) return "Uyarı"
        return "Bilgi"
    }

    // Score color
    readonly property color scoreColor: {
        if (score >= 90) return colorSuccess
        if (score >= 70) return colorWarning
        if (score >= 50) return colorMajor
        return colorCritical
    }

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
    }

    // Custom header
    header: Rectangle {
        height: 100
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

            // Score circle
            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                radius: 32
                color: Theme.withAlpha(scoreColor, 0.15)
                border.color: scoreColor
                border.width: 3

                Text {
                    anchors.centerIn: parent
                    text: root.score
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: scoreColor
                }
            }

            ColumnLayout {
                spacing: 4

                Text {
                    text: root.passed ? "Kalite Kontrolü Geçti" : "Kalite Kontrolü Başarısız"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: root.passed ? colorSuccess : colorCritical
                }

                Text {
                    text: root.issues.length > 0
                        ? root.issues.length + " sorun tespit edildi"
                        : "Sorun tespit edilmedi"
                    font.pixelSize: 13
                    color: Theme.textMuted
                }

                // Issue summary badges
                RowLayout {
                    spacing: 8
                    visible: root.issues.length > 0

                    // Critical badge
                    Rectangle {
                        visible: criticalCount > 0
                        width: criticalBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(colorCritical, 0.15)

                        Text {
                            id: criticalBadgeText
                            anchors.centerIn: parent
                            text: criticalCount + " Kritik"
                            font.pixelSize: 10
                            font.weight: Font.Medium
                            color: colorCritical
                        }
                    }

                    // Major badge
                    Rectangle {
                        visible: majorCount > 0
                        width: majorBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(colorMajor, 0.15)

                        Text {
                            id: majorBadgeText
                            anchors.centerIn: parent
                            text: majorCount + " Önemli"
                            font.pixelSize: 10
                            font.weight: Font.Medium
                            color: colorMajor
                        }
                    }

                    // Warning badge
                    Rectangle {
                        visible: warningCount > 0
                        width: warningBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(colorWarning, 0.15)

                        Text {
                            id: warningBadgeText
                            anchors.centerIn: parent
                            text: warningCount + " Uyarı"
                            font.pixelSize: 10
                            font.weight: Font.Medium
                            color: colorWarning
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Close button
            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: Dimensions.radiusStandard
                color: closeDialogMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    font.pixelSize: 20
                    color: Theme.textMuted
                }

                MouseArea {
                    id: closeDialogMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 16

        // No issues state
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.preferredHeight: 80
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(colorSuccess, 0.1)
            border.color: Theme.withAlpha(colorSuccess, 0.2)
            border.width: 1
            visible: root.issues.length === 0

            RowLayout {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    text: "\u2713"
                    font.pixelSize: 24
                    color: colorSuccess
                }

                Text {
                    text: "Harika! Hiçbir sorun tespit edilmedi."
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: colorSuccess
                }
            }
        }

        // Issues list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            clip: true
            visible: root.issues.length > 0

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
                id: issuesList
                model: root.issues
                spacing: 8

                delegate: Rectangle {
                    id: issueItem
                    width: issuesList.width
                    height: expanded ? (issueContent.height + 24) : 56
                    radius: Dimensions.radiusStandard
                    color: issueMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                          getSeverityColor(modelData.severity).g,
                                          getSeverityColor(modelData.severity).b, 0.3)
                    border.width: 1
                    clip: true

                    property bool expanded: false

                    Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }

                    ColumnLayout {
                        id: issueContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            // Severity icon
                            Rectangle {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                               getSeverityColor(modelData.severity).g,
                                               getSeverityColor(modelData.severity).b, 0.15)

                                Text {
                                    anchors.centerIn: parent
                                    text: getSeverityIcon(modelData.severity)
                                    font.pixelSize: 14
                                    color: getSeverityColor(modelData.severity)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: modelData.message
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 8

                                    // Severity badge
                                    Rectangle {
                                        width: severityText.width + 10
                                        height: 16
                                        radius: 3
                                        color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                                       getSeverityColor(modelData.severity).g,
                                                       getSeverityColor(modelData.severity).b, 0.15)

                                        Text {
                                            id: severityText
                                            anchors.centerIn: parent
                                            text: getSeverityLabel(modelData.severity)
                                            font.pixelSize: 9
                                            font.weight: Font.Medium
                                            color: getSeverityColor(modelData.severity)
                                        }
                                    }

                                    // Penalty points
                                    Text {
                                        text: "-" + modelData.penaltyPoints + " puan"
                                        font.pixelSize: 11
                                        color: Theme.textMuted
                                        visible: modelData.penaltyPoints > 0
                                    }
                                }
                            }

                            // Expand button
                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                radius: Dimensions.radiusStandard
                                color: "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: issueItem.expanded ? "\u25B2" : "\u25BC"  // Up/Down arrow
                                    font.pixelSize: 10
                                    color: Theme.textMuted
                                    rotation: issueItem.expanded ? 0 : 0
                                }
                            }
                        }

                        // Expanded details
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: detailsText.implicitHeight + 16
                            radius: Dimensions.radiusStandard
                            color: Qt.rgba(1, 1, 1, 0.03)
                            visible: issueItem.expanded
                            opacity: issueItem.expanded ? 1 : 0

                            Behavior on opacity { NumberAnimation { duration: 200 } }

                            Text {
                                id: detailsText
                                anchors.fill: parent
                                anchors.margins: 8
                                text: "Kod: " + modelData.code + "\n" +
                                      "Açıklama: " + modelData.message + "\n" +
                                      "Ceza Puanı: -" + modelData.penaltyPoints
                                font.pixelSize: 12
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MouseArea {
                        id: issueMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: issueItem.expanded = !issueItem.expanded
                    }
                }
            }
        }
    }

    // Custom footer
    footer: Rectangle {
        height: 80
        color: "transparent"

        // Top border
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 12

            Item { Layout.fillWidth: true }

            // Ignore and Continue button
            Rectangle {
                Layout.preferredWidth: ignoreBtnContent.width + 32
                Layout.preferredHeight: 44
                radius: Dimensions.radiusStandard
                color: ignoreBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.2)
                border.width: 1
                visible: root.issues.length > 0

                Behavior on color { ColorAnimation { duration: 150 } }

                Row {
                    id: ignoreBtnContent
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "Yoksay ve Devam"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: ignoreBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.ignoreAndContinue()
                        root.close()
                    }
                }
            }

            // Fix Issues / OK button
            Rectangle {
                Layout.preferredWidth: fixBtnContent.width + 32
                Layout.preferredHeight: 44
                radius: Dimensions.radiusStandard
                color: fixBtnMouse.containsMouse
                    ? (root.issues.length > 0 ? Theme.primaryHover : Theme.withAlpha(colorSuccess, 0.9))
                    : (root.issues.length > 0 ? Theme.primary : colorSuccess)

                Behavior on color { ColorAnimation { duration: 150 } }

                Row {
                    id: fixBtnContent
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: root.issues.length > 0 ? "\uD83D\uDD27" : "\u2713"  // Wrench or checkmark
                        font.pixelSize: 16
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: root.issues.length > 0 ? "Düzelt" : "Tamam"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: fixBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.issues.length > 0) {
                            root.fixIssues()
                        }
                        root.close()
                    }
                }
            }
        }
    }
}
