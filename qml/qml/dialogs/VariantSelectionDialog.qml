import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * VariantSelectionDialog.qml - Select version or platform variant before install
 *
 * Usage:
 *   VariantSelectionDialog {
 *       variants: ["1.00", "1.04", "1.05"]
 *       variantType: "version"  // or "platform"
 *       onVariantSelected: function(variant) { install(variant) }
 *   }
 */
Dialog {
    id: root

    property var variants: []
    property string variantType: "version" // "version" or "platform"
    property int selectedIndex: -1

    signal variantSelected(string variant)
    signal cancelled()

    title: variantType === "platform" ? qsTr("Platform Seçin") : qsTr("Sürüm Seçin")

    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 400
    contentHeight: contentColumn.implicitHeight

    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

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

    background: Rectangle {
        radius: Dimensions.radiusMD
        color: Theme.glassBackground
        border.color: Theme.withAlpha(Theme.accent, 0.15)
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: "transparent"
            border.color: Theme.glassHighlight
            border.width: 1
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.withAlpha(Theme.bgPrimary, 0.60)
    }

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.withAlpha(Theme.accent, 0.10)
                border.color: Theme.withAlpha(Theme.accent, 0.20)
                border.width: 1

                Canvas {
                    anchors.centerIn: parent
                    width: 16; height: 16
                    property color c: Theme.accent
                    onCChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = c
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        // List/selection icon
                        ctx.beginPath()
                        ctx.moveTo(3, 4); ctx.lineTo(13, 4); ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(3, 8); ctx.lineTo(13, 8); ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(3, 12); ctx.lineTo(13, 12); ctx.stroke()
                    }
                }
            }

            Label {
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 14
                color: _closeMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : "transparent"
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Canvas {
                    anchors.centerIn: parent
                    width: 10; height: 10
                    property bool hov: _closeMouse.containsMouse
                    onHovChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = hov ? Theme.textPrimary : Theme.textMuted
                        ctx.lineWidth = 1.5; ctx.lineCap = "round"
                        ctx.beginPath(); ctx.moveTo(1, 1); ctx.lineTo(9, 9); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(9, 1); ctx.lineTo(1, 9); ctx.stroke()
                    }
                }

                MouseArea {
                    id: _closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.cancelled(); root.close() }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingSM

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            text: variantType === "platform"
                ? qsTr("Bu oyunun birden fazla platform sürümü mevcut. Lütfen kurulu olan platformu seçin:")
                : qsTr("Bu oyunun birden fazla sürümü mevcut. Lütfen kurulu olan sürümü seçin:")
            font.pixelSize: Dimensions.fontSM
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        // Variant buttons
        Repeater {
            model: root.variants

            Rectangle {
                required property int index
                required property string modelData

                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                Layout.preferredHeight: 42
                radius: Dimensions.radiusStandard
                color: {
                    if (root.selectedIndex === index)
                        return Theme.withAlpha(Theme.accent, 0.15)
                    if (_variantMouse.containsMouse)
                        return Theme.withAlpha(Theme.textPrimary, 0.06)
                    return Theme.withAlpha(Theme.textPrimary, 0.03)
                }
                border.color: root.selectedIndex === index
                    ? Theme.withAlpha(Theme.accent, 0.40)
                    : Theme.withAlpha(Theme.textPrimary, 0.10)
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.paddingMD
                    anchors.rightMargin: Dimensions.paddingMD
                    spacing: Dimensions.spacingSM

                    // Radio indicator
                    Rectangle {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        radius: 9
                        color: "transparent"
                        border.color: root.selectedIndex === index
                            ? Theme.accent
                            : Theme.withAlpha(Theme.textPrimary, 0.25)
                        border.width: 1.5
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 10; height: 10
                            radius: 5
                            color: Theme.accent
                            visible: root.selectedIndex === index
                            scale: root.selectedIndex === index ? 1 : 0
                            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
                        }
                    }

                    Label {
                        text: modelData
                        font.pixelSize: Dimensions.fontSM
                        font.weight: root.selectedIndex === index ? Font.DemiBold : Font.Normal
                        color: root.selectedIndex === index ? Theme.textPrimary : Theme.textSecondary
                        Layout.fillWidth: true
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }
                }

                MouseArea {
                    id: _variantMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectedIndex = index
                }
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 56

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.withAlpha(Theme.textPrimary, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Label {
                text: qsTr("Esc")
                font.pixelSize: Dimensions.fontMicro
                color: Theme.textMuted
                opacity: 0.5
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: _cancelLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _cancelMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : "transparent"
                border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Cancel")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }

                Label {
                    id: _cancelLbl
                    anchors.centerIn: parent
                    text: qsTr("Vazgeç")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: _cancelMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }

                MouseArea {
                    id: _cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.cancelled(); root.close() }
                }
            }

            Rectangle {
                Layout.preferredWidth: _installLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                opacity: root.selectedIndex >= 0 ? 1.0 : 0.5
                color: {
                    if (root.selectedIndex < 0) return Theme.withAlpha(Theme.accent, 0.4)
                    return _installMouse.containsMouse ? Theme.accent : Theme.withAlpha(Theme.accent, 0.85)
                }
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                scale: _installMouse.pressed && root.selectedIndex >= 0 ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Install")
                activeFocusOnTab: true
                Keys.onReturnPressed: {
                    if (root.selectedIndex >= 0) {
                        root.variantSelected(root.variants[root.selectedIndex])
                        root.close()
                    }
                }

                Label {
                    id: _installLbl
                    anchors.centerIn: parent
                    text: qsTr("Kur")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }

                MouseArea {
                    id: _installMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.selectedIndex >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (root.selectedIndex >= 0) {
                            root.variantSelected(root.variants[root.selectedIndex])
                            root.close()
                        }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: { root.cancelled(); root.close() }
}
