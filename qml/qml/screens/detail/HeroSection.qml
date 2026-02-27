import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
        color: "transparent"

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

        // -- LEFT COLUMN: Cover Art + Disclaimer --
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: Dimensions.spacingLG

            // Cover frame
            Rectangle {
                id: coverFrame
                Layout.preferredWidth: 220
                Layout.preferredHeight: 310
                radius: Dimensions.radiusLG
                color: "transparent"
                clip: true

                Image {
                    id: coverImg
                    anchors.fill: parent
                    source: heroRoot.vm.coverUrl
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(440, 620)
                    asynchronous: true
                    opacity: status === Image.Ready ? 1.0 : 0
                    Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                }

            }

            // Community disclaimer
            Rectangle {
                Layout.preferredWidth: 220
                implicitHeight: disclaimerText.implicitHeight + 2 * Dimensions.paddingMD
                radius: Dimensions.radiusMD
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.06)
                border.width: 1

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

        // -- RIGHT COLUMN: Info + Action + About --
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
                source: heroRoot.vm.logoUrl
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

            // -- ACTION BUTTON --
            TranslationActionButton {
                vm: heroRoot.vm
                onTranslateClicked: heroRoot.translateClicked()
                onUninstallClicked: heroRoot.uninstallClicked()
            }

            // -- No translation notice (manual games) --
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

            // -- Badge row --
            Row {
                spacing: Dimensions.spacingMD

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

            // Spacer: align About section with cover bottom edge
            Item { Layout.fillHeight: true }

            // -- About --
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
