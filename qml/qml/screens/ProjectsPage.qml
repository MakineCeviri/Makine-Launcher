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

    readonly property var projectData: [
        { title: "Cyberless: Online", desc: qsTr("\u00C7ok oyunculu cyberpunk aksiyon oyunu"), category: "oyun", status: qsTr("Tamamland\u0131"), statusColor: Theme.statusOnline, accent: Theme.statusOnline, emoji: "\uD83C\uDFAE", progress: 1.0 },
        { title: "MakineAI Launcher", desc: qsTr("T\u00FCrk\u00E7e oyun \u00E7evirisi ba\u015Flat\u0131c\u0131s\u0131 ve y\u00F6netim arac\u0131"), category: "ceviri", status: qsTr("Alfa"), statusColor: Theme.primary, accent: Theme.primary, emoji: "\uD83D\uDE80", progress: 0.7 },
        { title: qsTr("Topluluk \u00C7eviri Paketi"), desc: qsTr("A\u00E7\u0131k kaynak topluluk \u00E7eviri paketleri"), category: "ceviri", status: qsTr("S\u00FCrekli"), statusColor: Theme.statusCyan, accent: Theme.statusCyan, emoji: "\uD83C\uDF0D", progress: -1 },
        { title: "MakineAI", desc: qsTr("Oyun \u00E7evirisi i\u00E7in yapay zeka dil modeli"), category: "ceviri", status: qsTr("Geli\u015Ftiriliyor"), statusColor: Theme.statusPurple, accent: Theme.statusPurple, emoji: "\uD83E\uDD16", progress: 0.3 },
        { title: qsTr("\u00C7eviri API"), desc: qsTr("Geli\u015Ftiriciler i\u00E7in RESTful \u00E7eviri API hizmeti"), category: "ceviri", status: qsTr("Planlan\u0131yor"), statusColor: Theme.warning, accent: Theme.warning, emoji: "\u26A1", progress: 0.0 }
    ]

    function replayProjectAnimations() {
        for (var i = 0; i < projectCardsRepeater.count; i++) {
            var item = projectCardsRepeater.itemAt(i)
            if (item) item.replayAnimation()
        }
    }

    // Main layout - EXACT same pattern as homepage
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
                    property string iconType: "" // "discord", "globe", "heart"

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

                        // Icon circle with Canvas-drawn vector icon
                        Rectangle {
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignVCenter
                            radius: 16
                            color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.15 : 0.08)
                            border.color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.25 : 0.12)
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

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
                                        // Chat bubble icon
                                        ctx.beginPath()
                                        ctx.moveTo(2, 4)
                                        ctx.lineTo(2, 11)
                                        ctx.lineTo(5, 11)
                                        ctx.lineTo(5, 14)
                                        ctx.lineTo(8, 11)
                                        ctx.lineTo(14, 11)
                                        ctx.lineTo(14, 4)
                                        ctx.closePath()
                                        ctx.stroke()
                                        // Dots inside
                                        ctx.beginPath()
                                        ctx.arc(6, 7.5, 1, 0, Math.PI * 2)
                                        ctx.fill()
                                        ctx.beginPath()
                                        ctx.arc(10, 7.5, 1, 0, Math.PI * 2)
                                        ctx.fill()
                                    } else if (iconType === "globe") {
                                        // Globe icon
                                        ctx.beginPath()
                                        ctx.arc(8, 8, 6.5, 0, Math.PI * 2)
                                        ctx.stroke()
                                        // Horizontal line
                                        ctx.beginPath()
                                        ctx.moveTo(1.5, 8)
                                        ctx.lineTo(14.5, 8)
                                        ctx.stroke()
                                        // Vertical ellipse (meridian)
                                        ctx.beginPath()
                                        ctx.ellipse(4.5, 1.5, 7, 13, 0, 0, Math.PI * 2)
                                        ctx.stroke()
                                    } else if (iconType === "heart") {
                                        // Heart icon
                                        ctx.beginPath()
                                        ctx.moveTo(8, 14)
                                        ctx.bezierCurveTo(1, 9, 1, 3.5, 4.5, 2.5)
                                        ctx.bezierCurveTo(6.5, 2, 8, 4, 8, 4)
                                        ctx.bezierCurveTo(8, 4, 9.5, 2, 11.5, 2.5)
                                        ctx.bezierCurveTo(15, 3.5, 15, 9, 8, 14)
                                        ctx.closePath()
                                        ctx.fill()
                                        ctx.globalAlpha = 0.3
                                        ctx.stroke()
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
                    Rectangle {
                        anchors.fill: parent; anchors.margins: -1
                        radius: parent.radius + 1; color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6); border.width: 2
                        visible: parent.activeFocus
                    }
                }

                LinkCard { label: "Discord"; subtitle: qsTr("Toplulu\u011Fa kat\u0131l"); url: Dimensions.discordUrl; cardColor: Theme.discordColor; iconType: "discord" }
                LinkCard { label: qsTr("Web Sitesi"); subtitle: "makineai.com"; url: Dimensions.websiteUrl; cardColor: Theme.primary; iconType: "globe" }
                LinkCard { label: qsTr("Aramıza Katıl"); subtitle: qsTr("Ekibe katılın"); url: "https://makineai.com"; cardColor: Theme.brandCoral; iconType: "heart" }
            }
        }

        // ===== PROJECTS SECTION =====
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: projectsPage.contentMargin
            Layout.rightMargin: projectsPage.contentMargin
            spacing: projectsPage.layoutGamesSectionGap

            RowLayout {
                spacing: Dimensions.spacingLG

                Label {
                    text: qsTr("Aktif Projeler")
                    font.pixelSize: Dimensions.fontXL
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: projectCountLabel.width + 14
                    radius: Dimensions.badgeRadius
                    color: Theme.withAlpha(Theme.primary, 0.12)
                    Label {
                        id: projectCountLabel; anchors.centerIn: parent
                        text: qsTr("%1 proje").arg(projectsPage.projectData.length)
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

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Dimensions.spacingXL
                rowSpacing: Dimensions.spacingLG

                Repeater {
                    id: projectCardsRepeater
                    model: projectsPage.projectData

                    ProjectShowcaseCard {
                        title: modelData.title
                        description: modelData.desc
                        status: modelData.status
                        statusColor: modelData.statusColor
                        accentColor: modelData.accent
                        emoji: modelData.emoji
                        progress: modelData.progress
                        entryIndex: index
                        animationsEnabled: projectsPage.animationsEnabled
                    }
                }
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
