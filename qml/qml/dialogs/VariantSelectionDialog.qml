import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

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
BaseDialog {
    id: root

    property var variants: []
    property string variantType: "version" // "version" or "platform"
    property int selectedIndex: -1

    signal variantSelected(string variant)

    title: variantType === "platform" ? qsTr("Platform Seçin") : qsTr("Sürüm Seçin")

    width: 400
    contentHeight: contentColumn.implicitHeight

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
                color: Theme.accent10
                border.color: Theme.accent20
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
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            DialogCloseButton { onClicked: { root.cancelled(); root.close() } }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.textPrimary06
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Dimensions.spacingSM

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Label {
            textFormat: Text.PlainText
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
                readonly property bool isSelected: root.selectedIndex === index

                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                Layout.preferredHeight: 42
                radius: Dimensions.radiusStandard
                color: {
                    if (isSelected)
                        return Theme.accent15
                    if (_variantMouse.containsMouse)
                        return Theme.textPrimary06
                    return Theme.textPrimary03
                }
                border.color: isSelected
                    ? Theme.accent40
                    : Theme.textPrimary10
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
                        border.color: isSelected
                            ? Theme.accent
                            : Theme.textPrimary25
                        border.width: 1.5
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 10; height: 10
                            radius: 5
                            color: Theme.accent
                            visible: isSelected
                            scale: isSelected ? 1 : 0
                            Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
                        }
                    }

                    Label {
                        textFormat: Text.PlainText
                        text: modelData
                        font.pixelSize: Dimensions.fontSM
                        font.weight: isSelected ? Font.DemiBold : Font.Normal
                        color: isSelected ? Theme.textPrimary : Theme.textSecondary
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
            height: 1; color: Theme.textPrimary06
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Label {
                textFormat: Text.PlainText
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
                color: _cancelMouse.containsMouse ? Theme.textPrimary08 : "transparent"
                border.color: Theme.textPrimary12
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("İptal")
                activeFocusOnTab: true
                Keys.onReturnPressed: { root.cancelled(); root.close() }

                Label {
                    textFormat: Text.PlainText
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
                    if (root.selectedIndex < 0) return Theme.accent40
                    return _installMouse.containsMouse ? Theme.accent : Theme.accent85
                }
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                scale: _installMouse.pressed && root.selectedIndex >= 0 ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Kur")
                activeFocusOnTab: true
                Keys.onReturnPressed: {
                    if (root.selectedIndex >= 0) {
                        root.variantSelected(root.variants[root.selectedIndex])
                        root.close()
                    }
                }

                Label {
                    textFormat: Text.PlainText
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
}
