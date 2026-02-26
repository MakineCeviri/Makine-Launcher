import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window
import MakineAI 1.0
pragma ComponentBehavior: Bound

Item {
    id: heroRoot
    Layout.fillWidth: true
    Layout.preferredHeight: 360

    // Required properties from parent
    required property string gameId
    required property string gameName
    required property string steamAppId
    required property string imageUrl
    required property bool verified
    required property string engine
    required property bool hasTranslation
    required property bool isEditorsPick
    required property string editorsNote
    required property bool isManualGame
    required property bool isGameInstalled
    required property bool packageInstalled
    required property bool isInstallingTranslation
    required property real installProgress
    required property string installStatus
    required property bool installCompleted
    property var updateImpact: null
    property bool isDownloading: false

    signal translateClicked()

    // Cover image (tall, left side, overlaps below)
    Rectangle {
        id: coverFrame
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginXL
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -40
        width: 240; height: 340
        radius: Dimensions.cardBorderRadius
        color: Theme.surfaceActive
        z: 2

        Image {
            id: coverImg
            anchors.fill: parent
            source: heroRoot.imageUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(440, 620)
            asynchronous: true
            visible: false
            property bool triedFallback: false
            onStatusChanged: {
                if (status === Image.Error && !triedFallback && heroRoot.steamAppId !== "") {
                    triedFallback = true
                    source = "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.steamAppId + "/header.jpg"
                }
            }
        }

        // Rounded mask
        Item {
            id: coverMask
            anchors.fill: parent; visible: false; layer.enabled: coverImg.status === Image.Ready
            Rectangle { anchors.fill: parent; radius: Dimensions.cardBorderRadius; color: "white" }
        }

        MultiEffect {
            id: coverEffect
            anchors.fill: parent
            source: coverImg
            maskEnabled: true; maskSource: coverMask
            opacity: coverImg.status === Image.Ready ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        }

        // Placeholder
        Text {
            textFormat: Text.PlainText
            anchors.centerIn: parent
            visible: coverImg.status !== Image.Ready
            text: heroRoot.gameName.length >= 2 ? heroRoot.gameName.substring(0, 2).toUpperCase() : "?"
            font.pixelSize: Dimensions.fontHero
            font.weight: Font.Bold
            color: Theme.textMuted
        }

        // Glass border (subtle, always visible)
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            color: "transparent"
            border.color: Theme.glassBorder; border.width: 1
        }
    }

    // Info column (right of cover, bottom-aligned)
    ColumnLayout {
        anchors.left: coverFrame.right
        anchors.leftMargin: Dimensions.marginLG
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginXL
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Dimensions.marginMD
        spacing: Dimensions.spacingLG

        // Badge row
        Row {
            spacing: Dimensions.spacingMD

            // Verified badge
            Rectangle {
                visible: heroRoot.verified
                width: verifiedRow.width + 20; height: 26
                radius: Dimensions.radiusFull
                color: Theme.verifiedBg

                Row {
                    id: verifiedRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingSM
                    Text {
                        textFormat: Text.PlainText
                        text: "\u2713"
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.verifiedText
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Onaylı Çeviri")
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.DemiBold
                        color: Theme.verifiedText
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Update impact badge
            Rectangle {
                visible: heroRoot.updateImpact && heroRoot.updateImpact.level === "broken"
                width: brokenRow.width + 20; height: 26
                radius: Dimensions.radiusFull
                color: Theme.withAlpha(Theme.error, 0.12)
                border.color: Theme.withAlpha(Theme.error, 0.25); border.width: 1

                Row {
                    id: brokenRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingSM
                    Text {
                        textFormat: Text.PlainText
                        text: "\u26A0"
                        font.pixelSize: Dimensions.fontCaption
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Güncelleme Gerekli")
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.DemiBold
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle {
                visible: heroRoot.updateImpact && heroRoot.updateImpact.level === "lost"
                width: lostRow.width + 20; height: 26
                radius: Dimensions.radiusFull
                color: Theme.withAlpha(Theme.warning, 0.12)
                border.color: Theme.withAlpha(Theme.warning, 0.25); border.width: 1

                Row {
                    id: lostRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingSM
                    Text {
                        textFormat: Text.PlainText
                        text: "\u26A0"
                        font.pixelSize: Dimensions.fontCaption
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Dosyalar Eksik")
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.DemiBold
                        color: Theme.warning
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Editor's pick badge
            Rectangle {
                visible: heroRoot.isEditorsPick
                width: editorsPickRow.width + 20; height: 26
                radius: Dimensions.radiusFull
                color: Theme.withAlpha(Theme.warning, 0.12)
                border.color: Theme.withAlpha(Theme.warning, 0.25); border.width: 1

                Row {
                    id: editorsPickRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingSM
                    Text {
                        textFormat: Text.PlainText
                        text: "\u2B50"
                        font.pixelSize: Dimensions.fontCaption
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Editörün Seçimi")
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.DemiBold
                        color: Theme.warning
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // Game name
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: heroRoot.gameName
            font.pixelSize: Dimensions.fontHero
            font.weight: Font.Bold
            font.letterSpacing: Dimensions.letterSpacingHeadline
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        // Editor's note (play recommendation)
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: heroRoot.isEditorsPick && heroRoot.editorsNote !== ""
            text: "\u201C" + heroRoot.editorsNote + "\u201D"
            font.pixelSize: Dimensions.fontBody
            font.italic: true
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            opacity: 0.85
        }

        // Action area
        RowLayout {
            spacing: Dimensions.spacingLG

            // ── Translation progress bar (installed games with translation) ──
            Rectangle {
                id: translationBar
                visible: heroRoot.isGameInstalled && heroRoot.hasTranslation
                Layout.preferredWidth: 280
                Layout.preferredHeight: 42
                radius: Dimensions.radiusStandard

                // Background: glass style
                color: {
                    if (heroRoot.packageInstalled || heroRoot.installCompleted)
                        return Theme.withAlpha(Theme.accent, 0.10)
                    if (heroRoot.isInstallingTranslation)
                        return Theme.withAlpha(Theme.accent, 0.06)
                    return Theme.withAlpha(Theme.textPrimary, 0.04)
                }
                border.color: {
                    if (heroRoot.packageInstalled || heroRoot.installCompleted)
                        return Theme.withAlpha(Theme.accent, 0.30)
                    if (heroRoot.isInstallingTranslation)
                        return Theme.withAlpha(Theme.accent, 0.20)
                    return Theme.withAlpha(Theme.textPrimary, 0.08)
                }
                border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animNormal } }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animNormal } }

                // Progress fill
                Rectangle {
                    id: progressFill
                    visible: heroRoot.isInstallingTranslation && heroRoot.installProgress > 0
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: Math.max(0, (parent.width - 4) * heroRoot.installProgress)
                    radius: parent.radius - 2
                    color: Theme.withAlpha(Theme.accent, 0.18)
                    Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                }

                // Shimmer effect during install
                // Keeps running even when window loses focus (user switches app),
                // stops only when minimized/hidden to save GPU.
                Rectangle {
                    id: shimmerRect
                    visible: heroRoot.isInstallingTranslation && heroRoot.installProgress > 0
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
                        onRunningChanged: {
                            if (typeof SceneProfiler !== "undefined")
                                SceneProfiler.registerAnimation("heroProgressShimmer", running)
                        }
                    }

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: Math.max(0, shimmerRect.shimmerPos - 0.15); color: "transparent" }
                        GradientStop { position: Math.max(0, Math.min(1, shimmerRect.shimmerPos)); color: Theme.withAlpha(Theme.accent, 0.15) }
                        GradientStop { position: Math.min(1, shimmerRect.shimmerPos + 0.15); color: "transparent" }
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14; anchors.rightMargin: 14
                    spacing: Dimensions.spacingSM

                    // Status icon (Canvas — matches nav icons style)
                    Canvas {
                        Layout.preferredWidth: 16; Layout.preferredHeight: 16
                        // Track icon type to avoid unnecessary repaints on same state
                        readonly property int _iconType: {
                            if (heroRoot.packageInstalled || heroRoot.installCompleted) return 2  // checkmark
                            if (heroRoot.isInstallingTranslation) return 1  // download
                            return 0  // globe
                        }
                        property color iconColor: {
                            if (heroRoot.packageInstalled || heroRoot.installCompleted)
                                return Theme.accent
                            if (heroRoot.isInstallingTranslation)
                                return Theme.accent
                            return Theme.textSecondary
                        }
                        on_IconTypeChanged: requestPaint()
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.6
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"

                            if (heroRoot.packageInstalled || heroRoot.installCompleted) {
                                // Checkmark icon
                                ctx.beginPath()
                                ctx.moveTo(3, 8); ctx.lineTo(6.5, 11.5); ctx.lineTo(13, 4.5)
                                ctx.stroke()
                            } else if (heroRoot.isInstallingTranslation) {
                                // Download arrow icon
                                ctx.beginPath()
                                ctx.moveTo(8, 2); ctx.lineTo(8, 10); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(5, 7.5); ctx.lineTo(8, 11); ctx.lineTo(11, 7.5); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(3, 13); ctx.lineTo(13, 13); ctx.stroke()
                            } else {
                                // Globe/translate icon
                                ctx.beginPath()
                                ctx.arc(8, 8, 6, 0, Math.PI * 2); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(2, 8); ctx.lineTo(14, 8); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(8, 2); ctx.quadraticCurveTo(5, 8, 8, 14); ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(8, 2); ctx.quadraticCurveTo(11, 8, 8, 14); ctx.stroke()
                            }
                        }
                    }

                    // Status text
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: {
                            if (heroRoot.updateImpact && heroRoot.updateImpact.level === "broken")
                                return qsTr("Çeviri Bozulmuş — Onarım Gerekli")
                            if (heroRoot.updateImpact && heroRoot.updateImpact.level === "lost")
                                return qsTr("Bazı Dosyalar Eksik")
                            if (heroRoot.packageInstalled || heroRoot.installCompleted)
                                return qsTr("Türkçe Yama Kurulu")
                            if (heroRoot.isInstallingTranslation) {
                                if (heroRoot.isDownloading && heroRoot.installStatus !== "")
                                    return heroRoot.installStatus
                                if (heroRoot.installProgress > 0)
                                    return qsTr("Kuruluyor... %1%").arg(Math.round(heroRoot.installProgress * 100))
                                return heroRoot.installStatus || qsTr("Hazırlanıyor...")
                            }
                            return qsTr("Türkçe Yama Mevcut")
                        }
                        font.pixelSize: Dimensions.fontSM
                        font.weight: (heroRoot.packageInstalled || heroRoot.installCompleted)
                            ? Font.DemiBold : Font.Medium
                        color: {
                            if (heroRoot.updateImpact && heroRoot.updateImpact.level === "broken")
                                return Theme.error
                            if (heroRoot.updateImpact && heroRoot.updateImpact.level === "lost")
                                return Theme.warning
                            if (heroRoot.packageInstalled || heroRoot.installCompleted)
                                return Theme.accent
                            if (heroRoot.isInstallingTranslation)
                                return Theme.textPrimary
                            return Theme.textSecondary
                        }
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    // Percentage badge (during install)
                    Rectangle {
                        visible: heroRoot.isInstallingTranslation && heroRoot.installProgress > 0
                        Layout.preferredWidth: pctText.width + 12
                        Layout.preferredHeight: 22
                        radius: 11
                        color: Theme.withAlpha(Theme.accent, 0.15)
                        border.color: Theme.withAlpha(Theme.accent, 0.25)
                        border.width: 1

                        Text {
                            textFormat: Text.PlainText
                            id: pctText
                            anchors.centerIn: parent
                            text: qsTr("%1%").arg(Math.round(heroRoot.installProgress * 100))
                            font.pixelSize: Dimensions.fontMicro
                            font.weight: Font.Bold
                            color: Theme.accent
                        }
                    }

                    // Cancel button (during install)
                    Rectangle {
                        visible: heroRoot.isInstallingTranslation
                        Layout.preferredWidth: 22; Layout.preferredHeight: 22
                        radius: 11
                        color: cancelMouse.containsMouse
                            ? Theme.withAlpha(Theme.error, 0.20)
                            : Theme.withAlpha(Theme.textMuted, 0.10)
                        border.color: cancelMouse.containsMouse
                            ? Theme.withAlpha(Theme.error, 0.40)
                            : Theme.withAlpha(Theme.textMuted, 0.15)
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Kurulumu iptal et")

                        Text {
                            textFormat: Text.PlainText
                            anchors.centerIn: parent
                            text: "\u2715"
                            font.pixelSize: Dimensions.fontMicro
                            font.weight: Font.Bold
                            color: cancelMouse.containsMouse ? Theme.error : Theme.textMuted
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        }
                        MouseArea {
                            id: cancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (heroRoot.isDownloading)
                                    TranslationDownloader.cancelDownload(heroRoot.gameId)
                                else
                                    GameService.cancelInstallation()
                            }
                        }
                    }
                }

                Accessible.role: Accessible.ProgressBar
                Accessible.name: heroRoot.isInstallingTranslation
                    ? qsTr("Installing %1%").arg(Math.round(heroRoot.installProgress * 100))
                    : heroRoot.packageInstalled ? qsTr("Installed") : qsTr("Available")

                // Click to install if not yet installed/installing
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: (!heroRoot.packageInstalled && !heroRoot.installCompleted && !heroRoot.isInstallingTranslation)
                        ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: !heroRoot.packageInstalled && !heroRoot.installCompleted && !heroRoot.isInstallingTranslation
                    onClicked: heroRoot.translateClicked()
                }
            }

            // ── No translation notice (manual games) ──
            Rectangle {
                visible: heroRoot.isManualGame && !heroRoot.hasTranslation
                implicitWidth: noTransRow.width + 36; implicitHeight: 42
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.textMuted, 0.08)
                border.color: Theme.withAlpha(Theme.textMuted, 0.15); border.width: 1

                Row {
                    id: noTransRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                    Text {
                        textFormat: Text.PlainText
                        text: "\u26A0"
                        font.pixelSize: Dimensions.fontMD; color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Bu oyun için Türkçe yama mevcut değil")
                        font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                        color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Steam link (only for catalog-only games, hidden for installed+translation) ──
            Rectangle {
                id: steamBtn
                visible: heroRoot.steamAppId !== "" && !(heroRoot.isGameInstalled && heroRoot.hasTranslation)
                implicitWidth: steamContent.width + 28; implicitHeight: 42
                radius: Dimensions.radiusStandard
                color: steamMouse.containsMouse ? "#2a475e" : "#1b2838"
                border.color: steamMouse.containsMouse
                    ? Theme.withAlpha("#66c0f4", 0.50)
                    : Theme.withAlpha("#66c0f4", 0.20)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 180 } }
                Behavior on border.color { ColorAnimation { duration: 180 } }

                Accessible.role: Accessible.Link
                Accessible.name: qsTr("Steam'de Aç")

                Row {
                    id: steamContent; anchors.centerIn: parent; spacing: 8

                    // Steam logo (real SVG)
                    Image {
                        id: steamSvgSrc
                        source: "qrc:/qt/qml/MakineAI/resources/icons/steam.svg"
                        sourceSize: Qt.size(20, 20)
                        width: 20; height: 20
                        asynchronous: true
                        anchors.verticalCenter: parent.verticalCenter
                        visible: false
                    }
                    MultiEffect {
                        source: steamSvgSrc
                        width: 20; height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        brightness: steamMouse.containsMouse ? 0.35 : 0.0
                        Behavior on brightness { NumberAnimation { duration: 180 } }
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: "Steam"
                        font.pixelSize: Dimensions.fontMD; font.weight: Font.DemiBold
                        color: steamMouse.containsMouse ? "#c5e8ff" : "#66c0f4"
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 180 } }
                    }
                }

                MouseArea {
                    id: steamMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally("https://store.steampowered.com/app/" + heroRoot.steamAppId)
                }
            }

        }
    }
}
