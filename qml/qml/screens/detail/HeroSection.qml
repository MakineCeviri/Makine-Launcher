import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineAI 1.0
pragma ComponentBehavior: Bound

Item {
    id: heroRoot
    Layout.fillWidth: true
    Layout.preferredHeight: contribSection.y + contribSection.height + Dimensions.spacingMD

    // Single ViewModel reference — all state accessed via vm
    required property var vm

    signal translateClicked()
    signal uninstallClicked()

    // =========================================================================
    // HERO BANNER (full width, ~200px)
    // =========================================================================

    Rectangle {
        id: heroBanner
        width: parent.width
        height: 200
        color: Theme.surfaceActive

        Image {
            id: bannerImg
            anchors.fill: parent
            source: {
                if (heroRoot.vm.steamAppId !== "")
                    return "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.vm.steamAppId + "/library_hero.jpg"
                return heroRoot.vm.imageUrl
            }
            fillMode: Image.PreserveAspectCrop
            verticalAlignment: Image.AlignTop
            asynchronous: true
            opacity: status === Image.Ready ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        }

        // Bottom gradient fade into content area
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 180
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
        anchors.topMargin: -170
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginLG
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginLG
        spacing: Dimensions.spacingXL

        // ── LEFT COLUMN: Cover Art + Disclaimer ──
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: Dimensions.spacingLG

            // Cover frame
            Rectangle {
                id: coverFrame
                Layout.preferredWidth: 220
                Layout.preferredHeight: 310
                radius: Dimensions.radiusLG
                color: Theme.surfaceActive
                clip: true

                Image {
                    id: coverImg
                    anchors.fill: parent
                    source: heroRoot.vm.imageUrl !== ""
                        ? heroRoot.vm.imageUrl
                        : heroRoot.vm.steamAppId !== ""
                            ? "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.vm.steamAppId + "/library_600x900_2x.jpg"
                            : ""
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(440, 620)
                    asynchronous: true
                    opacity: status === Image.Ready ? 1.0 : 0
                    Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
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
                Layout.preferredWidth: 220
                text: qsTr("Bu yerelleştirme topluluk tarafından yapılmıştır ve resmi değildir.")
                font.pixelSize: Dimensions.fontCaption
                font.italic: true
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                opacity: 0.7
            }

        }

        // ── RIGHT COLUMN: Info + Action + About + Contributors ──
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Dimensions.spacingMD

            // Game logo (Steam) with text fallback
            Image {
                id: gameLogo
                Layout.fillWidth: true
                Layout.preferredHeight: status === Image.Ready ? implicitHeight * (width / implicitWidth) : 0
                Layout.maximumHeight: 80
                visible: status === Image.Ready
                source: heroRoot.vm.steamAppId !== ""
                    ? "https://cdn.akamai.steamstatic.com/steam/apps/" + heroRoot.vm.steamAppId + "/logo.png"
                    : ""
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignLeft
                sourceSize.width: 600
                asynchronous: true
            }

            // Game name (fallback when logo unavailable)
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: gameLogo.status !== Image.Ready
                text: heroRoot.vm.gameName
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
                visible: heroRoot.vm.isEditorsPick && heroRoot.vm.editorsNote !== ""
                text: "\u201C" + heroRoot.vm.editorsNote + "\u201D"
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
                visible: heroRoot.vm.hasTranslation && heroRoot.vm.isGameInstalled

                color: {
                    if (heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "broken")
                        return Theme.error
                    if (heroRoot.vm.installCompleted)
                        return Theme.accent
                    if (heroRoot.vm.packageInstalled)
                        return actionMouse.containsMouse ? "#8B2020" : Theme.accent
                    if (heroRoot.vm.isInstallingTranslation)
                        return "#3A3A3E"
                    // Default: gold/yellow download button
                    return actionMouse.containsMouse ? "#D4940C" : "#E5A00D"
                }
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                // Progress fill (during install)
                Rectangle {
                    id: progressFill
                    visible: heroRoot.vm.isInstallingTranslation && heroRoot.vm.installProgress > 0
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: Math.max(0, (parent.width - 4) * heroRoot.vm.installProgress)
                    radius: parent.radius - 2
                    color: Theme.accent18
                    Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                }

                // Shimmer during install
                Rectangle {
                    id: shimmerRect
                    visible: heroRoot.vm.isInstallingTranslation && heroRoot.vm.installProgress > 0
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
                            if (heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "broken")
                                return qsTr("ONARIM GEREKLİ")
                            if (heroRoot.vm.installCompleted)
                                return qsTr("Türkçe Yama Kuruldu \u2713")
                            if (heroRoot.vm.packageInstalled)
                                return actionMouse.containsMouse ? qsTr("Yamayı Kaldır") : qsTr("Türkçe Yama Kurulu \u2713")
                            if (heroRoot.vm.isInstallingTranslation) {
                                if (heroRoot.vm.isDownloading && heroRoot.vm.installStatus !== "")
                                    return heroRoot.vm.installStatus
                                if (heroRoot.vm.installProgress > 0)
                                    return qsTr("Kuruluyor... %1%").arg(heroRoot.vm.progressPercent)
                                return heroRoot.vm.installStatus || qsTr("Hazırlanıyor...")
                            }
                            return qsTr("TÜRKÇE YAMA İNDİR")
                        }
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Bold
                        font.letterSpacing: 0.5
                        color: {
                            if (heroRoot.vm.isInstallingTranslation)
                                return Theme.textPrimary
                            return Theme.textOnColor
                        }
                    }

                    // Cancel button (during install)
                    Rectangle {
                        visible: heroRoot.vm.isInstallingTranslation
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
                                if (heroRoot.vm.isDownloading)
                                    TranslationDownloader.cancelDownload(heroRoot.vm.gameId)
                                else
                                    GameService.cancelInstallation()
                            }
                        }
                    }
                }

                Accessible.role: heroRoot.vm.isInstallingTranslation ? Accessible.ProgressBar : Accessible.Button
                Accessible.name: heroRoot.vm.isInstallingTranslation
                    ? qsTr("Installing %1%").arg(heroRoot.vm.progressPercent)
                    : heroRoot.vm.packageInstalled ? qsTr("Installed") : qsTr("Download Turkish Patch")

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: (!heroRoot.vm.installCompleted && !heroRoot.vm.isInstallingTranslation)
                        ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: !heroRoot.vm.installCompleted && !heroRoot.vm.isInstallingTranslation
                    onClicked: {
                        if (heroRoot.vm.packageInstalled)
                            heroRoot.uninstallClicked()
                        else
                            heroRoot.translateClicked()
                    }
                }
            }

            // ── No translation notice (manual games) ──
            Rectangle {
                visible: heroRoot.vm.isManualGame && !heroRoot.vm.hasTranslation
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
                    visible: heroRoot.vm.verified
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
                    visible: heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "broken"
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
                    visible: heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "lost"
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
                    visible: heroRoot.vm.isEditorsPick
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

            // ── About ──
            AboutSection {
                Layout.fillWidth: true
                vm: heroRoot.vm
            }

        }
    }

    // =========================================================================
    // CONTRIBUTORS (full width, below two-column layout)
    // =========================================================================

    ContributorsSection {
        id: contribSection
        anchors.top: contentRow.bottom
        anchors.topMargin: Dimensions.spacingLG
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginLG
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginLG
        contributors: heroRoot.vm.contributors
    }
}
