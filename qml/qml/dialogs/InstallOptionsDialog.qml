import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * InstallOptionsDialog.qml - Checkbox-based install options (multiple selectable)
 *
 * Used for packages with independent components (e.g. patch + dubbing).
 * Supports special themed modes (e.g. "eldenRing" with gold accents).
 */
BaseDialog {
    id: root

    property var options: []          // [{id, label, description, icon, defaultSelected}]
    property var selectedOptionIds: []
    property string specialMode: ""   // "" or "eldenRing"
    property string gameName: ""

    signal optionsConfirmed(var selectedIds)

    accentColor: specialMode === "eldenRing" ? "#C8AA6E" : Theme.accent
    readonly property bool isEldenRing: specialMode === "eldenRing"
    readonly property bool multipleSelected: selectedOptionIds.length > 1

    title: qsTr("Kurulum Seçenekleri")

    width: isEldenRing ? 480 : 420
    contentHeight: contentColumn.implicitHeight

    background: Rectangle {
        radius: Dimensions.radiusMD
        color: Theme.bgSecondary
        border.color: Theme.withAlpha(root.accentColor, 0.15)
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: "transparent"
            border.color: Theme.glassHighlight
            border.width: 1
        }

        // Decorative top gradient band for Elden Ring
        Rectangle {
            visible: root.isEldenRing
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.topMargin: 1
            height: 3
            radius: parent.radius - 1

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Theme.withAlpha(root.accentColor, 0.6) }
                GradientStop { position: 0.7; color: Theme.withAlpha(root.accentColor, 0.6) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            // Icon
            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.withAlpha(root.accentColor, 0.10)
                border.color: Theme.withAlpha(root.accentColor, 0.20)
                border.width: 1

                Canvas {
                    anchors.centerIn: parent
                    width: 18; height: 18
                    renderStrategy: Canvas.Cooperative
                    renderTarget: Canvas.FramebufferObject
                    property color c: root.accentColor
                    property bool er: root.isEldenRing
                    onCChanged: requestPaint()
                    onErChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = c
                        ctx.lineWidth = 1.4
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        if (er) {
                            // Erdtree / rune-like icon
                            ctx.beginPath()
                            ctx.moveTo(9, 2)
                            ctx.lineTo(9, 16)
                            ctx.stroke()
                            // Branches
                            ctx.beginPath()
                            ctx.moveTo(9, 5)
                            ctx.lineTo(4, 3)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 5)
                            ctx.lineTo(14, 3)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 8)
                            ctx.lineTo(3, 6)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 8)
                            ctx.lineTo(15, 6)
                            ctx.stroke()
                            // Roots
                            ctx.beginPath()
                            ctx.moveTo(9, 16)
                            ctx.lineTo(5, 17)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 16)
                            ctx.lineTo(13, 17)
                            ctx.stroke()
                        } else {
                            // Checkbox list icon
                            ctx.beginPath()
                            ctx.rect(2, 2, 5, 5); ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 5); ctx.lineTo(16, 5); ctx.stroke()
                            ctx.beginPath()
                            ctx.rect(2, 10, 5, 5); ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(9, 13); ctx.lineTo(16, 13); ctx.stroke()
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    textFormat: Text.PlainText
                    text: root.gameName || root.title
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: root.isEldenRing ? root.accentColor : Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    textFormat: Text.PlainText
                    visible: root.gameName.length > 0
                    text: root.title
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                    Layout.fillWidth: true
                }
            }

            // Close button
            DialogCloseButton {
                onClicked: { root.cancelled(); root.close() }
            }
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
            text: qsTr("Kurmak istediğiniz bileşenleri seçin:")
            font.pixelSize: Dimensions.fontSM
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        // Option checkboxes
        Repeater {
            model: root.options

            Rectangle {
                required property int index
                required property var modelData

                readonly property bool isChecked: root.selectedOptionIds.indexOf(modelData.id) >= 0
                readonly property string optIcon: modelData.icon || ""

                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG
                Layout.rightMargin: Dimensions.paddingLG
                Layout.preferredHeight: _optContent.implicitHeight + Dimensions.paddingMD * 2
                radius: Dimensions.radiusStandard
                color: {
                    if (isChecked)
                        return Theme.withAlpha(root.accentColor, 0.12)
                    if (_optMouse.containsMouse)
                        return Theme.textPrimary06
                    return Theme.textPrimary03
                }
                border.color: isChecked
                    ? Theme.withAlpha(root.accentColor, 0.40)
                    : Theme.textPrimary10
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                RowLayout {
                    id: _optContent
                    anchors.fill: parent
                    anchors.margins: Dimensions.paddingMD
                    spacing: Dimensions.spacingSM

                    // Checkbox indicator
                    Rectangle {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Layout.alignment: Qt.AlignTop
                        radius: 4
                        color: isChecked ? root.accentColor : "transparent"
                        border.color: isChecked ? root.accentColor : Theme.textPrimary25
                        border.width: 1.5
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                        Canvas {
                            anchors.centerIn: parent
                            width: 12; height: 12
                            renderStrategy: Canvas.Cooperative
                            renderTarget: Canvas.FramebufferObject
                            visible: isChecked
                            onVisibleChanged: if (visible) requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = Theme.textOnColor
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                ctx.moveTo(2, 6)
                                ctx.lineTo(5, 9)
                                ctx.lineTo(10, 3)
                                ctx.stroke()
                            }
                        }
                    }

                    // Option icon
                    Canvas {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 1
                        renderStrategy: Canvas.Cooperative
                        renderTarget: Canvas.FramebufferObject
                        visible: optIcon.length > 0
                        property color c: isChecked ? root.accentColor : Theme.textMuted
                        property string ico: optIcon
                        onCChanged: requestPaint()
                        onIcoChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = c
                            ctx.lineWidth = 1.4
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"

                            if (ico === "text") {
                                // Text/document icon
                                ctx.beginPath()
                                ctx.moveTo(3, 2); ctx.lineTo(3, 14); ctx.lineTo(13, 14); ctx.lineTo(13, 5)
                                ctx.lineTo(10, 2); ctx.lineTo(3, 2); ctx.stroke()
                                ctx.beginPath(); ctx.moveTo(5, 7); ctx.lineTo(11, 7); ctx.stroke()
                                ctx.beginPath(); ctx.moveTo(5, 10); ctx.lineTo(11, 10); ctx.stroke()
                            } else if (ico === "voice") {
                                // Microphone icon
                                ctx.beginPath()
                                ctx.moveTo(8, 2); ctx.lineTo(8, 10); ctx.stroke()
                                ctx.beginPath()
                                ctx.arc(8, 6, 3, -Math.PI/2, Math.PI/2, false); ctx.stroke()
                                ctx.beginPath()
                                ctx.arc(8, 6, 3, Math.PI/2, -Math.PI/2, false); ctx.stroke()
                                ctx.beginPath()
                                ctx.arc(8, 7, 5, Math.PI * 0.15, Math.PI * 0.85, false); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(8, 12); ctx.lineTo(8, 15); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(5, 15); ctx.lineTo(11, 15); ctx.stroke()
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            textFormat: Text.PlainText
                            text: modelData.label || ""
                            font.pixelSize: Dimensions.fontSM
                            font.weight: isChecked ? Font.DemiBold : Font.Normal
                            color: isChecked ? Theme.textPrimary : Theme.textSecondary
                            Layout.fillWidth: true
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        }

                        Label {
                            textFormat: Text.PlainText
                            visible: (modelData.description || "").length > 0
                            text: modelData.description || ""
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                MouseArea {
                    id: _optMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var ids = root.selectedOptionIds.slice()
                        var idx = ids.indexOf(modelData.id)
                        if (idx >= 0) {
                            ids.splice(idx, 1)
                        } else {
                            ids.push(modelData.id)
                        }
                        root.selectedOptionIds = ids
                    }
                }
            }
        }

        // Info box when multiple options selected (Elden Ring specific)
        Rectangle {
            visible: root.isEldenRing && root.multipleSelected
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            Layout.topMargin: Dimensions.spacingXS
            Layout.preferredHeight: _infoRow.implicitHeight + Dimensions.paddingMD * 2
            radius: Dimensions.radiusStandard
            color: Theme.withAlpha(root.accentColor, 0.06)
            border.color: Theme.withAlpha(root.accentColor, 0.15)
            border.width: 1

            RowLayout {
                id: _infoRow
                anchors.fill: parent
                anchors.margins: Dimensions.paddingMD
                spacing: Dimensions.spacingSM

                Label {
                    textFormat: Text.PlainText
                    text: "ℹ"
                    font.pixelSize: Dimensions.fontMD
                    color: root.accentColor
                }

                Label {
                    textFormat: Text.PlainText
                    text: qsTr("eldenring.tr.exe otomatik olarak eldenring.exe olarak yeniden adlandırılacak")
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    lineHeight: 1.3
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

            // Cancel button
            Rectangle {
                Layout.preferredWidth: _cancelLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _cancelMouse.containsMouse ? Theme.textPrimary08 : "transparent"
                border.color: Theme.textPrimary12
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

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

            // Install button
            Rectangle {
                Layout.preferredWidth: _installLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                opacity: root.selectedOptionIds.length > 0 ? 1.0 : 0.5
                color: {
                    if (root.selectedOptionIds.length === 0) return Theme.withAlpha(root.accentColor, 0.4)
                    return _installMouse.containsMouse ? root.accentColor : Theme.withAlpha(root.accentColor, 0.85)
                }
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                scale: _installMouse.pressed && root.selectedOptionIds.length > 0 ? Dimensions.pressScale : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animInstant } }

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
                    cursorShape: root.selectedOptionIds.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (root.selectedOptionIds.length > 0) {
                            root.optionsConfirmed(root.selectedOptionIds)
                            root.close()
                        }
                    }
                }
            }
        }
    }

    // Initialize default selections from options data
    Component.onCompleted: {
        var defaults = []
        if (root.options) {
            for (var i = 0; i < root.options.length; i++) {
                if (root.options[i].defaultSelected) {
                    defaults.push(root.options[i].id)
                }
            }
        }
        root.selectedOptionIds = defaults
    }
}
