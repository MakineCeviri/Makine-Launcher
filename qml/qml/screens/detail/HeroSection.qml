// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * HeroSection — the top of a game's page, Apple TV style: the game's art on
 * a large rounded card with its logo, what it is and the patch's action on
 * it; under the card what that action will do, where the game is, what went
 * wrong (if anything), the game's facts, the patch notes and who made it.
 */
ColumnLayout {
    id: heroRoot
    // As wide as its card; centred on the page
    Layout.alignment: Qt.AlignHCenter

    // Single ViewModel reference — all state accessed via vm
    required property var vm

    signal translateClicked()
    signal updateClicked()
    signal uninstallClicked()

    // A folder dropped on the page or picked elsewhere: GameFolderBar takes it.
    function inspectFolder(url) {
        folderBar.inspect(url, "change")
    }

    // A new game fades in rather than swapping under the pointer
    function replayEntryAnim() {
        entry.stop()
        if (!Dimensions.animNormal) { heroRoot.opacity = 1; return }
        heroRoot.opacity = 0
        entry.start()
    }
    NumberAnimation {
        id: entry
        target: heroRoot
        property: "opacity"
        from: 0; to: 1
        duration: Dimensions.animSlow
        easing.type: Easing.OutCubic
    }

    // Cached binding — vm.updateImpact.level evaluated once, not per-consumer
    readonly property string _impactLevel: vm.updateImpact ? vm.updateImpact.level : ""
    readonly property string _year: {
        const m = /(\d{4})/.exec(heroRoot.vm.releaseDate || "")
        return m ? m[1] : ""
    }
    readonly property string _genre: {
        const g = heroRoot.vm.genresText || ""
        const comma = g.indexOf(",")
        return comma > 0 ? g.substring(0, comma).trim() : g
    }
    readonly property string _meta: [heroRoot.vm.developersText, heroRoot._genre, heroRoot._year]
                                    .filter(s => s && s !== "").join("  ·  ")
    // From the page, not from this layout: the layout's own width follows its
    // children's maximum, and a width derived from it shrank on every pass
    // (a layout loop that froze the page).
    readonly property real _pageWidth: heroRoot.parent ? heroRoot.parent.width : 0
    readonly property real _width: Math.max(0, Math.min(_pageWidth - Dimensions.pageMargin * 2, 1240))

    spacing: 26

    // Reset CDN fallback flags when game changes
    Connections {
        target: heroRoot.vm
        function onSteamAppIdChanged() {
            bannerImg._useCdnFallback = false
            gameLogo._useCdnFallback = false
            gameLogo._aspect = 0
        }
    }

    // =========================================================================
    // THE CARD — art, identity, action
    // =========================================================================

    Item {
        id: card
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 52
        Layout.fillWidth: false
        Layout.preferredWidth: heroRoot._width
        Layout.preferredHeight: Math.round(Math.max(320, Math.min(470, heroRoot._width * 0.44)))

        RectangularShadow {
            anchors.fill: art
            radius: Dimensions.radiusHero
            offset.y: 10
            blur: 34
            spread: -8
            color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.55 : 0.2)
        }

        Item {
            id: art
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: artMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }

            Rectangle { anchors.fill: parent; color: Theme.surface }

            Image {
                id: bannerImg
                anchors.fill: parent
                property bool _useCdnFallback: false
                source: _useCdnFallback
                    ? "https://cdn.makineceviri.org/assets/banners/" + heroRoot.vm.steamAppId + ".jpg"
                    : heroRoot.vm.heroUrl
                fillMode: Image.PreserveAspectCrop
                verticalAlignment: Image.AlignTop
                sourceSize.width: 1600
                asynchronous: true
                opacity: status === Image.Ready ? 1.0 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                onStatusChanged: {
                    if (status === Image.Error && !_useCdnFallback && heroRoot.vm.steamAppId !== "")
                        _useCdnFallback = true
                }
            }

            // Legibility: a dark foot and a dark left side for the white text
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.25; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.82) }
                }
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.5) }
                    GradientStop { position: 0.6; color: "transparent" }
                }
            }
        }

        Item {
            id: artMask
            anchors.fill: parent
            visible: false
            layer.enabled: true
            Rectangle { anchors.fill: parent; radius: Dimensions.radiusHero; color: "white" }
        }
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusHero
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.08 : 0)
        }

        // Badges over the art, top left: what the user must know first
        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 8

            StatusBadge {
                onArt: true
                visible: !heroRoot.vm.isGameInstalled && heroRoot.vm.hasTranslation
                         && heroRoot.vm.externalUrl === ""
                tint: "#FF9F0A"
                text: qsTr("Bilgisayarında bulunamadı")
            }
            StatusBadge {
                onArt: true
                visible: heroRoot._impactLevel === "broken"
                tint: "#FF453A"
                text: qsTr("Oyun güncellendi · yama bozuldu")
            }
            StatusBadge {
                onArt: true
                visible: heroRoot._impactLevel === "lost"
                tint: "#FF9F0A"
                text: qsTr("Yama dosyaları eksik")
            }
            StatusBadge {
                onArt: true
                visible: heroRoot.vm.isEditorsPick
                tint: "#FFD60A"
                text: qsTr("Editörün seçimi")
            }
        }

        // Identity and action, bottom left
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            anchors.bottomMargin: 28
            spacing: 10

            // Logo when Steam has one, the name otherwise
            Item {
                id: gameIdentity
                Layout.fillWidth: true
                readonly property real _maxH: Math.max(64, Math.min(120, card.height * 0.26))
                Layout.preferredHeight: gameLogo.ready
                    ? Math.min(_maxH, Math.min(width * 0.5, 460) * gameLogo._aspect)
                    : gameTitle.implicitHeight

                Text {
                    id: gameTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: !gameLogo.ready
                    textFormat: Text.PlainText
                    text: heroRoot.vm.gameName
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: card.height > 380 ? 40 : 32
                    font.weight: Font.Bold
                    font.letterSpacing: -0.8
                    lineHeight: 1.02
                    color: "#FFFFFF"
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Image {
                    id: gameLogo
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    height: parent.height
                    width: Math.min(parent.width * 0.5, 460)
                    property real _aspect: 0
                    property bool ready: _aspect > 0
                    property bool _useCdnFallback: false
                    onStatusChanged: {
                        if (status === Image.Ready && implicitWidth > 0)
                            _aspect = implicitHeight / implicitWidth
                        if (status === Image.Error && !_useCdnFallback && heroRoot.vm.steamAppId !== "")
                            _useCdnFallback = true
                    }
                    visible: ready
                    source: _useCdnFallback
                        ? "https://cdn.makineceviri.org/assets/banners/" + heroRoot.vm.steamAppId + "_logo.png"
                        : heroRoot.vm.logoUrl
                    asynchronous: true
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    verticalAlignment: Image.AlignBottom
                    sourceSize.width: 600
                    layer.enabled: ready
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: "#000000"
                        shadowBlur: 0.6
                        shadowOpacity: 0.55
                        shadowVerticalOffset: 2
                        shadowHorizontalOffset: 0
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: heroRoot._meta !== ""
                textFormat: Text.PlainText
                text: heroRoot._meta
                elide: Text.ElideRight
                font.pixelSize: Dimensions.callout
                font.weight: Font.Medium
                color: Qt.rgba(1, 1, 1, 0.78)
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 640
                visible: heroRoot.vm.isEditorsPick && heroRoot.vm.editorsNote !== ""
                textFormat: Text.PlainText
                text: "“" + heroRoot.vm.editorsNote + "”"
                font.pixelSize: Dimensions.callout
                font.italic: true
                color: Qt.rgba(1, 1, 1, 0.75)
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            TranslationActionButton {
                id: actionButton
                Layout.topMargin: 8
                vm: heroRoot.vm
                onLocateClicked: folderBar.pick("install")
                onTranslateClicked: heroRoot.translateClicked()
                onUpdateClicked: heroRoot.updateClicked()
                onUninstallClicked: heroRoot.uninstallClicked()
                // "lost" = patch files gone from disk; a store verify cannot
                // bring them back, a reinstall does (same as UpdateBanner).
                onRepairClicked: GameService.recoverTranslation(heroRoot.vm.gameId)
            }

            // No Turkish patch for a game added by hand
            StatusBadge {
                Layout.topMargin: 6
                onArt: true
                visible: heroRoot.vm.isManualGame && !heroRoot.vm.hasTranslation
                tint: Theme.textSecondary
                text: qsTr("Bu oyun için henüz Türkçe yama yok")
            }
        }
    }

    // =========================================================================
    // UNDER THE CARD
    // =========================================================================

    ColumnLayout {
        Layout.alignment: Qt.AlignHCenter
        // Its children fill it; it must not fill the page in turn.
        Layout.fillWidth: false
        Layout.preferredWidth: heroRoot._width
        spacing: 18

        // What the button above is about to do
        InstallExplainer {
            Layout.fillWidth: true
            vm: heroRoot.vm
            mode: actionButton._state
        }

        // A game update broke the patch, or its files are gone
        UpdateBanner {
            vm: heroRoot.vm
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.topMargin: 0
        }

        // What a patch's own installer prints, and the answers to it
        ProgramConsole {
            vm: heroRoot.vm
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.topMargin: 0
        }

        // Where the game is — or, not found, how to point at it
        GameFolderBar {
            id: folderBar
            Layout.fillWidth: true
            vm: heroRoot.vm
            onInstallRequested: heroRoot.translateClicked()
        }

        // What went wrong, and the way out
        Rectangle {
            visible: heroRoot.vm.installErrorMessage !== ""
            Layout.fillWidth: true
            implicitHeight: errCol.implicitHeight + 32
            radius: Dimensions.radiusLG
            color: Theme.withAlpha(Theme.error, Theme.darkMode ? 0.12 : 0.07)
            border.color: Theme.withAlpha(Theme.error, 0.3)
            border.width: 1

            RowLayout {
                id: errCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                IconTile {
                    Layout.alignment: Qt.AlignTop
                    size: 34
                    glyph: Icons.circleAlert
                    tint: Theme.error
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: qsTr("Bir sorun çıktı")
                        font.pixelSize: Dimensions.headline
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    // Selectable: the message may carry a folder path or an
                    // error the user is asked to pass on to us.
                    TextEdit {
                        Layout.fillWidth: true
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.PlainText
                        text: heroRoot.vm.installErrorMessage
                        font.pixelSize: Dimensions.callout
                        color: Theme.textSecondary
                        selectionColor: Theme.primary40
                        wrapMode: TextEdit.WordWrap
                    }
                }
                // Store verification: when the patch left the game in a mixed
                // state with no backup to restore, letting the store fetch its
                // own files again is the recovery that always works.
                // Only while a record says the game was changed: after a failed
                // install that left nothing behind there is nothing to repair.
                AppButton {
                    Layout.alignment: Qt.AlignVCenter
                    visible: !heroRoot.vm.installErrorOnFolder && heroRoot.vm.packageInstalled
                    style: "gray"
                    icon: Icons.wrench
                    text: qsTr("Oyun dosyalarını doğrula")
                    toolTip: qsTr("Steam'de oyunun dosyalarını doğrulatır; başka mağazalar için adımları gösterir")
                    onClicked: GameService.repairGameFiles(heroRoot.vm.gameId)
                }
                // The folder was not the game's: nothing was touched, and the
                // way out is the right folder, not a repair.
                AppButton {
                    Layout.alignment: Qt.AlignVCenter
                    visible: heroRoot.vm.installErrorOnFolder
                    style: "gray"
                    icon: Icons.folderSearch
                    text: qsTr("Klasörü değiştir")
                    toolTip: qsTr("Oyunun kendi klasörünü seç; yama oraya kurulur")
                    onClicked: folderBar.pick("install")
                }
            }
        }

        InfoTilesBar {
            Layout.fillWidth: true
            Layout.topMargin: 6
            vm: heroRoot.vm
        }

        AboutSection {
            Layout.fillWidth: true
            vm: heroRoot.vm
        }

        ContributorsSection {
            Layout.fillWidth: true
            contributors: heroRoot.vm.contributors
            externalUrl: heroRoot.vm.externalUrl
            isApex: heroRoot.vm.isApex
            isHangar: heroRoot.vm.isHangar
            apexTier: heroRoot.vm.apexTier
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: qsTr("Bu yerelleştirme topluluk tarafından yapılmıştır ve resmi değildir.")
            font.pixelSize: Dimensions.footnote
            color: Theme.textTertiary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
