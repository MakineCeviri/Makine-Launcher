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
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        spacing: 6

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            source: "qrc:/qt/qml/MakineLauncher/resources/images/logo.png"
            sourceSize: Qt.size(144, 144)
            fillMode: Image.PreserveAspectFit
            mipmap: true
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
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 460
            Layout.topMargin: 4
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: qsTr("Bilgisayarındaki oyunları bulur, Türkçe yamalarını tek tıkla kurar ve kaldırır. Makine Çeviri topluluğunun ücretsiz aracıdır.")
            font.pixelSize: Dimensions.callout
            color: Theme.textSecondary
        }
    }

    ListSection {
        title: qsTr("Bu kopya")

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
            icon: Icons.messagesSquare
            iconTint: Theme.discordColor
            clickable: true
            title: qsTr("Discord")
            subtitle: qsTr("Bir yama çalışmıyorsa ya da bir sorun varsa topluluğa sor; ekip de orada.")
            onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
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
                { key: "Ctrl+R", desc: qsTr("Oyunları ve yamaları yeniden denetle") },
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
                { name: "Lucide", license: "ISC", url: "https://github.com/lucide-icons/lucide/blob/main/LICENSE" }
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
}
