import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * TranslationActionButton — Main CTA for translation install/uninstall.
 *
 * States: download (gold), installing (dark + progress/shimmer),
 *         installed (accent + uninstall hover), completed (accent flash),
 *         broken (error red).
 */
Rectangle {
    id: actionBtn

    required property var vm

    signal translateClicked()
    signal uninstallClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 48
    radius: Dimensions.radiusMD
    visible: actionBtn.vm.hasTranslation && actionBtn.vm.isGameInstalled && actionBtn.vm.fromLibrary

    color: {
        if (actionBtn.vm.updateImpact && actionBtn.vm.updateImpact.level === "broken")
            return Theme.error
        if (actionBtn.vm.installCompleted)
            return Theme.accent
        if (actionBtn.vm.packageInstalled)
            return actionMouse.containsMouse ? "#8B2020" : Theme.accent
        if (actionBtn.vm.isInstallingTranslation)
            return "#3A3A3E"
        // Default: download button
        return actionMouse.containsMouse ? Theme.primaryHover : Theme.primary
    }
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    // Progress fill (during install)
    Rectangle {
        id: progressFill
        visible: actionBtn.vm.isInstallingTranslation && actionBtn.vm.installProgress > 0
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.margins: 2
        width: Math.max(0, (parent.width - 4) * actionBtn.vm.installProgress)
        radius: parent.radius - 2
        color: Theme.accent18
        Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
    }

    // Shimmer during install
    Rectangle {
        id: shimmerRect
        visible: actionBtn.vm.isInstallingTranslation && actionBtn.vm.installProgress > 0
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.margins: 2
        width: progressFill.width
        radius: parent.radius - 2

        property real shimmerPos: 0
        NumberAnimation on shimmerPos {
            running: shimmerRect.visible
                     && Window.window !== null
                     && Window.window.visibility !== Window.Minimized
                     && Window.window.visibility !== Window.Hidden
            from: -0.3; to: 1.3; duration: Dimensions.animLoadingCycle
            loops: Animation.Infinite
        }

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: Math.max(0, shimmerRect.shimmerPos - 0.15); color: "transparent" }
            GradientStop { position: Math.max(0, Math.min(1, shimmerRect.shimmerPos)); color: Theme.accent15 }
            GradientStop { position: Math.min(1, shimmerRect.shimmerPos + 0.15); color: "transparent" }
        }
    }

    // Button content
    RowLayout {
        anchors.centerIn: parent
        spacing: Dimensions.spacingMD

        // Status text
        Text {
            textFormat: Text.PlainText
            text: {
                if (actionBtn.vm.updateImpact && actionBtn.vm.updateImpact.level === "broken")
                    return qsTr("ONARIM GEREKLİ")
                if (actionBtn.vm.installCompleted)
                    return qsTr("Türkçe Yama Kuruldu \u2713")
                if (actionBtn.vm.packageInstalled)
                    return actionMouse.containsMouse ? qsTr("Yamayı Kaldır") : qsTr("Türkçe Yama Kurulu \u2713")
                if (actionBtn.vm.isInstallingTranslation) {
                    if (actionBtn.vm.isDownloading && actionBtn.vm.installStatus !== "")
                        return actionBtn.vm.installStatus
                    if (actionBtn.vm.installProgress > 0)
                        return qsTr("Kuruluyor... %1%").arg(actionBtn.vm.progressPercent)
                    return actionBtn.vm.installStatus || qsTr("Hazırlanıyor...")
                }
                return qsTr("TÜRKÇE YAMA İNDİR")
            }
            font.pixelSize: Dimensions.fontMD
            font.weight: Font.Bold
            font.letterSpacing: 0.5
            color: actionBtn.vm.isInstallingTranslation ? Theme.textPrimary : Theme.textOnColor
        }

        // Cancel button (during install)
        Rectangle {
            visible: actionBtn.vm.isInstallingTranslation
            width: 26; height: 26; radius: 13
            color: cancelMouse.containsMouse ? Theme.error20 : Theme.textMuted10
            border.color: cancelMouse.containsMouse ? Theme.error40 : Theme.textMuted15
            border.width: 1
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Kurulumu iptal et")

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2715"
                font.pixelSize: Dimensions.fontMicro
                font.weight: Font.Bold
                color: cancelMouse.containsMouse ? Theme.error : Theme.textMuted
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

    Accessible.role: actionBtn.vm.isInstallingTranslation ? Accessible.ProgressBar : Accessible.Button
    Accessible.name: actionBtn.vm.isInstallingTranslation
        ? qsTr("Installing %1%").arg(actionBtn.vm.progressPercent)
        : actionBtn.vm.packageInstalled ? qsTr("Installed") : qsTr("Download Turkish Patch")

    MouseArea {
        id: actionMouse
        anchors.fill: parent; hoverEnabled: true
        cursorShape: (!actionBtn.vm.installCompleted && !actionBtn.vm.isInstallingTranslation)
            ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: !actionBtn.vm.installCompleted && !actionBtn.vm.isInstallingTranslation
        onClicked: {
            if (actionBtn.vm.packageInstalled)
                actionBtn.uninstallClicked()
            else
                actionBtn.translateClicked()
        }
    }
}
