// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * AboutSettings.qml - "Hakkında": what this is, the keys, where to get
 * help, and the open-source parts it is built on.
 */
ColumnLayout {
    id: aboutRoot
    spacing: 28

    // ── Identity ─────────────────────────────────────────────────────────
    // The app's card, centred on the page
    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 6
        spacing: 6

        AppLogo {
            Layout.alignment: Qt.AlignHCenter
            size: 72
            playing: true
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            textFormat: Text.PlainText
            text: "Makine Launcher"
            font.family: Dimensions.fontDisplay
            font.pixelSize: Dimensions.title2
            font.weight: Font.Bold
            color: Theme.textPrimary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Sürüm %1").arg(Dimensions.appVersion)
            font.pixelSize: Dimensions.callout
            color: Theme.textMuted
        }
        // Who makes it and why, in a sentence
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: 540
            Layout.topMargin: 10
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            lineHeight: 1.12
            text: qsTr("Makine Çeviri oyunları senin için Türkçeye çevirir; daha keyifli bir deneyim yaşaman için de Makine Launcher'ı kâr amacı gütmeden, açık kaynak olarak geliştirir.")
            font.family: Dimensions.fontDisplay
            font.pixelSize: Dimensions.title3
            font.weight: Font.Medium
            color: Theme.textPrimary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.topMargin: 2
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: qsTr("Bilgisayarındaki oyunları bulur, Türkçe yamalarını tek tıkla kurar ve istediğin an kaldırır.")
            font.pixelSize: Dimensions.callout
            color: Theme.textSecondary
        }
    }

    // What that means, side by side under one lit edge
    Card {
        Layout.fillWidth: true
        radius: Dimensions.radiusSection
        implicitHeight: pillars.implicitHeight + 40

        RowLayout {
            id: pillars
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 0

            Pillar {
                glyph: Icons.languages
                tint: Theme.primary
                title: qsTr("Türkçe oyna")
                text: qsTr("Türkçesi olmayan oyunları gönüllü çevirmenlerimiz dilimize uyarlar.")
            }
            Divider {}
            Pillar {
                glyph: Icons.handHeart
                tint: "#FF375F"
                title: qsTr("Kâr amacı yok")
                text: qsTr("Yamalar da launcher da ücretsiz; sunucu masraflarına destekçilerimiz katkı verir.")
            }
            Divider {}
            Pillar {
                glyph: Icons.github
                tint: "#30D158"
                title: qsTr("Açık kaynak")
                text: qsTr("Kodu herkese açık: GitHub'da inceleyebilir, hata bildirip katkı verebilirsin.")
                link: "https://github.com/MakineCeviri/Makine-Launcher"
            }
        }
    }

    // Where else to find us, each in its own colour
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12
        Text {
            Layout.alignment: Qt.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Bizi takip et")
            font.pixelSize: Dimensions.callout
            font.weight: Font.DemiBold
            color: Theme.textSecondary
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: socialRow.implicitHeight
            Row {
                id: socialRow
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Repeater {
                    model: [
                        { glyph: Icons.discord, name: "Discord", tint: Theme.discordColor, url: Dimensions.discordUrl },
                        { glyph: Icons.youtube, name: "YouTube", tint: Theme.youtubeColor, url: Dimensions.youtubeUrl },
                        { glyph: Icons.instagram, name: "Instagram", tint: "#E1306C", url: Dimensions.instagramUrl },
                        { glyph: Icons.xLogo, name: "X", tint: Theme.textPrimary, url: Dimensions.xUrl },
                        { glyph: Icons.linkedin, name: "LinkedIn", tint: "#2D8CFF", url: Dimensions.linkedinUrl },
                        { glyph: Icons.github, name: "GitHub", tint: Theme.textPrimary, url: Dimensions.githubUrl }
                    ]
                    delegate: SocialChip {}
                }
            }
        }
    }

    ListSection {
        title: qsTr("Sürüm ve lisans")

        ListRow {
            title: qsTr("Sürüm")
            Text { textFormat: Text.PlainText; text: Dimensions.appVersion; font.pixelSize: Dimensions.body; color: Theme.textMuted }
        }
        ListRow {
            title: qsTr("Grafik arabirimi")
            Text {
                textFormat: Text.PlainText
                text: SettingsManager.activeGraphicsApi()
                font.pixelSize: Dimensions.body
                color: Theme.textMuted
            }
        }
        ListRow {
            title: qsTr("Qt")
            Text { textFormat: Text.PlainText; text: SettingsManager.qtVersion(); font.pixelSize: Dimensions.body; color: Theme.textMuted }
        }
        ListRow {
            title: qsTr("Lisans")
            Text { textFormat: Text.PlainText; text: "AGPL-3.0 + Commons Clause"; font.pixelSize: Dimensions.body; color: Theme.textMuted }
        }
    }

    ListSection {
        title: qsTr("Yardım ve topluluk")

        ListRow {
            icon: Icons.discord
            iconTint: Theme.discordColor
            clickable: true
            title: qsTr("Discord")
            subtitle: qsTr("Bir yama çalışmıyorsa ya da bir sorun varsa topluluğa sor; ekip de orada.")
            onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
        }
        ListRow {
            icon: Icons.youtube
            iconTint: Theme.youtubeColor
            clickable: true
            title: qsTr("YouTube")
            subtitle: qsTr("Yama kurulum videoları ve launcher rehberleri.")
            onClicked: Qt.openUrlExternally(Dimensions.youtubeUrl)
        }
        ListRow {
            icon: Icons.messageSquareText
            iconTint: Theme.primary
            clickable: true
            title: qsTr("Geri bildirim")
            subtitle: qsTr("Hata bildirimi ve öneriler için iletişim sayfası.")
            onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
        }
        ListRow {
            icon: Icons.users
            iconTint: "#30D158"
            clickable: true
            title: qsTr("Aramıza katıl")
            subtitle: qsTr("Çeviri, test ya da geliştirme — Makine Çeviri ekibine katıl.")
            onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
        }
        ListRow {
            icon: Icons.heart
            iconTint: "#FF375F"
            clickable: true
            title: qsTr("Destek ol")
            subtitle: qsTr("Sunucu ve CDN masraflarına katkı (Kreosus).")
            onClicked: Qt.openUrlExternally("https://kreosus.com/makineceviri/")
        }
    }

    ListSection {
        title: qsTr("Klavye kısayolları")

        Repeater {
            model: [
                { key: "Ctrl+1", desc: qsTr("Ana Sayfa") },
                { key: "Ctrl+2", desc: qsTr("Kütüphanem") },
                { key: "Ctrl+3", desc: qsTr("Tüm Oyunlar") },
                { key: "Ctrl+F", desc: qsTr("Oyun ara") },
                { key: "F1", desc: qsTr("Nasıl çalışır?") },
                { key: "Ctrl+,", desc: qsTr("Ayarlar") },
                { key: "Ctrl+R", desc: qsTr("Kataloğu yenile, oyunları yeniden tara") },
                { key: "Esc", desc: qsTr("Bir oyunun sayfasından geri dön") },
                { key: "Ctrl+Q", desc: qsTr("Launcher'dan çık") }
            ]

            ListRow {
                id: keyRow
                required property var modelData
                minHeight: 40
                title: keyRow.modelData.desc
                Rectangle {
                    implicitWidth: keyLabel.implicitWidth + 14
                    implicitHeight: 22
                    radius: 6
                    color: Theme.fillTertiary
                    Text {
                        id: keyLabel
                        anchors.centerIn: parent
                        textFormat: Text.PlainText
                        text: keyRow.modelData.key
                        font.pixelSize: Dimensions.caption1
                        font.family: Dimensions.fontText
                        font.weight: Font.DemiBold
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    ListSection {
        title: qsTr("Yasal")

        ListRow {
            clickable: true
            title: qsTr("Gizlilik politikası")
            onClicked: Qt.openUrlExternally("https://makineceviri.org/gizlilik-politikasi/")
        }
        ListRow {
            clickable: true
            title: qsTr("Kullanım koşulları")
            onClicked: Qt.openUrlExternally("https://makineceviri.org/kullanim-kosullari/")
        }
    }

    ListSection {
        title: qsTr("Açık kaynak bileşenler")
        footer: qsTr("Launcher bu projelerin üstüne kurulu; her birinin lisansı satırına tıklayınca açılır.")

        Repeater {
            model: [
                { name: "Qt", license: "LGPL-3.0", url: "https://www.qt.io/licensing" },
                { name: "OpenSSL", license: "Apache-2.0", url: "https://www.openssl.org/source/license.html" },
                { name: "Sentry Native", license: "MIT", url: "https://github.com/getsentry/sentry-native/blob/master/LICENSE" },
                { name: "spdlog", license: "MIT", url: "https://github.com/gabime/spdlog/blob/v1.x/LICENSE" },
                { name: "nlohmann/json", license: "MIT", url: "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT" },
                { name: "zstd", license: "BSD-3-Clause", url: "https://github.com/facebook/zstd/blob/dev/LICENSE" },
                { name: "xdelta3", license: "Apache-2.0", url: "https://github.com/jmacd/xdelta/blob/release3_1_apl/xdelta3/LICENSE" },
                { name: "Inter", license: "OFL-1.1", url: "https://github.com/rsms/inter/blob/master/LICENSE.txt" },
                { name: "Lucide", license: "ISC", url: "https://github.com/lucide-icons/lucide/blob/main/LICENSE" },
                { name: "Phosphor Icons", license: "MIT", url: "https://github.com/phosphor-icons/core/blob/main/LICENSE" },
                { name: "Simple Icons", license: "CC0-1.0", url: "https://github.com/simple-icons/simple-icons/blob/develop/LICENSE.md" }
            ]

            ListRow {
                id: licenseRow
                required property var modelData
                minHeight: 40
                clickable: true
                showChevron: false
                title: licenseRow.modelData.name
                onClicked: Qt.openUrlExternally(licenseRow.modelData.url)
                Text {
                    textFormat: Text.PlainText
                    text: licenseRow.modelData.license
                    font.pixelSize: Dimensions.footnote
                    color: Theme.textMuted
                }
                Icon { glyph: Icons.arrowUpRight; size: 13; color: Theme.textTertiary }
            }
        }
    }
    // One promise: a sign, a few words, and a link when there is a page for it
    component Pillar: ColumnLayout {
        id: pillar
        property string glyph: ""
        property color tint: Theme.primary
        property string title: ""
        property string text: ""
        property string link: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        spacing: 6

        IconTile {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 4
            size: 34
            glyph: pillar.glyph
            tint: pillar.tint
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: pillar.title
            font.pixelSize: Dimensions.headline
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: pillar.text
            font.pixelSize: Dimensions.footnote
            font.family: Dimensions.fontText
            color: Theme.textSecondary
        }
        Row {
            visible: pillar.link !== ""
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            spacing: 3
            Text {
                textFormat: Text.PlainText
                text: qsTr("GitHub'da gör")
                font.pixelSize: Dimensions.footnote
                font.weight: Font.DemiBold
                color: linkHover.hovered ? Theme.primaryHover : Theme.primaryText
            }
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                glyph: Icons.arrowUpRight
                size: 12
                color: linkHover.hovered ? Theme.primaryHover : Theme.primaryText
            }
            HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Qt.openUrlExternally(pillar.link) }
        }
    }

    // A place to follow us: its mark in its own colour, its name
    component SocialChip: Rectangle {
        id: chip
        required property var modelData
        implicitWidth: chipRow.implicitWidth + 28
        implicitHeight: 38
        radius: height / 2
        color: chipHover.hovered ? Theme.fillSecondary : Theme.fillTertiary
        border.width: 1
        border.color: chipHover.hovered ? Theme.withAlpha(chip.modelData.tint, 0.5) : "transparent"
        scale: chipTap.pressed ? 0.95 : 1
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
        Accessible.role: Accessible.Link
        Accessible.name: chip.modelData.name

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 8
            MotionIcon {
                id: chipIcon
                anchors.verticalCenter: parent.verticalCenter
                glyph: chip.modelData.glyph
                // Brand marks drawn filled (Lucide has no GitHub mark)
                font.family: Icons.filled.indexOf(chip.modelData.glyph) >= 0 ? Icons.symbolsFamily : Icons.family
                size: 16
                color: chip.modelData.tint
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: chip.modelData.name
                font.pixelSize: Dimensions.callout
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
        HoverHandler {
            id: chipHover
            cursorShape: Qt.PointingHandCursor
            onHoveredChanged: if (hovered) chipIcon.play()
        }
        TapHandler { id: chipTap; onTapped: Qt.openUrlExternally(chip.modelData.url) }
        StyledToolTip { visible: chipHover.hovered; delay: 700; text: chip.modelData.url }
    }

    // A hairline between promises, faded at both ends
    component Divider: Rectangle {
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        implicitWidth: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.separator }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
