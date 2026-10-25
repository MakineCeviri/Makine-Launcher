// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * WhatsNewScreen.qml - "Yenilikler": the launcher's own news
 *
 * Whether a newer launcher is out and how to get it (the Store's page in the
 * Store build, the download page for the ZIP), what changed in each version,
 * and what we are working on next. Opens by itself on the first start after
 * an update. Notes and plan come from UpdateService: built into the app
 * (qml/resources/whatsnew.json) and refreshed from the CDN's copy.
 */
Item {
    id: news

    property real contentMargin: 16

    function scrollToTop() { newsFlick.contentY = 0 }

    readonly property int _state: UpdateService.state
    readonly property bool _update: _state !== UpdateService.Idle && _state !== UpdateService.Checking
    readonly property bool _failed: _state === UpdateService.Idle && UpdateService.error !== ""

    onVisibleChanged: if (visible) UpdateService.markNotesSeen()
    Component.onCompleted: if (visible) UpdateService.markNotesSeen()

    function _kindLabel(type) {
        return type === "new" ? qsTr("Yeni") : type === "fixed" ? qsTr("Düzeldi") : qsTr("İyileşti")
    }
    function _kindTint(type) {
        return type === "new" ? Theme.primary : type === "fixed" ? Theme.success : Theme.info
    }

    // ── Small parts ──────────────────────────────────────────────────────────
    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 12
        textFormat: Text.PlainText
        font.family: Dimensions.fontDisplay
        font.pixelSize: Dimensions.title2
        font.weight: Font.Bold
        font.letterSpacing: Dimensions.trackingHeadline
        color: Theme.textPrimary
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        lineHeight: 1.3
        font.pixelSize: Dimensions.body
        color: Theme.textSecondary
    }

    // One version: its number and date, what it is about, and every change
    // with its kind. Older versions open on a tap.
    component Release: Card {
        id: rel
        property var release: ({})
        property bool open: true

        Layout.fillWidth: true
        implicitHeight: relCol.implicitHeight + 40
        radius: Dimensions.radiusLG
        glow: rel.release.newer === true

        ColumnLayout {
            id: relCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            Item {
                id: relHead
                Layout.fillWidth: true
                implicitHeight: headCol.implicitHeight

                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Sürüm %1").arg(rel.release.version)
                Keys.onReturnPressed: rel.open = !rel.open
                Keys.onSpacePressed: rel.open = !rel.open

                ColumnLayout {
                    id: headCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Sürüm %1").arg(rel.release.version)
                            font.family: Dimensions.fontDisplay
                            font.pixelSize: Dimensions.title3
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                        StatusBadge {
                            visible: rel.release.running === true
                            text: qsTr("Kullandığın sürüm")
                            tint: Theme.success
                        }
                        StatusBadge {
                            visible: rel.release.newer === true
                            text: qsTr("Yeni sürüm")
                            tint: Theme.primary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            textFormat: Text.PlainText
                            text: rel.release.dateText || ""
                            font.pixelSize: Dimensions.footnote
                            color: Theme.textMuted
                        }
                        Icon {
                            glyph: Icons.chevronDown
                            size: 15
                            color: Theme.textTertiary
                            rotation: rel.open ? 180 : 0
                            Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: (rel.release.title || "") !== ""
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        text: rel.release.title || ""
                        font.pixelSize: Dimensions.headline
                        font.weight: Font.DemiBold
                        color: Theme.textSecondary
                    }
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: rel.open = !rel.open }
            }

            Text {
                Layout.fillWidth: true
                visible: rel.open && (rel.release.summary || "") !== ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: rel.release.summary || ""
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }

            Repeater {
                model: rel.open ? (rel.release.changes || []) : []
                delegate: RowLayout {
                    id: change
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 12
                    Item {
                        Layout.preferredWidth: 78
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: kind.implicitHeight
                        StatusBadge {
                            id: kind
                            text: news._kindLabel(change.modelData.type)
                            tint: news._kindTint(change.modelData.type)
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                        text: change.modelData.text
                        font.pixelSize: Dimensions.callout
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    // One column of the plan: what we are on, what is next, what is later
    component Stage: Card {
        id: stage
        property string status: ""
        property string title: ""
        property string glyph: ""
        property color tint: Theme.primary
        readonly property var items: UpdateService.roadmap.filter(i => i.status === stage.status)

        visible: stage.items.length > 0
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        implicitHeight: stageCol.implicitHeight + 40
        radius: Dimensions.radiusLG

        ColumnLayout {
            id: stageCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 14

            RowLayout {
                spacing: 10
                IconTile { size: 30; glyph: stage.glyph; tint: stage.tint }
                Text {
                    textFormat: Text.PlainText
                    text: stage.title
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }
            }
            Repeater {
                model: stage.items
                delegate: ColumnLayout {
                    id: item
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 4
                    Rectangle {
                        visible: item.index > 0
                        Layout.fillWidth: true
                        Layout.bottomMargin: 10
                        implicitHeight: 1
                        color: Theme.separator
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        text: item.modelData.title
                        font.pixelSize: Dimensions.body
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                        text: item.modelData.text || ""
                        font.pixelSize: Dimensions.callout
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: newsFlick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: newsFlick.parent
            anchors.top: newsFlick.top; anchors.right: newsFlick.right; anchors.bottom: newsFlick.bottom
        }

        ColumnLayout {
            id: page
            width: Math.min(newsFlick.width - Dimensions.pageMargin * 2, Dimensions.contentMaxWidth)
            x: (newsFlick.width - width) / 2
            spacing: 16

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Yenilikler")
                subtitle: qsTr("Launcher'ın her sürümünde neyin değiştiği ve sırada ne olduğu. Yeni bir sürüm çıktığında burada ve kenar çubuğunun altında haber veririz.")
            }

            // ── This copy: up to date, or how to get the new one ──
            Card {
                Layout.fillWidth: true
                implicitHeight: statusRow.implicitHeight + 36
                radius: Dimensions.radiusLG
                glow: news._update

                RowLayout {
                    id: statusRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 14

                    IconTile {
                        Layout.alignment: Qt.AlignTop
                        size: 40
                        glyph: news._update ? Icons.circleArrowDown : news._failed ? Icons.circleAlert : Icons.circleCheck
                        tint: news._update ? Theme.primary : news._failed ? Theme.warning : Theme.success
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            text: {
                                switch (news._state) {
                                case UpdateService.Checking:    return qsTr("Yeni sürüm var mı diye bakılıyor…")
                                case UpdateService.Available:   return qsTr("Yeni sürüm çıktı: %1").arg(UpdateService.version)
                                case UpdateService.Downloading: return qsTr("İndiriliyor… %%1").arg(Math.round(UpdateService.progress * 100))
                                case UpdateService.Verifying:   return qsTr("İndirilen dosya doğrulanıyor…")
                                case UpdateService.Ready:       return qsTr("Güncelleme hazır")
                                case UpdateService.Installing:  return qsTr("Güncelleme kuruluyor…")
                                default: return news._failed ? qsTr("Yeni sürüme bakılamadı") : qsTr("Güncelsin")
                                }
                            }
                            font.pixelSize: Dimensions.headline
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            lineHeight: 1.25
                            text: {
                                switch (news._state) {
                                case UpdateService.Available: return UpdateService.actionHint
                                case UpdateService.Ready:     return qsTr("Launcher kapanıp yeni sürümle açılır; kurulu yamalarına dokunulmaz.")
                                case UpdateService.Idle:
                                    if (news._failed)
                                        return qsTr("%1. İnternet bağlantını kontrol edip yeniden dene; kurulu yamaların bundan etkilenmez.").arg(UpdateService.error)
                                    return UpdateService.storeBuild
                                        ? qsTr("Sürüm %1. Yeni sürümler Microsoft Store'dan gelir; çıktığında burada ve kenar çubuğunun altında görürsün.").arg(Dimensions.appVersion)
                                        : qsTr("Sürüm %1. Launcher açılışta ve gün içinde yeni sürüm olup olmadığına kendisi bakar.").arg(Dimensions.appVersion)
                                default: return qsTr("Sürüm %1").arg(Dimensions.appVersion)
                                }
                            }
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                        }
                    }
                    Spinner {
                        visible: news._state === UpdateService.Checking || news._state === UpdateService.Installing
                    }
                    AppButton {
                        visible: news._state === UpdateService.Idle
                        style: "gray"
                        icon: Icons.refreshCw
                        text: news._failed ? qsTr("Tekrar dene") : qsTr("Denetle")
                        onClicked: UpdateService.check()
                    }
                    AppButton {
                        visible: UpdateService.actionable
                        style: news._state === UpdateService.Available || news._state === UpdateService.Ready ? "filled" : "gray"
                        text: UpdateService.actionText
                        trailingIcon: UpdateService.opensPage ? Icons.arrowUpRight : ""
                        onClicked: {
                            switch (news._state) {
                            case UpdateService.Available: UpdateService.download(); break
                            case UpdateService.Ready: UpdateService.install(); break
                            case UpdateService.Downloading:
                            case UpdateService.Verifying: UpdateService.cancel(); break
                            }
                        }
                    }
                }
            }

            // ── Notes ──
            SectionTitle { text: qsTr("Sürüm notları") }
            Repeater {
                model: UpdateService.releases
                delegate: Release {
                    required property var modelData
                    required property int index
                    release: modelData
                    // The newest few open, older ones on a tap
                    open: index < 2 || modelData.newer === true || modelData.running === true
                }
            }

            // ── Plan ──
            SectionTitle { text: qsTr("Sırada ne var?") }
            Body {
                text: qsTr("Üzerinde çalıştığımız ve sıraya aldığımız işler. Plan Discord'daki isteklerle ve hata raporlarıyla değişebilir; tarih vermiyoruz, hazır olanı yayınlıyoruz.")
            }
            GridLayout {
                Layout.fillWidth: true
                columns: page.width > 860 ? 3 : 1
                columnSpacing: 14
                rowSpacing: 14

                Stage { status: "doing"; title: qsTr("Üzerinde çalışıyoruz"); glyph: Icons.wrench; tint: Theme.primary }
                Stage { status: "next"; title: qsTr("Sırada"); glyph: Icons.listOrdered; tint: Theme.info }
                Stage { status: "later"; title: qsTr("Daha sonra"); glyph: Icons.hourglass; tint: Theme.textMuted }
            }

            Body {
                Layout.topMargin: 4
                text: qsTr("Bir isteğin ya da bir sorun mu var? Topluluğumuz ve çevirmenlerimiz Discord'da.")
            }
            AppButton {
                Layout.bottomMargin: 8
                style: "gray"
                icon: Icons.messagesSquare
                text: qsTr("Discord'da yaz")
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }
        }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: newsFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
