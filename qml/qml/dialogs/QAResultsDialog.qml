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

    // Severity filter (0 = show all, 1-4 = specific severity)
    property int activeSeverityFilter: 0
    property var filteredIssues: activeSeverityFilter === 0
        ? issues
        : issues.filter(i => i.severity === activeSeverityFilter)

    signal ignoreAndContinue()
    signal fixIssues()

    title: qsTr("Kalite Kontrol Sonuçları")
    modal: true
    closePolicy: Popup.CloseOnEscape
    onOpened: activeSeverityFilter = 0
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
        if (severity >= 4) return qsTr("Kritik")
        if (severity === 3) return qsTr("Önemli")
        if (severity === 2) return qsTr("Uyarı")
        return qsTr("Bilgi")
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
        border.color: Theme.withAlpha(Theme.textPrimary, 0.1)
        border.width: 1
    }

    // Custom header
    header: Rectangle {
        height: 100
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginLG
            anchors.rightMargin: Dimensions.marginLG
            spacing: Dimensions.spacingXL

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
                    font.pixelSize: Dimensions.fontHeadline
                    font.weight: Font.Bold
                    color: scoreColor
                }
            }

            ColumnLayout {
                spacing: Dimensions.spacingXS

                Text {
                    text: root.passed ? qsTr("Kalite Kontrolü Geçti") : qsTr("Kalite Kontrolü Başarısız")
                    font.pixelSize: Dimensions.fontTitle
                    font.weight: Font.DemiBold
                    color: root.passed ? colorSuccess : colorCritical
                }

                Text {
                    text: root.issues.length > 0
                        ? (root.activeSeverityFilter > 0
                            ? qsTr("%1/%2 sorun gösteriliyor").arg(root.filteredIssues.length).arg(root.issues.length)
                            : qsTr("%1 sorun tespit edildi").arg(root.issues.length))
                        : qsTr("Sorun tespit edilmedi")
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                }

                // Issue summary badges (clickable for filtering)
                RowLayout {
                    spacing: Dimensions.spacingMD
                    visible: root.issues.length > 0

                    // "All" badge
                    Rectangle {
                        visible: root.issues.length > 1
                        width: allBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: root.activeSeverityFilter === 0
                            ? Theme.withAlpha(Theme.primary, 0.3)
                            : Theme.withAlpha(Theme.primary, 0.1)
                        border.color: root.activeSeverityFilter === 0 ? Theme.primary : "transparent"
                        border.width: root.activeSeverityFilter === 0 ? 1 : 0

                        Text {
                            id: allBadgeText
                            anchors.centerIn: parent
                            text: qsTr("Tümü")
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.Medium
                            color: Theme.primary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSeverityFilter = 0
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("All issues")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.activeSeverityFilter = 0
                        Keys.onSpacePressed: root.activeSeverityFilter = 0

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -1
                            radius: parent.radius + 1
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: parent.activeFocus
                        }
                    }

                    // Critical badge
                    Rectangle {
                        visible: criticalCount > 0
                        width: criticalBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: root.activeSeverityFilter === 4
                            ? Theme.withAlpha(colorCritical, 0.3)
                            : Theme.withAlpha(colorCritical, 0.15)
                        border.color: root.activeSeverityFilter === 4 ? colorCritical : "transparent"
                        border.width: root.activeSeverityFilter === 4 ? 1 : 0

                        Text {
                            id: criticalBadgeText
                            anchors.centerIn: parent
                            text: qsTr("%1 Kritik").arg(criticalCount)
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.Medium
                            color: colorCritical
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSeverityFilter = root.activeSeverityFilter === 4 ? 0 : 4
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Critical issues")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.activeSeverityFilter = root.activeSeverityFilter === 4 ? 0 : 4
                        Keys.onSpacePressed: root.activeSeverityFilter = root.activeSeverityFilter === 4 ? 0 : 4

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -1
                            radius: parent.radius + 1
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: parent.activeFocus
                        }
                    }

                    // Major badge
                    Rectangle {
                        visible: majorCount > 0
                        width: majorBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: root.activeSeverityFilter === 3
                            ? Theme.withAlpha(colorMajor, 0.3)
                            : Theme.withAlpha(colorMajor, 0.15)
                        border.color: root.activeSeverityFilter === 3 ? colorMajor : "transparent"
                        border.width: root.activeSeverityFilter === 3 ? 1 : 0

                        Text {
                            id: majorBadgeText
                            anchors.centerIn: parent
                            text: qsTr("%1 Önemli").arg(majorCount)
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.Medium
                            color: colorMajor
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSeverityFilter = root.activeSeverityFilter === 3 ? 0 : 3
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Major issues")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.activeSeverityFilter = root.activeSeverityFilter === 3 ? 0 : 3
                        Keys.onSpacePressed: root.activeSeverityFilter = root.activeSeverityFilter === 3 ? 0 : 3

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -1
                            radius: parent.radius + 1
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: parent.activeFocus
                        }
                    }

                    // Warning badge
                    Rectangle {
                        visible: warningCount > 0
                        width: warningBadgeText.width + 12
                        height: 20
                        radius: Dimensions.radiusStandard
                        color: root.activeSeverityFilter === 2
                            ? Theme.withAlpha(colorWarning, 0.3)
                            : Theme.withAlpha(colorWarning, 0.15)
                        border.color: root.activeSeverityFilter === 2 ? colorWarning : "transparent"
                        border.width: root.activeSeverityFilter === 2 ? 1 : 0

                        Text {
                            id: warningBadgeText
                            anchors.centerIn: parent
                            text: qsTr("%1 Uyarı").arg(warningCount)
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.Medium
                            color: colorWarning
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSeverityFilter = root.activeSeverityFilter === 2 ? 0 : 2
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Warning issues")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.activeSeverityFilter = root.activeSeverityFilter === 2 ? 0 : 2
                        Keys.onSpacePressed: root.activeSeverityFilter = root.activeSeverityFilter === 2 ? 0 : 2

                        // Focus indicator
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -1
                            radius: parent.radius + 1
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: parent.activeFocus
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
                color: closeDialogMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close")
                activeFocusOnTab: true
                Keys.onReturnPressed: root.close()
                Keys.onSpacePressed: root.close()

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    font.pixelSize: Dimensions.fontXL
                    color: Theme.textMuted
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                    border.width: 2
                    visible: parent.activeFocus
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
            color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingXL

        // No issues state
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.marginLG
            Layout.rightMargin: Dimensions.marginLG
            Layout.preferredHeight: 80
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(colorSuccess, 0.1)
            border.color: Theme.withAlpha(colorSuccess, 0.2)
            border.width: 1
            visible: root.issues.length === 0

            RowLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingLG

                Text {
                    text: "\u2713"
                    font.pixelSize: Dimensions.headlineLarge
                    color: colorSuccess
                }

                Text {
                    text: qsTr("Harika! Hiçbir sorun tespit edilmedi.")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: colorSuccess
                }
            }
        }

        // Issues list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Dimensions.marginLG
            Layout.rightMargin: Dimensions.marginLG
            clip: true
            visible: root.issues.length > 0

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                background: Rectangle { color: "transparent" }
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: parent.pressed ? Theme.withAlpha(Theme.textPrimary, 0.2) : Theme.withAlpha(Theme.textPrimary, 0.1)
                }
            }

            // Empty state when no issues match filter
            Column {
                anchors.centerIn: parent
                spacing: Dimensions.spacingMD
                visible: root.filteredIssues.length === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\u2714"
                    font.pixelSize: Dimensions.fontBanner
                    color: Theme.success
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.activeSeverityFilter === 0
                        ? qsTr("Sorun bulunamadı")
                        : qsTr("Bu filtre için sorun yok")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }
            }

            ListView {
                id: issuesList
                model: root.filteredIssues
                spacing: Dimensions.spacingMD
                visible: root.filteredIssues.length > 0

                delegate: Rectangle {
                    id: issueItem
                    width: issuesList.width
                    height: expanded ? (issueContent.height + 24) : 56
                    radius: Dimensions.radiusStandard
                    color: issueMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06) : Theme.withAlpha(Theme.textPrimary, 0.03)
                    border.color: Qt.rgba(getSeverityColor(modelData.severity).r,
                                          getSeverityColor(modelData.severity).g,
                                          getSeverityColor(modelData.severity).b, 0.3)
                    border.width: 1
                    clip: true

                    property bool expanded: false

                    Behavior on height { NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    ColumnLayout {
                        id: issueContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Dimensions.marginMS
                        spacing: Dimensions.spacingMD

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

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
                                    font.pixelSize: Dimensions.fontMD
                                    color: getSeverityColor(modelData.severity)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXXS

                                Text {
                                    text: modelData.message
                                    font.pixelSize: Dimensions.fontBody
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: Dimensions.spacingMD

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
                                            font.pixelSize: Dimensions.fontMini
                                            font.weight: Font.Medium
                                            color: getSeverityColor(modelData.severity)
                                        }
                                    }

                                    // Penalty points
                                    Text {
                                        text: qsTr("-%1 puan").arg(modelData.penaltyPoints)
                                        font.pixelSize: Dimensions.fontXS
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
                                    font.pixelSize: Dimensions.fontCaption
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
                            color: Theme.withAlpha(Theme.textPrimary, 0.03)
                            visible: issueItem.expanded
                            opacity: issueItem.expanded ? 1 : 0

                            Behavior on opacity { NumberAnimation { duration: Dimensions.transitionDuration } }

                            Text {
                                id: detailsText
                                anchors.fill: parent
                                anchors.margins: Dimensions.marginSM
                                text: qsTr("Kod: %1").arg(modelData.code) + "\n" +
                                      qsTr("Açıklama: %1").arg(modelData.message) + "\n" +
                                      qsTr("Ceza Puanı: -%1").arg(modelData.penaltyPoints)
                                font.pixelSize: Dimensions.fontSM
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
            color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginLG
            anchors.rightMargin: Dimensions.marginLG
            spacing: Dimensions.spacingLG

            Item { Layout.fillWidth: true }

            // Ignore and Continue button
            Rectangle {
                Layout.preferredWidth: ignoreBtnContent.width + 32
                Layout.preferredHeight: 44
                radius: Dimensions.radiusStandard
                color: ignoreBtnMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                border.color: Theme.withAlpha(Theme.textPrimary, 0.2)
                border.width: 1
                visible: root.issues.length > 0
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Ignore and continue")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.ignoreAndContinue(); root.close() }
                Keys.onSpacePressed: { root.ignoreAndContinue(); root.close() }

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Row {
                    id: ignoreBtnContent
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingMD

                    Text {
                        text: qsTr("Yoksay ve Devam")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -1
                    radius: parent.radius + 1
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                    border.width: 2
                    visible: parent.activeFocus
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
                Accessible.role: Accessible.Button
                Accessible.name: root.issues.length > 0 ? qsTr("Fix issues") : qsTr("OK")
                activeFocusOnTab: true
                Keys.onReturnPressed: { if (root.issues.length > 0) root.fixIssues(); root.close() }
                Keys.onSpacePressed: { if (root.issues.length > 0) root.fixIssues(); root.close() }
                color: fixBtnMouse.containsMouse
                    ? (root.issues.length > 0 ? Theme.primaryHover : Theme.withAlpha(colorSuccess, 0.9))
                    : (root.issues.length > 0 ? Theme.primary : colorSuccess)

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Row {
                    id: fixBtnContent
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingMD

                    Text {
                        text: root.issues.length > 0 ? "\uD83D\uDD27" : "\u2713"  // Wrench or checkmark
                        font.pixelSize: Dimensions.fontLG
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: root.issues.length > 0 ? qsTr("Düzelt") : qsTr("Tamam")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Focus indicator
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: parent.radius + 2
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.gold, 0.6)
                    border.width: 2
                    visible: parent.activeFocus
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
