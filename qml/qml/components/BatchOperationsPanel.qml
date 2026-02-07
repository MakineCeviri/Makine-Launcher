import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * BatchOperationsPanel.qml - Inline panel for batch translation operations
 * Shows progress when running, results summary when completed.
 */
Rectangle {
    id: root

    property bool animationsEnabled: true

    // Panel is visible when batch is running or has results to show
    readonly property bool shouldShow: BatchOperationService.isRunning
                                       || BatchOperationService.results.length > 0

    Layout.fillWidth: true
    Layout.preferredHeight: shouldShow ? contentCol.implicitHeight + 24 : 0
    Layout.topMargin: shouldShow ? 0 : -spacing
    visible: shouldShow
    clip: true

    radius: Dimensions.radiusStandard
    color: Qt.rgba(1, 1, 1, 0.03)
    border.color: BatchOperationService.isRunning
                  ? Theme.withAlpha(Theme.primary, 0.3)
                  : Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    Behavior on Layout.preferredHeight {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }
    Behavior on border.color {
        ColorAnimation { duration: 200 }
    }

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 10

        // ===== HEADER ROW =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // Animated indicator
            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: BatchOperationService.isRunning ? Theme.primary : Theme.success
                visible: root.shouldShow

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: BatchOperationService.isRunning && root.animationsEnabled
                    NumberAnimation { from: 1.0; to: 0.3; duration: 600; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.3; to: 1.0; duration: 600; easing.type: Easing.InOutSine }
                }
            }

            Label {
                text: {
                    if (BatchOperationService.isRunning)
                        return qsTr("Toplu İşlem Devam Ediyor")
                    var results = BatchOperationService.results
                    var failed = results.filter(function(r) { return !r.success }).length
                    if (failed > 0)
                        return qsTr("Toplu İşlem Tamamlandı — %1 hata").arg(failed)
                    return qsTr("Toplu İşlem Tamamlandı")
                }
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            // Stats badges
            Rectangle {
                visible: BatchOperationService.isRunning || BatchOperationService.completedItems > 0
                Layout.preferredHeight: 20
                Layout.preferredWidth: completedLabel.width + 12
                radius: Dimensions.badgeRadius
                color: Theme.withAlpha(Theme.success, 0.12)

                Label {
                    id: completedLabel
                    anchors.centerIn: parent
                    text: BatchOperationService.completedItems + "/" + BatchOperationService.totalItems
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: Theme.success
                }
            }

            Rectangle {
                visible: BatchOperationService.failedItems > 0
                Layout.preferredHeight: 20
                Layout.preferredWidth: failedLabel.width + 12
                radius: Dimensions.badgeRadius
                color: Theme.withAlpha(Theme.destructive, 0.12)

                Label {
                    id: failedLabel
                    anchors.centerIn: parent
                    text: BatchOperationService.failedItems + " " + qsTr("hata")
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: Theme.destructive
                }
            }

            // Cancel button (running) or Clear button (completed)
            Rectangle {
                Layout.preferredWidth: actionBtnLabel.width + 16
                Layout.preferredHeight: 24
                radius: Dimensions.badgeRadius
                Accessible.role: Accessible.Button
                Accessible.name: BatchOperationService.isRunning ? qsTr("İptal") : qsTr("Temizle")
                color: actionBtnMouse.containsMouse
                       ? (BatchOperationService.isRunning
                          ? Theme.withAlpha(Theme.destructive, 0.2)
                          : Qt.rgba(1, 1, 1, 0.1))
                       : (BatchOperationService.isRunning
                          ? Theme.withAlpha(Theme.destructive, 0.1)
                          : Qt.rgba(1, 1, 1, 0.05))

                Behavior on color { ColorAnimation { duration: 150 } }

                Label {
                    id: actionBtnLabel
                    anchors.centerIn: parent
                    text: BatchOperationService.isRunning ? qsTr("İptal") : qsTr("Temizle")
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: BatchOperationService.isRunning ? Theme.destructive : Theme.textSecondary
                }

                MouseArea {
                    id: actionBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (BatchOperationService.isRunning)
                            BatchOperationService.cancel()
                        else
                            BatchOperationService.clearResults()
                    }
                }
            }
        }

        // ===== PROGRESS SECTION (running only) =====
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: BatchOperationService.isRunning

            // Current game label
            Label {
                visible: BatchOperationService.currentGameName !== ""
                text: BatchOperationService.statusMessage || BatchOperationService.currentGameName
                font.pixelSize: 11
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Overall progress bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                radius: 3
                color: Qt.rgba(1, 1, 1, 0.06)

                Rectangle {
                    width: parent.width * BatchOperationService.overallProgress
                    height: parent.height
                    radius: 3
                    color: Theme.primary

                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Per-item progress (smaller)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 3
                radius: 2
                color: Qt.rgba(1, 1, 1, 0.04)
                visible: BatchOperationService.currentItemProgress > 0

                Rectangle {
                    width: parent.width * BatchOperationService.currentItemProgress
                    height: parent.height
                    radius: 2
                    color: Theme.withAlpha(Theme.primary, 0.5)

                    Behavior on width {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                }
            }
        }

        // ===== RESULTS SUMMARY (completed only) =====
        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: !BatchOperationService.isRunning && BatchOperationService.results.length > 0

            Repeater {
                model: {
                    var results = BatchOperationService.results
                    // Show max 5 results inline
                    return results.slice(0, 5)
                }

                Rectangle {
                    required property var modelData
                    required property int index
                    width: resultItemRow.width + 12
                    height: 22
                    radius: Dimensions.badgeRadius
                    color: modelData.success
                           ? Theme.withAlpha(Theme.success, 0.08)
                           : Theme.withAlpha(Theme.destructive, 0.08)

                    Row {
                        id: resultItemRow
                        anchors.centerIn: parent
                        spacing: 4

                        Label {
                            text: modelData.success ? "\u2713" : "\u2717"
                            font.pixelSize: 10
                            color: modelData.success ? Theme.success : Theme.destructive
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: modelData.gameName || modelData.gameId || ""
                            font.pixelSize: 10
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // "+N more" badge
            Rectangle {
                visible: BatchOperationService.results.length > 5
                width: moreLabel.width + 12
                height: 22
                radius: Dimensions.badgeRadius
                color: Qt.rgba(1, 1, 1, 0.05)

                Label {
                    id: moreLabel
                    anchors.centerIn: parent
                    text: "+" + (BatchOperationService.results.length - 5)
                    font.pixelSize: 10
                    color: Theme.textMuted
                }
            }
        }
    }
}
