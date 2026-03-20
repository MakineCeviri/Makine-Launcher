import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TranslationActionButton — Premium CTA for translation lifecycle.
 *
 * States: download, update, installing, installed, completed, broken.
 * Features: gradient border, contextual icons, shimmer progress,
 *           hover lift, smooth state transitions.
 */
Rectangle {
    id: actionBtn

    required property var vm

    signal translateClicked()
    signal updateClicked()
    signal uninstallClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 52
    radius: Dimensions.radiusLG
    visible: actionBtn.vm.hasTranslation && actionBtn.vm.isGameInstalled
    border.width: _state === "installed" && _hovered ? 2 : 0
    border.color: Theme.error

    // ── State resolution (C++ TranslationStateManager) ──
    readonly property string _state: TranslationStateManager.state

    function _reevaluateState() {
        TranslationStateManager.evaluate(
            actionBtn.vm.hasTranslationUpdate,
            actionBtn.vm.packageInstalled,
            actionBtn.vm.isInstallingTranslation,
            actionBtn.vm.installCompleted,
            actionBtn.vm.impactLevel,
            actionBtn.vm.externalUrl
        )
    }

    // Re-evaluate whenever any input changes
    Connections {
        target: actionBtn.vm
        function onHasTranslationUpdateChanged() { actionBtn._reevaluateState() }
        function onPackageInstalledChanged()      { actionBtn._reevaluateState() }
        function onIsInstallingTranslationChanged(){ actionBtn._reevaluateState() }
        function onInstallCompletedChanged()      { actionBtn._reevaluateState() }
        function onImpactLevelChanged()           { actionBtn._reevaluateState() }
        function onExternalUrlChanged()           { actionBtn._reevaluateState() }
    }
    Component.onCompleted: _reevaluateState()

    readonly property bool _hovered: actionMouse.containsMouse
    readonly property bool _interactive: _state !== "completed" && _state !== "installing"

    // ── Background color per state ──
    color: {
        switch (_state) {
        case "broken":
            return _hovered ? Theme.darken(Theme.error, 0.15) : Theme.error
        case "completed":
            return Theme.success
        case "installing":
            return Theme.surface
        case "update":
            return _hovered ? Theme.darken(Theme.warning, 0.12) : Theme.warning
        case "installed":
            return _hovered ? Theme.error : Theme.success12
        case "external":
            return _hovered ? Qt.darker("#907575", 1.15) : "#907575"
        case "download":
        default:
            return _hovered ? Theme.primaryHover : Theme.primary
        }
    }
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    // ── Subtle hover scale ──
    scale: _interactive && _hovered ? 1.012 : 1.0
    Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

    // ── Premium gradient border (top-lit) ──
    GradientBorder {
        cornerRadius: actionBtn.radius
        topColor: {
            switch (actionBtn._state) {
            case "installing": return Qt.rgba(1, 1, 1, 0.08)
            case "installed":  return actionBtn._hovered ? Qt.rgba(1, 0.3, 0.3, 0.2) : Qt.rgba(1, 1, 1, 0.10)
            case "broken":     return Qt.rgba(1, 0.4, 0.4, 0.25)
            case "update":    return Qt.rgba(1, 0.85, 0.3, 0.2)
            default:           return Qt.rgba(1, 1, 1, 0.18)
            }
        }
        midColor: Qt.rgba(1, 1, 1, 0.04)
        bottomColor: Qt.rgba(1, 1, 1, 0.01)
    }

    // ── Progress track (installing state) ──
    Rectangle {
        id: progressTrack
        visible: actionBtn._state === "installing" && actionBtn.vm.installProgress > 0
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 2 }
        width: Math.max(0, (parent.width - 4) * actionBtn.vm.installProgress)
        radius: parent.radius - 2
        color: Theme.primary15
        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    // ── Shimmer sweep (installing state) ──
    Rectangle {
        id: shimmer
        visible: actionBtn._state === "installing" && actionBtn.vm.installProgress > 0
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 2 }
        width: progressTrack.width
        radius: parent.radius - 2
        clip: true

        color: "transparent"
        property real pos: 0
        NumberAnimation on pos {
            running: shimmer.visible && actionBtn.visible
            from: -0.3; to: 1.3; duration: Dimensions.animLoadingCycle
            loops: Animation.Infinite
        }

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: Math.max(0, shimmer.pos - 0.12); color: "transparent" }
            GradientStop { position: Math.max(0, Math.min(1, shimmer.pos)); color: Theme.primary20 }
            GradientStop { position: Math.min(1, shimmer.pos + 0.12); color: "transparent" }
        }
    }

    // ── Button content ──
    RowLayout {
        anchors.centerIn: parent
        spacing: Dimensions.spacingMD

        // Apex logo (external state only) — embedded, sized to button height
        Image {
            visible: actionBtn._state === "external"
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: actionBtn.height * 1.06
            Layout.preferredWidth: Layout.preferredHeight * (sourceSize.width / Math.max(1, sourceSize.height))
            source: "qrc:/qt/qml/MakineLauncher/resources/images/apex_logo.svg"
            sourceSize: Qt.size(128, 128)
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        // State icon (Canvas-rendered for crisp small sizes)
        Canvas {
            id: stateIcon
            width: 18; height: 18
            visible: actionBtn._state !== "installing" && actionBtn._state !== "external"
            Layout.alignment: Qt.AlignVCenter
            renderStrategy: Canvas.Cooperative

            property color iconColor: {
                if (actionBtn._state === "installed")
                    return actionBtn._hovered ? Theme.textOnColor : Theme.success
                return Theme.textOnColor
            }
            Behavior on iconColor { ColorAnimation { duration: Dimensions.animFast } }

            onPaint: drawIcon()
            onIconColorChanged: requestPaint()
            Component.onCompleted: requestPaint()

            // Redraw when state changes
            property string _st: actionBtn._state
            on_StChanged: requestPaint()
            property bool _h: actionBtn._hovered
            on_HChanged: { if (actionBtn._state === "installed") requestPaint() }

            function drawIcon() {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = iconColor
                ctx.fillStyle = iconColor
                ctx.lineWidth = 1.8
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                var cx = width / 2, cy = height / 2

                switch (actionBtn._state) {
                case "external":
                    // External link icon (arrow going top-right from box)
                    ctx.beginPath()
                    ctx.moveTo(7, 4)
                    ctx.lineTo(14, 4)
                    ctx.lineTo(14, 11)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(14, 4)
                    ctx.lineTo(6, 12)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(10, 4)
                    ctx.lineTo(4, 4)
                    ctx.lineTo(4, 14)
                    ctx.lineTo(14, 14)
                    ctx.lineTo(14, 11)
                    ctx.stroke()
                    break

                case "download":
                case "update":
                    // Down arrow + line (same icon for both states)
                    ctx.beginPath()
                    ctx.moveTo(cx, 3)
                    ctx.lineTo(cx, 11.5)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(cx - 4, 8)
                    ctx.lineTo(cx, 12)
                    ctx.lineTo(cx + 4, 8)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(4, 15)
                    ctx.lineTo(14, 15)
                    ctx.stroke()
                    break

                case "installed":
                    if (actionBtn._hovered) {
                        // Trash icon
                        ctx.beginPath()
                        ctx.moveTo(5, 5.5)
                        ctx.lineTo(13, 5.5)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(cx, 4)
                        ctx.lineTo(cx, 5.5)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(6, 5.5)
                        ctx.lineTo(6.8, 14)
                        ctx.lineTo(11.2, 14)
                        ctx.lineTo(12, 5.5)
                        ctx.stroke()
                    } else {
                        // Checkmark
                        ctx.lineWidth = 2.2
                        ctx.beginPath()
                        ctx.moveTo(4.5, cy)
                        ctx.lineTo(7.5, cy + 3.5)
                        ctx.lineTo(13.5, cy - 3)
                        ctx.stroke()
                    }
                    break

                case "completed":
                    // Double checkmark (success)
                    ctx.lineWidth = 2.0
                    ctx.beginPath()
                    ctx.moveTo(2, cy)
                    ctx.lineTo(5, cy + 3.5)
                    ctx.lineTo(11, cy - 3)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(6, cy)
                    ctx.lineTo(9, cy + 3.5)
                    ctx.lineTo(15, cy - 3)
                    ctx.stroke()
                    break

                case "broken":
                    // Warning triangle
                    ctx.lineWidth = 1.8
                    ctx.beginPath()
                    ctx.moveTo(cx, 3)
                    ctx.lineTo(15, 15)
                    ctx.lineTo(3, 15)
                    ctx.closePath()
                    ctx.stroke()
                    // Exclamation
                    ctx.lineWidth = 2.0
                    ctx.beginPath()
                    ctx.moveTo(cx, 7.5)
                    ctx.lineTo(cx, 11)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(cx, 13.2, 0.8, 0, Math.PI * 2)
                    ctx.fill()
                    break
                }
            }
        }

        // Label text
        Text {
            id: labelText
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignVCenter
            text: {
                switch (actionBtn._state) {
                case "broken":
                    return qsTr("ONARIM GEREKLİ")
                case "completed":
                    return qsTr("Yama Başarıyla Kuruldu")
                case "update":
                    return qsTr("Güncelleme Mevcut")
                case "installed":
                    return actionBtn._hovered ? qsTr("Yamayı Kaldır") : qsTr("Türkçe Yama Kurulu")
                case "installing":
                    if (actionBtn.vm.isDownloading && actionBtn.vm.installStatus !== "")
                        return actionBtn.vm.installStatus
                    if (actionBtn.vm.installProgress > 0)
                        return qsTr("Kuruluyor... %1%").arg(actionBtn.vm.progressPercent)
                    return actionBtn.vm.installStatus || qsTr("Hazırlanıyor...")
                case "external":
                    return qsTr("İndir")
                case "download":
                default:
                    return qsTr("Türkçe Yama İndir")
                }
            }
            font.pixelSize: Dimensions.fontMD
            font.weight: Font.DemiBold
            font.letterSpacing: actionBtn._state === "download" || actionBtn._state === "update"
                                || actionBtn._state === "broken" || actionBtn._state === "external" ? 0.8 : 0.3
            color: {
                switch (actionBtn._state) {
                case "installing": return Theme.textPrimary
                case "installed":  return actionBtn._hovered ? Theme.textOnColor : Theme.success
                case "external":   return "#ffcec6"
                default:           return Theme.textOnColor
                }
            }
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }

        // Cancel button (installing only)
        Rectangle {
            visible: actionBtn._state === "installing"
            Layout.alignment: Qt.AlignVCenter
            width: 28; height: 28
            radius: Dimensions.radiusSM
            color: cancelMouse.containsMouse ? Theme.error12 : "transparent"
            border.color: cancelMouse.containsMouse ? Theme.error40 : Theme.textMuted15
            border.width: 1
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Cancel installation")

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2715"
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.Bold
                color: cancelMouse.containsMouse ? Theme.error : Theme.textMuted
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            }
            MouseArea {
                id: cancelMouse
                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (actionBtn.vm.isDownloading)
                        TranslationDownloader.cancelDownload(actionBtn.vm.gameId)
                    else
                        GameService.cancelInstallation()
                }
            }
        }
    }

    // ── Accessibility ──
    Accessible.role: actionBtn._state === "installing" ? Accessible.ProgressBar : Accessible.Button
    Accessible.name: {
        switch (_state) {
        case "installing": return qsTr("Installing %1%").arg(actionBtn.vm.progressPercent)
        case "update":     return qsTr("Update available")
        case "installed":  return qsTr("Installed")
        case "broken":     return qsTr("Repair needed")
        case "completed":  return qsTr("Installation complete")
        case "external":   return qsTr("Download from ApexYama")
        default:           return qsTr("Download Turkish Patch")
        }
    }

    // ── Interaction ──
    MouseArea {
        id: actionMouse
        anchors.fill: parent; hoverEnabled: true
        cursorShape: actionBtn._interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: actionBtn._interactive
        onClicked: {
            switch (actionBtn._state) {
            case "update":    actionBtn.updateClicked(); break
            case "installed": actionBtn.uninstallClicked(); break
            case "broken":    actionBtn.updateClicked(); break
            case "external":  Qt.openUrlExternally(actionBtn.vm.externalUrl); break
            default:          actionBtn.translateClicked(); break
            }
        }
    }
}
