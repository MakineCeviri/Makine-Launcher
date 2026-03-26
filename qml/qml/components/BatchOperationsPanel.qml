import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

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
    Layout.topMargin: 0
    visible: shouldShow

    radius: Dimensions.radiusStandard
    color: Theme.textPrimary03

    GradientBorder {
        cornerRadius: parent.radius
        topColor: BatchOperationService.isRunning ? Theme.primary30 : Qt.rgba(1, 1, 1, 0.12)
        bottomColor: BatchOperationService.isRunning ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.02)
        Behavior on topColor { ColorAnimation { duration: Dimensions.transitionDuration } }
    }

    Behavior on Layout.preferredHeight {
        NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic }
    }

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Dimensions.marginMS
        spacing: Dimensions.spacingBase

        // ===== HEADER ROW =====
        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingBase

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
                    onRunningChanged: {
                        if (typeof SceneProfiler !== "undefined")
                            SceneProfiler.registerAnimation("batchOpsPulse", running)
                    }
                }
            }

            Label {
                textFormat: Text.PlainText
                text: {
                    if (BatchOperationService.isRunning)
                        return qsTr("Toplu İşlem Devam Ediyor")
                    if (BatchOperationService.failedItems > 0)
                        return qsTr("Toplu İşlem Tamamlandı — %1 hata").arg(BatchOperationService.failedItems)
                    return qsTr("Toplu İşlem Tamamlandı")
                }
                font.pixelSize: Dimensions.fontBody
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
                color: Theme.success12

                Label {
                    textFormat: Text.PlainText
                    id: completedLabel
                    anchors.centerIn: parent
                    text: BatchOperationService.completedItems + "/" + BatchOperationService.totalItems
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Medium
                    color: Theme.success
                }
            }

            Rectangle {
                visible: BatchOperationService.failedItems > 0
                Layout.preferredHeight: 20
                Layout.preferredWidth: failedLabel.width + 12
                radius: Dimensions.badgeRadius
                color: Theme.destructive12

                Label {
                    textFormat: Text.PlainText
                    id: failedLabel
                    anchors.centerIn: parent
                    text: qsTr("%1 hata").arg(BatchOperationService.failedItems)
                    font.pixelSize: Dimensions.fontCaption
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
                activeFocusOnTab: true
                Keys.onReturnPressed: { if (BatchOperationService.isRunning) BatchOperationService.cancel(); else BatchOperationService.clearResults() }
                Keys.onSpacePressed: { if (BatchOperationService.isRunning) BatchOperationService.cancel(); else BatchOperationService.clearResults() }
                color: actionBtnMouse.containsMouse
                       ? (BatchOperationService.isRunning
                          ? Theme.destructive20
                          : Theme.textPrimary10)
                       : (BatchOperationService.isRunning
                          ? Theme.destructive10
                          : Theme.textPrimary05)

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Label {
                    textFormat: Text.PlainText
                    id: actionBtnLabel
                    anchors.centerIn: parent
                    text: BatchOperationService.isRunning ? qsTr("İptal") : qsTr("Temizle")
                    font.pixelSize: Dimensions.fontXS
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
            spacing: Dimensions.spacingSM
            visible: BatchOperationService.isRunning

            // Current game label
            Label {
                textFormat: Text.PlainText
                visible: BatchOperationService.currentGameName !== ""
                text: BatchOperationService.statusMessage || BatchOperationService.currentGameName
                font.pixelSize: Dimensions.fontXS
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Overall progress bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                radius: 3
                color: Theme.textPrimary06

                Rectangle {
                    width: parent.width * BatchOperationService.overallProgress
                    height: parent.height
                    radius: 3
                    color: Theme.primary

                    Behavior on width {
                        NumberAnimation { duration: Dimensions.fadeTransitionDuration; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Per-item progress (smaller)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 3
                radius: 2
                color: Theme.textPrimary04
                visible: BatchOperationService.currentItemProgress > 0

                Rectangle {
                    width: parent.width * BatchOperationService.currentItemProgress
                    height: parent.height
                    radius: 2
                    color: Theme.primary50

                    Behavior on width {
                        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                    }
                }
            }
        }

        // ===== RESULTS SUMMARY (completed only) =====
        Flow {
            Layout.fillWidth: true
            spacing: Dimensions.spacingSM
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
                           ? Theme.success08
                           : Theme.destructive08

                    Row {
                        id: resultItemRow
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingXS

                        Label {
                            textFormat: Text.PlainText
                            text: modelData.success ? "\u2713" : "\u2717"
                            font.pixelSize: Dimensions.fontCaption
                            color: modelData.success ? Theme.success : Theme.destructive
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: modelData.gameName || modelData.gameId || ""
                            font.pixelSize: Dimensions.fontCaption
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
                color: Theme.textPrimary05

                Label {
                    textFormat: Text.PlainText
                    id: moreLabel
                    anchors.centerIn: parent
                    text: "+" + (BatchOperationService.results.length - 5)
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.textMuted
                }
            }
        }
    }
}
