import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Item {
    id: heroRoot
    Layout.fillWidth: true
    Layout.preferredHeight: mainColumn.y + mainColumn.height + Dimensions.spacingLG

    // Single ViewModel reference — all state accessed via vm
    required property var vm

    signal translateClicked()
    signal updateClicked()
    signal uninstallClicked()

    // =========================================================================
    // STAGGERED ENTRY ANIMATION STATE
    // =========================================================================

    property real _heroOp: 0
    property real _titleOp: 0;  property real _titleTY: 18
    property real _actionOp: 0; property real _actionTY: 18
    property real _tilesOp: 0;  property real _tilesTY: 18
    property real _aboutOp: 0;  property real _aboutTY: 18
    property real _contribOp: 0; property real _contribTY: 18

    // Cached binding — vm.updateImpact.level evaluated once, not per-consumer
    readonly property string _impactLevel: vm.updateImpact ? vm.updateImpact.level : ""

    function replayEntryAnim() {
        _entrySeq.stop()
        if (!Dimensions.animNormal) {
            _heroOp = 1; _titleOp = 1; _titleTY = 0
            _actionOp = 1; _actionTY = 0; _tilesOp = 1; _tilesTY = 0
            _aboutOp = 1; _aboutTY = 0; _contribOp = 1; _contribTY = 0
            return
        }
        _heroOp = 0; _titleOp = 0; _titleTY = 18
        _actionOp = 0; _actionTY = 18; _tilesOp = 0; _tilesTY = 18
        _aboutOp = 0; _aboutTY = 18; _contribOp = 0; _contribTY = 18
        _entrySeq.start()
    }

    ParallelAnimation {
        id: _entrySeq

        // Hero banner: 0ms delay, 300ms fade
        NumberAnimation { target: heroRoot; property: "_heroOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }

        // Title zone: 120ms delay
        SequentialAnimation {
            PauseAnimation { duration: 120 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_titleOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_titleTY"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            }
        }

        // Action button: 240ms delay
        SequentialAnimation {
            PauseAnimation { duration: 240 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_actionOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_actionTY"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            }
        }

        // Info tiles: 360ms delay
        SequentialAnimation {
            PauseAnimation { duration: 360 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_tilesOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_tilesTY"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            }
        }

        // About: 480ms delay
        SequentialAnimation {
            PauseAnimation { duration: 480 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_aboutOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_aboutTY"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            }
        }

        // Contributors: 600ms delay
        SequentialAnimation {
            PauseAnimation { duration: 600 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_contribOp"; to: 1; duration: 300; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_contribTY"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            }
        }
    }

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
    // HERO BANNER (full width, 280px, multi-stop gradient fade)
    // =========================================================================

    Rectangle {
        id: heroBanner
        width: parent.width
        height: 280
        color: "transparent"
        opacity: heroRoot._heroOp

        Image {
            id: bannerImg
            anchors.fill: parent
            property bool _useCdnFallback: false
            source: _useCdnFallback
                ? "https://cdn.makineceviri.org/assets/banners/" + heroRoot.vm.steamAppId + ".jpg"
                : heroRoot.vm.heroUrl
            fillMode: Image.PreserveAspectCrop
            verticalAlignment: Image.AlignTop
            sourceSize: Qt.size(width, height)
            asynchronous: true
            mipmap: true
            opacity: status === Image.Ready ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
            onStatusChanged: {
                if (status === Image.Error && !_useCdnFallback && heroRoot.vm.steamAppId !== "")
                    _useCdnFallback = true
            }
        }

        // Top vignette — subtle darkening for contrast
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top
            height: 80
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.3) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Bottom gradient fade — multi-stop for smooth transition
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 220
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Theme.withAlpha(Theme.bgPrimary, 0.6) }
                GradientStop { position: 1.0; color: Theme.bgPrimary }
            }
        }
    }

    // =========================================================================
    // MAIN CONTENT COLUMN (overlaps hero by 80px)
    // =========================================================================

    ColumnLayout {
        id: mainColumn
        anchors.top: heroBanner.bottom
        anchors.topMargin: -100
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginXL
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginXL
        spacing: Dimensions.spacingSection

        // =================================================================
        // TITLE ZONE — centered, Apple TV+ style
        // =================================================================

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMD
            opacity: heroRoot._titleOp
            transform: Translate { y: heroRoot._titleTY }

            // Game identity — title as placeholder, logo replaces when loaded
            // Logo sizing: 10% of window width, clamped 60–140px, proportional
            Item {
                id: gameIdentity
                Layout.fillWidth: true

                readonly property real _clampedH: Math.max(60, Math.min(140, heroRoot.width * 0.10))

                Layout.preferredHeight: gameLogo.ready
                    ? Math.min(width * gameLogo._aspect, _clampedH)
                    : gameTitle.implicitHeight

                Text {
                    id: gameTitle
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !gameLogo.ready
                    textFormat: Text.PlainText
                    text: heroRoot.vm.gameName
                    font.pixelSize: Dimensions.fontBanner
                    font.weight: Font.Bold
                    font.letterSpacing: -0.8
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    opacity: gameLogo.ready ? 0 : 1
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                Image {
                    id: gameLogo
                    anchors.fill: parent
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
                    horizontalAlignment: Image.AlignHCenter
                    sourceSize.width: 500
                    opacity: ready ? 1 : 0
                    layer.enabled: ready
                    layer.smooth: false
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: "#ffffff"
                        shadowBlur: 0.4
                        shadowVerticalOffset: 0
                        shadowHorizontalOffset: 0
                        shadowOpacity: 0.5
                        brightness: 0.15
                    }
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
            }

            // Developer name — centered
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: heroRoot.vm.developersText !== ""
                text: heroRoot.vm.developersText
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textMuted
                elide: Text.ElideRight
                maximumLineCount: 1
                horizontalAlignment: Text.AlignHCenter
            }

            // Editor's pick badge — centered
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: editorsPill.height
                visible: heroRoot.vm.isEditorsPick

                Rectangle {
                    id: editorsPill
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: editorsPickContent.width + 20
                    height: 24
                    radius: Dimensions.radiusFull
                    color: Theme.warning12
                    border.color: Theme.warning25; border.width: 1

                    Row {
                        id: editorsPickContent
                        anchors.centerIn: parent; spacing: Dimensions.spacingXS
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

            // Editor's note — centered
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: heroRoot.vm.isEditorsPick && heroRoot.vm.editorsNote !== ""
                text: "\u201C" + heroRoot.vm.editorsNote + "\u201D"
                font.pixelSize: Dimensions.fontSM
                font.italic: true
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.8
            }
        }

        // =================================================================
        // BADGE ROW (broken / lost warnings) — centered
        // =================================================================

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: Dimensions.spacingMD
            opacity: heroRoot._titleOp
            visible: heroRoot._impactLevel === "broken" || heroRoot._impactLevel === "lost"

            Rectangle {
                visible: heroRoot._impactLevel === "broken"
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

            Rectangle {
                visible: heroRoot._impactLevel === "lost"
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
        }

        // =================================================================
        // ACTION BUTTON (right under title when available)
        // =================================================================

        Item {
            Layout.fillWidth: true
            visible: heroRoot.vm.hasTranslation && heroRoot.vm.isGameInstalled
            implicitHeight: visible ? actionCol.implicitHeight : 0
            opacity: heroRoot._actionOp
            transform: Translate { y: heroRoot._actionTY }

            ColumnLayout {
                id: actionCol
                anchors.left: parent.left; anchors.right: parent.right
                spacing: Dimensions.spacingMD

                TranslationActionButton {
                    vm: heroRoot.vm
                    onTranslateClicked: heroRoot.translateClicked()
                    onUpdateClicked: heroRoot.updateClicked()
                    onUninstallClicked: heroRoot.uninstallClicked()
                }

                // Install error message
                Rectangle {
                    visible: heroRoot.vm.installErrorMessage !== ""
                    Layout.fillWidth: true
                    implicitHeight: _errRow.height + 16
                    radius: Dimensions.radiusMD
                    color: Theme.error08
                    border.color: Theme.error20; border.width: 1

                    Row {
                        id: _errRow
                        anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        width: parent.width - 24
                        Text {
                            textFormat: Text.PlainText
                            text: heroRoot.vm.installErrorMessage
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.error
                            wrapMode: Text.WordWrap
                            width: parent.width
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    opacity: visible ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
                }

                // No translation notice (manual games)
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
            }
        }

        // =================================================================
        // INFO TILES BAR (genre, metacritic, release — below action button)
        // =================================================================

        InfoTilesBar {
            Layout.fillWidth: true
            vm: heroRoot.vm
            opacity: heroRoot._tilesOp
            transform: Translate { y: heroRoot._tilesTY }
        }

        // =================================================================
        // ABOUT SECTION
        // =================================================================

        AboutSection {
            Layout.fillWidth: true
            vm: heroRoot.vm
            opacity: heroRoot._aboutOp
            transform: Translate { y: heroRoot._aboutTY }
        }

        // =================================================================
        // CONTRIBUTORS SECTION
        // =================================================================

        ContributorsSection {
            Layout.fillWidth: true
            contributors: heroRoot.vm.contributors
            externalUrl: heroRoot.vm.externalUrl
            isApex: heroRoot.vm.isApex
            isHangar: heroRoot.vm.isHangar
            apexTier: heroRoot.vm.apexTier
            opacity: heroRoot._contribOp
            transform: Translate { y: heroRoot._contribTY }
        }

        // =================================================================
        // COMMUNITY DISCLAIMER — subtle, centered
        // =================================================================

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: qsTr("Bu yerelleştirme topluluk tarafından yapılmıştır ve resmi değildir.")
            font.pixelSize: Dimensions.fontCaption
            font.italic: true
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            opacity: heroRoot._contribOp * 0.5
        }
    }
}
