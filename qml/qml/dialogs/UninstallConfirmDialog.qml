import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * UninstallConfirmDialog.qml - Translation uninstall confirmation
 *
 * Glassmorphism-styled confirmation dialog with:
 * - Animated warning icon
 * - Backup restore explanation
 * - Keyboard navigation support
 * - Smooth entry/exit transitions
 */
Dialog {
    id: root

    property string gameId: ""
    property string gameName: ""

    signal confirmed()
    signal cancelled()

    title: qsTr("Yamayı Kaldır")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 440
    contentHeight: contentColumn.implicitHeight

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animFast }
            NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: Dimensions.animFast }
        }
    }

    // Glassmorphism background
    background: Rectangle {
        radius: Dimensions.radiusMD
        color: Theme.glassBackground
        border.color: Theme.withAlpha(Theme.error, 0.15)
        border.width: 1

        // Inner glass highlight
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: "transparent"
            border.color: Theme.glassHighlight
            border.width: 1
        }
    }

    // Dim overlay
    Overlay.modal: Rectangle {
        color: Theme.withAlpha(Theme.bgPrimary, 0.60)
    }

    // Custom header with animated warning icon
    header: Item {
        implicitHeight: 64

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            // Animated warning icon
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                color: Theme.withAlpha(Theme.error, 0.10)
                border.color: Theme.withAlpha(Theme.error, 0.20)
                border.width: 1

                // Pulse animation
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    running: root.opened
                    NumberAnimation { to: 1.06; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                }

                Canvas {
                    anchors.centerIn: parent
                    width: 18; height: 18
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        // Triangle outline
                        ctx.strokeStyle = Theme.error
                        ctx.lineWidth = 1.8
                        ctx.lineJoin = "round"
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.moveTo(9, 2)
                        ctx.lineTo(17, 16)
                        ctx.lineTo(1, 16)
                        ctx.closePath()
                        ctx.stroke()

                        // Exclamation mark
                        ctx.fillStyle = Theme.error
                        ctx.beginPath()
                        ctx.arc(9, 13, 1.2, 0, Math.PI * 2)
                        ctx.fill()
                        ctx.lineWidth = 1.8
                        ctx.beginPath()
                        ctx.moveTo(9, 6)
                        ctx.lineTo(9, 10.5)
                        ctx.stroke()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: root.title
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: root.gameName
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // Close button
            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 14
                color: closeBtnMouse.containsMouse
                    ? Theme.withAlpha(Theme.textPrimary, 0.08)
                    : "transparent"

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Canvas {
                    anchors.centerIn: parent
                    width: 10; height: 10
                    property bool hov: closeBtnMouse.containsMouse
                    onHovChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = hov ? Theme.textPrimary : Theme.textMuted
                        ctx.lineWidth = 1.5
                        ctx.lineCap = "round"
                        ctx.beginPath(); ctx.moveTo(1, 1); ctx.lineTo(9, 9); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(9, 1); ctx.lineTo(1, 9); ctx.stroke()
                    }
                }

                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.cancelled(); root.close() }
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingMD

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        // Description
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            text: qsTr("Yedekten geri yüklenerek Türkçe yama kaldırılacak. Oyun dosyaları orijinal haline döndürülecektir.")
            font.pixelSize: Dimensions.fontSM
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.5
        }

        // Backup info box
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            Layout.preferredHeight: infoContent.implicitHeight + Dimensions.paddingMD * 2
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(Theme.info, 0.06)
            border.color: Theme.withAlpha(Theme.info, 0.12)
            border.width: 1

            RowLayout {
                id: infoContent
                anchors.fill: parent
                anchors.margins: Dimensions.paddingMD
                spacing: Dimensions.spacingSM

                // Info icon
                Rectangle {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    radius: 11
                    color: Theme.withAlpha(Theme.info, 0.12)

                    Label {
                        anchors.centerIn: parent
                        text: "i"
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.Bold
                        font.italic: true
                        color: Theme.info
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Yamayı tekrar kurmak isterseniz, Türkçe Yamalar sayfasından yeniden yükleyebilirsiniz.")
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    // Custom footer with action buttons
    footer: Item {
        implicitHeight: 64

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            // Keyboard hint
            Label {
                text: qsTr("Esc")
                font.pixelSize: Dimensions.fontMicro
                color: Theme.textMuted
                opacity: 0.5
            }

            Item { Layout.fillWidth: true }

            // Cancel button
            Rectangle {
                Layout.preferredWidth: cancelRow.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: cancelMouse.containsMouse
                    ? Theme.withAlpha(Theme.textPrimary, 0.08)
                    : "transparent"
                border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                border.width: 1

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Cancel")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }
                Keys.onSpacePressed: { root.cancelled(); root.close() }

                Row {
                    id: cancelRow
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingXS

                    Label {
                        text: qsTr("Vazgeç")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: cancelMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }
                }

                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.cancelled(); root.close() }
                }
            }

            // Confirm danger button
            Rectangle {
                Layout.preferredWidth: confirmRow.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: confirmMouse.containsMouse
                    ? Theme.error
                    : Theme.withAlpha(Theme.error, 0.85)

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                // Press effect
                scale: confirmMouse.pressed ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Remove patch")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.confirmed(); root.close() }
                Keys.onSpacePressed: { root.confirmed(); root.close() }

                Row {
                    id: confirmRow
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingSM

                    // Trash icon
                    Canvas {
                        width: 12; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "white"
                            ctx.lineWidth = 1.3
                            ctx.lineCap = "round"
                            // Lid
                            ctx.beginPath(); ctx.moveTo(1, 3); ctx.lineTo(11, 3); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(4, 3); ctx.lineTo(4, 1); ctx.lineTo(8, 1); ctx.lineTo(8, 3); ctx.stroke()
                            // Body
                            ctx.beginPath(); ctx.moveTo(2, 3); ctx.lineTo(2.5, 11); ctx.lineTo(9.5, 11); ctx.lineTo(10, 3); ctx.stroke()
                            // Lines
                            ctx.beginPath(); ctx.moveTo(5, 5); ctx.lineTo(5, 9); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(7, 5); ctx.lineTo(7, 9); ctx.stroke()
                        }
                    }

                    Label {
                        text: qsTr("Yamayı Kaldır")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.DemiBold
                        color: Theme.textOnColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: confirmMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.confirmed(); root.close() }
                }
            }
        }
    }

    // Close on Escape
    Keys.onEscapePressed: { root.cancelled(); root.close() }
}
