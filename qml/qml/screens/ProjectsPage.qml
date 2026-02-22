import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

Item {
    id: projectsPage

    property bool animationsEnabled: true
    property real contentMargin: 16
    property real layoutGamesSectionGap: 8
    property real layoutSepTopMargin: 4
    property real layoutSepBottomMargin: 8

    property var installedList: GameService.installedTranslations()

    signal gameSelected(string gameId, string gameName, string installPath, string engine)

    function replayProjectAnimations() {
        installedList = GameService.installedTranslations()
    }

    // Main layout
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: projectsPage.contentMargin
        spacing: Dimensions.marginMD

        // ===== COMMUNITY SECTION =====
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: projectsPage.contentMargin
            Layout.rightMargin: projectsPage.contentMargin
            spacing: projectsPage.layoutGamesSectionGap

            Label {
                text: qsTr("Topluluk")
                font.pixelSize: Dimensions.fontXL
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: projectsPage.layoutSepTopMargin
                Layout.bottomMargin: projectsPage.layoutSepBottomMargin
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingLG

                component LinkCard: Rectangle {
                    property string label: ""
                    property string subtitle: ""
                    property string url: ""
                    property color cardColor: Theme.primary
                    property string iconType: ""

                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    radius: Dimensions.radiusStandard
                    color: lcMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                                 : Theme.withAlpha(Theme.textPrimary, 0.03)
                    border.color: lcMouse.containsMouse ? Theme.withAlpha(cardColor, 0.25)
                                                        : Theme.withAlpha(Theme.textPrimary, 0.08)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Dimensions.marginMS
                        anchors.rightMargin: Dimensions.marginMS
                        spacing: Dimensions.spacingLG

                        Rectangle {
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignVCenter
                            radius: 16
                            color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.15 : 0.08)
                            border.color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.25 : 0.12)
                            border.width: 1

                            Canvas {
                                anchors.centerIn: parent
                                width: 16; height: 16
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = Qt.rgba(cardColor.r, cardColor.g, cardColor.b, 0.9)
                                    ctx.fillStyle = ctx.strokeStyle
                                    ctx.lineWidth = 1.5
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"

                                    if (iconType === "discord") {
                                        ctx.beginPath()
                                        ctx.moveTo(2, 4); ctx.lineTo(2, 11); ctx.lineTo(5, 11)
                                        ctx.lineTo(5, 14); ctx.lineTo(8, 11); ctx.lineTo(14, 11)
                                        ctx.lineTo(14, 4); ctx.closePath(); ctx.stroke()
                                        ctx.beginPath(); ctx.arc(6, 7.5, 1, 0, Math.PI * 2); ctx.fill()
                                        ctx.beginPath(); ctx.arc(10, 7.5, 1, 0, Math.PI * 2); ctx.fill()
                                    } else if (iconType === "globe") {
                                        ctx.beginPath(); ctx.arc(8, 8, 6.5, 0, Math.PI * 2); ctx.stroke()
                                        ctx.beginPath(); ctx.moveTo(1.5, 8); ctx.lineTo(14.5, 8); ctx.stroke()
                                        ctx.beginPath(); ctx.ellipse(4.5, 1.5, 7, 13, 0, 0, Math.PI * 2); ctx.stroke()
                                    } else if (iconType === "heart") {
                                        ctx.beginPath(); ctx.moveTo(8, 14)
                                        ctx.bezierCurveTo(1, 9, 1, 3.5, 4.5, 2.5)
                                        ctx.bezierCurveTo(6.5, 2, 8, 4, 8, 4)
                                        ctx.bezierCurveTo(8, 4, 9.5, 2, 11.5, 2.5)
                                        ctx.bezierCurveTo(15, 3.5, 15, 9, 8, 14)
                                        ctx.closePath(); ctx.fill()
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                            Label {
                                text: label; font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                color: lcMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            }
                            Label { text: subtitle; font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted }
                        }

                        Label {
                            text: "\u2197"; font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                            opacity: lcMouse.containsMouse ? 1.0 : 0.3
                            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                        }
                    }

                    MouseArea {
                        id: lcMouse; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally(url)
                    }
                    Accessible.role: Accessible.Link; Accessible.name: label
                    activeFocusOnTab: true
                    Keys.onReturnPressed: Qt.openUrlExternally(url)
                    Keys.onSpacePressed: Qt.openUrlExternally(url)
                }

                LinkCard { label: "Discord"; subtitle: qsTr("Topluluğa katıl"); url: Dimensions.discordUrl; cardColor: Theme.discordColor; iconType: "discord" }
                LinkCard { label: qsTr("Web Sitesi"); subtitle: "makineai.com"; url: Dimensions.websiteUrl; cardColor: Theme.primary; iconType: "globe" }
                LinkCard { label: qsTr("Aramıza Katıl"); subtitle: qsTr("Ekibe katılın"); url: "https://makineai.com"; cardColor: Theme.brandCoral; iconType: "heart" }
            }
        }

        // ===== MY TRANSLATIONS SECTION =====
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: projectsPage.contentMargin
            Layout.rightMargin: projectsPage.contentMargin
            spacing: projectsPage.layoutGamesSectionGap

            RowLayout {
                spacing: Dimensions.spacingLG

                Label {
                    text: qsTr("Çevirilerim")
                    font.pixelSize: Dimensions.fontXL
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: installedCountLabel.width + 14
                    radius: Dimensions.badgeRadius
                    color: Theme.withAlpha(Theme.primary, 0.12)
                    visible: projectsPage.installedList.length > 0
                    Label {
                        id: installedCountLabel; anchors.centerIn: parent
                        text: qsTr("%1 kurulu").arg(projectsPage.installedList.length)
                        font.pixelSize: Dimensions.fontXS; font.weight: Font.Medium; color: Theme.primary
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: projectsPage.layoutSepTopMargin
                Layout.bottomMargin: projectsPage.layoutSepBottomMargin
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }

            // Installed translations list
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingSM
                visible: projectsPage.installedList.length > 0

                Repeater {
                    model: projectsPage.installedList

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        radius: Dimensions.radiusStandard
                        color: itMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                                     : Theme.withAlpha(Theme.textPrimary, 0.03)
                        border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Dimensions.marginMS
                            anchors.rightMargin: Dimensions.marginMS
                            spacing: Dimensions.spacingLG

                            Rectangle {
                                Layout.preferredWidth: 80
                                Layout.preferredHeight: 38
                                Layout.alignment: Qt.AlignVCenter
                                radius: 6
                                color: Theme.withAlpha(Theme.textPrimary, 0.05)
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: modelData.headerImageUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXXS

                                Label {
                                    text: modelData.name || ""
                                    font.pixelSize: Dimensions.fontBody
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: Dimensions.spacingSM

                                    Label {
                                        text: modelData.version ? "v" + modelData.version : ""
                                        font.pixelSize: Dimensions.fontCaption
                                        color: Theme.textMuted
                                        visible: text.length > 0
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 4
                                        Layout.preferredHeight: 4
                                        radius: 2
                                        color: Theme.statusOnline
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    Label {
                                        text: qsTr("Kurulu")
                                        font.pixelSize: Dimensions.fontCaption
                                        color: Theme.statusOnline
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                Layout.alignment: Qt.AlignVCenter
                                radius: 6
                                color: uninstBtnMouse.containsMouse ? Theme.withAlpha(Theme.error, 0.15) : "transparent"
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                Label {
                                    anchors.centerIn: parent
                                    text: "\u2715"
                                    font.pixelSize: Dimensions.fontSM
                                    color: uninstBtnMouse.containsMouse ? Theme.error : Theme.textMuted
                                }

                                MouseArea {
                                    id: uninstBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: GameService.uninstallTranslation(modelData.id)
                                }
                            }
                        }

                        MouseArea {
                            id: itMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            z: -1
                            onClicked: projectsPage.gameSelected(
                                modelData.id || "", modelData.name || "",
                                modelData.installPath || "", modelData.engine || ""
                            )
                        }
                    }
                }
            }

            // Empty state
            EmptyState {
                visible: projectsPage.installedList.length === 0
                title: qsTr("Henüz çeviri kurulmamış")
                subtitle: qsTr("Kütüphaneden bir oyun seçip çevirisini kurun")
            }
        }

        Item { Layout.fillHeight: true }
    }

    // Bottom fade gradient overlay
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 60
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }
    }
}
