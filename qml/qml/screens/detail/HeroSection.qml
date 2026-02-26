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
    Layout.preferredHeight: heroBanner.height + contentRow.height + Dimensions.marginLG

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
    property var screenshots: []

    // Throttle: integer percent reduces text binding updates from ~6000x to ~100x
    readonly property int _progressPct: Math.round(installProgress * 100)

    signal translateClicked()

    // =========================================================================
    // HERO BANNER (full width, ~250px)
    // =========================================================================

    Rectangle {
        id: heroBanner
        width: parent.width
        height: 250
        color: Theme.surfaceActive

        Image {
            id: bannerImg
            anchors.fill: parent
            source: {
                if (heroRoot.steamAppId !== "")
                    return "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.steamAppId + "/library_hero.jpg"
                return heroRoot.imageUrl
            }
            fillMode: Image.PreserveAspectCrop
            verticalAlignment: Image.AlignTop
            asynchronous: true
            opacity: status === Image.Ready ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

            property int _fallbackStage: 0
            onStatusChanged: {
                if (status === Image.Error && heroRoot.steamAppId !== "") {
                    if (_fallbackStage === 0) {
                        _fallbackStage = 1
                        source = "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.steamAppId + "/header.jpg"
                    } else if (_fallbackStage === 1) {
                        _fallbackStage = 2
                        source = heroRoot.imageUrl
                    }
                }
            }
        }

        // Bottom gradient fade into content area
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 120
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.bgPrimary }
            }
        }
    }

    // =========================================================================
    // TWO-COLUMN CONTENT (cover left, info right)
    // =========================================================================

    RowLayout {
        id: contentRow
        anchors.top: heroBanner.bottom
        anchors.topMargin: -60
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginXL
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginXL
        spacing: Dimensions.spacingPage

        // ── LEFT COLUMN: Cover Art ──
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: Dimensions.spacingLG

            // Cover frame
            Rectangle {
                id: coverFrame
                Layout.preferredWidth: 280
                Layout.preferredHeight: 392
                radius: Dimensions.radiusLG
                color: Theme.surfaceActive

                Image {
                    id: coverImg
                    anchors.fill: parent
                    source: heroRoot.imageUrl
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(560, 784)
                    asynchronous: true
                    visible: false

                    property int _fallbackStage: 0
                    onStatusChanged: {
                        if (status === Image.Error && heroRoot.steamAppId !== "") {
                            if (_fallbackStage === 0) {
                                _fallbackStage = 1
                                source = "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.steamAppId + "/library_600x900_2x.jpg"
                            } else if (_fallbackStage === 1) {
                                _fallbackStage = 2
                                source = "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.steamAppId + "/header.jpg"
                            }
                        }
                    }
                }

                // Rounded mask
                Item {
                    id: coverMask
                    anchors.fill: parent; visible: false; layer.enabled: coverImg.status === Image.Ready
                    Rectangle { anchors.fill: parent; radius: Dimensions.radiusLG; color: "white" }
                }

                MultiEffect {
                    anchors.fill: parent
                    source: coverImg
                    maskEnabled: true; maskSource: coverMask
                    opacity: coverImg.status === Image.Ready ? 1.0 : 0
                    Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                }

                // Placeholder initials
                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    visible: coverImg.status !== Image.Ready
                    text: heroRoot.gameName.length >= 2 ? heroRoot.gameName.substring(0, 2).toUpperCase() : "?"
                    font.pixelSize: Dimensions.fontBanner
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }

                // Glass border
                Rectangle {
                    anchors.fill: parent; radius: parent.radius
                    color: "transparent"
                    border.color: Theme.glassBorder; border.width: 1
                }
            }

            // Community disclaimer
            Text {
                textFormat: Text.PlainText
                Layout.preferredWidth: 280
                text: qsTr("Bu yerelleştirme topluluk tarafından yapılmıştır ve resmi değildir.")
                font.pixelSize: Dimensions.fontCaption
                font.italic: true
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }

        // ── RIGHT COLUMN: Info + Action + Screenshots ──
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Dimensions.spacingXL

            // Game name
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: heroRoot.gameName
                font.pixelSize: Dimensions.fontBanner
                font.weight: Font.Bold
                font.letterSpacing: Dimensions.letterSpacingHeadline
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            // Editor's note
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

            // ── ACTION BUTTON (full width, 48px) ──
            Rectangle {
                id: actionBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Dimensions.radiusMD
                visible: heroRoot.hasTranslation && heroRoot.isGameInstalled

                color: {
                    if (heroRoot.updateImpact && heroRoot.updateImpact.level === "broken")
                        return Theme.error
                    if (heroRoot.packageInstalled || heroRoot.installCompleted)
                        return Theme.accent
                    if (heroRoot.isInstallingTranslation)
                        return "#3A3A3E"
                    // Default: gold/yellow download button
                    return actionMouse.containsMouse ? "#D4940C" : "#E5A00D"
                }
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                // Progress fill (during install)
                Rectangle {
                    id: progressFill
                    visible: heroRoot.isInstallingTranslation && heroRoot.installProgress > 0
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: Math.max(0, (parent.width - 4) * heroRoot.installProgress)
                    radius: parent.radius - 2
                    color: Theme.accent18
                    Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                }

                // Shimmer during install
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
                            if (heroRoot.updateImpact && heroRoot.updateImpact.level === "broken")
                                return qsTr("ONARIM GEREKLİ")
                            if (heroRoot.packageInstalled || heroRoot.installCompleted)
                                return qsTr("Türkçe Yama Kurulu \u2713")
                            if (heroRoot.isInstallingTranslation) {
                                if (heroRoot.isDownloading && heroRoot.installStatus !== "")
                                    return heroRoot.installStatus
                                if (heroRoot.installProgress > 0)
                                    return qsTr("Kuruluyor... %1%").arg(heroRoot._progressPct)
                                return heroRoot.installStatus || qsTr("Hazırlanıyor...")
                            }
                            return qsTr("TÜRKÇE YAMA İNDİR")
                        }
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Bold
                        font.letterSpacing: 0.5
                        color: {
                            if (heroRoot.isInstallingTranslation)
                                return Theme.textPrimary
                            return Theme.textOnColor
                        }
                    }

                    // Cancel button (during install)
                    Rectangle {
                        visible: heroRoot.isInstallingTranslation
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
                                if (heroRoot.isDownloading)
                                    TranslationDownloader.cancelDownload(heroRoot.gameId)
                                else
                                    GameService.cancelInstallation()
                            }
                        }
                    }
                }

                Accessible.role: heroRoot.isInstallingTranslation ? Accessible.ProgressBar : Accessible.Button
                Accessible.name: heroRoot.isInstallingTranslation
                    ? qsTr("Installing %1%").arg(heroRoot._progressPct)
                    : heroRoot.packageInstalled ? qsTr("Installed") : qsTr("Download Turkish Patch")

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: (!heroRoot.packageInstalled && !heroRoot.installCompleted && !heroRoot.isInstallingTranslation)
                        ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: !heroRoot.packageInstalled && !heroRoot.installCompleted && !heroRoot.isInstallingTranslation
                    onClicked: heroRoot.translateClicked()
                }
            }

            // ── No translation notice (manual games) ──
            Rectangle {
                visible: heroRoot.isManualGame && !heroRoot.hasTranslation
                Layout.fillWidth: true
                implicitHeight: 48
                radius: Dimensions.radiusMD
                color: Theme.textMuted08
                border.color: Theme.textMuted15; border.width: 1

                Row {
                    anchors.centerIn: parent; spacing: Dimensions.spacingMD
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

            // ── Badge row ──
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

                // Update impact: broken
                Rectangle {
                    visible: heroRoot.updateImpact && heroRoot.updateImpact.level === "broken"
                    width: brokenRow.width + 20; height: 26
                    radius: Dimensions.radiusFull
                    color: Theme.error12
                    border.color: Theme.error25; border.width: 1

                    Row {
                        id: brokenRow
                        anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        Text {
                            textFormat: Text.PlainText; text: "\u26A0"
                            font.pixelSize: Dimensions.fontCaption
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Güncelleme Gerekli")
                            font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold
                            color: Theme.error
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Update impact: lost
                Rectangle {
                    visible: heroRoot.updateImpact && heroRoot.updateImpact.level === "lost"
                    width: lostRow.width + 20; height: 26
                    radius: Dimensions.radiusFull
                    color: Theme.warning12
                    border.color: Theme.warning25; border.width: 1

                    Row {
                        id: lostRow
                        anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        Text {
                            textFormat: Text.PlainText; text: "\u26A0"
                            font.pixelSize: Dimensions.fontCaption
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Dosyalar Eksik")
                            font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold
                            color: Theme.warning
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Editor's pick
                Rectangle {
                    visible: heroRoot.isEditorsPick
                    width: editorsPickRow.width + 20; height: 26
                    radius: Dimensions.radiusFull
                    color: Theme.warning12
                    border.color: Theme.warning25; border.width: 1

                    Row {
                        id: editorsPickRow
                        anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        Text {
                            textFormat: Text.PlainText; text: "\u2B50"
                            font.pixelSize: Dimensions.fontCaption
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Editörün Seçimi")
                            font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold
                            color: Theme.warning
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ── Screenshots (inline, reuses ScreenshotCarousel) ──
            Loader {
                id: screenshotsInHero
                Layout.fillWidth: true
                active: heroRoot.screenshots.length > 0
                sourceComponent: ScreenshotCarousel {
                    screenshots: heroRoot.screenshots
                }
            }
        }
    }
}
