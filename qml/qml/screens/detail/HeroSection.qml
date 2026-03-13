import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
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
    property real _titleOp: 0;  property real _titleTY: 12
    property real _actionOp: 0; property real _actionTY: 12
    property real _tilesOp: 0;  property real _tilesTY: 12
    property real _aboutOp: 0;  property real _aboutTY: 12
    property real _contribOp: 0; property real _contribTY: 12

    function replayEntryAnim() {
        _entrySeq.stop()
        if (!Dimensions.animNormal) {
            _heroOp = 1; _titleOp = 1; _titleTY = 0
            _actionOp = 1; _actionTY = 0; _tilesOp = 1; _tilesTY = 0
            _aboutOp = 1; _aboutTY = 0; _contribOp = 1; _contribTY = 0
            return
        }
        _heroOp = 0; _titleOp = 0; _titleTY = 12
        _actionOp = 0; _actionTY = 12; _tilesOp = 0; _tilesTY = 12
        _aboutOp = 0; _aboutTY = 12; _contribOp = 0; _contribTY = 12
        _entrySeq.start()
    }

    ParallelAnimation {
        id: _entrySeq

        // Hero banner: 0ms delay, 250ms fade
        NumberAnimation { target: heroRoot; property: "_heroOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }

        // Cover + Title: 100ms delay
        SequentialAnimation {
            PauseAnimation { duration: 100 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_titleOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_titleTY"; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }

        // Action button: 200ms delay
        SequentialAnimation {
            PauseAnimation { duration: 200 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_actionOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_actionTY"; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }

        // Info tiles: 300ms delay
        SequentialAnimation {
            PauseAnimation { duration: 300 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_tilesOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_tilesTY"; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }

        // About: 400ms delay
        SequentialAnimation {
            PauseAnimation { duration: 400 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_aboutOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_aboutTY"; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }

        // Contributors: 500ms delay
        SequentialAnimation {
            PauseAnimation { duration: 500 }
            ParallelAnimation {
                NumberAnimation { target: heroRoot; property: "_contribOp"; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: heroRoot; property: "_contribTY"; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }
    }

    // =========================================================================
    // HERO BANNER (full width, 240px, gradient fade)
    // =========================================================================

    Rectangle {
        id: heroBanner
        width: parent.width
        height: 240
        color: "transparent"
        opacity: heroRoot._heroOp

        Image {
            id: bannerImg
            anchors.fill: parent
            source: heroRoot.vm.heroUrl
            fillMode: Image.PreserveAspectCrop
            verticalAlignment: Image.AlignTop
            asynchronous: true
            opacity: status === Image.Ready ? 1.0 : 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        }

        // Bottom gradient fade into background
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 200
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.bgPrimary }
            }
        }
    }

    // =========================================================================
    // MAIN CONTENT COLUMN (overlaps hero by 60px)
    // =========================================================================

    ColumnLayout {
        id: mainColumn
        anchors.top: heroBanner.bottom
        anchors.topMargin: -60
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.marginLG
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.marginLG
        spacing: Dimensions.spacingXL

        // =================================================================
        // COVER + TITLE ZONE
        // =================================================================

        // =================================================================
        // TITLE ZONE (logo/name + developer + editor badge)
        // =================================================================

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingSM
            opacity: heroRoot._titleOp
            transform: Translate { y: heroRoot._titleTY }

            // Editor's pick badge (pill)
            Rectangle {
                visible: heroRoot.vm.isEditorsPick
                Layout.preferredWidth: editorsPickContent.width + 16
                Layout.preferredHeight: 22
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

            // Game logo (Steam) with text fallback
            Image {
                id: gameLogo
                Layout.fillWidth: true
                property real _aspect: 0
                onStatusChanged: if (status === Image.Ready && implicitWidth > 0)
                    _aspect = implicitHeight / implicitWidth
                Layout.preferredHeight: _aspect > 0 ? Math.min(width * _aspect, 60) : 0
                Layout.maximumHeight: 60
                visible: _aspect > 0
                source: heroRoot.vm.logoUrl
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignLeft
                sourceSize.width: 400
                asynchronous: true
            }

            // Game title — always visible, bold
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: heroRoot.vm.gameName
                font.pixelSize: Dimensions.fontHero
                font.weight: Font.Bold
                font.letterSpacing: Dimensions.letterSpacingHeadline
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            // Developer name
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: heroRoot.vm.developersText !== ""
                text: heroRoot.vm.developersText
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // Editor's note
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
                opacity: 0.8
            }
        }

        // =================================================================
        // BADGE ROW (broken / lost warnings)
        // =================================================================

        Row {
            spacing: Dimensions.spacingMD
            opacity: heroRoot._titleOp
            visible: (heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "broken") ||
                     (heroRoot.vm.updateImpact && heroRoot.vm.updateImpact.level === "lost")

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
        }

        // =================================================================
        // ACTION BUTTON
        // =================================================================

        Item {
            Layout.fillWidth: true
            implicitHeight: actionCol.implicitHeight
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
        // INFO TILES BAR
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
            opacity: heroRoot._contribOp
            transform: Translate { y: heroRoot._contribTY }
        }

        // =================================================================
        // COMMUNITY DISCLAIMER
        // =================================================================

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: disclaimerText.implicitHeight + 2 * Dimensions.paddingMD
            radius: Dimensions.radiusMD
            color: Theme.textPrimary03
            border.color: Theme.textPrimary06
            border.width: 1
            opacity: heroRoot._contribOp

            Text {
                id: disclaimerText
                textFormat: Text.PlainText
                anchors.fill: parent
                anchors.margins: Dimensions.paddingMD
                text: qsTr("Bu yerelleştirme topluluk tarafından yapılmıştır ve resmi değildir.")
                font.pixelSize: Dimensions.fontCaption
                font.italic: true
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                opacity: 0.7
            }
        }
    }
}
