// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * HelpScreen.qml - "Nasıl çalışır?": what the launcher does, at a glance
 *
 * The page a user reaches from the sidebar's "Nasıl çalışır?" (or F1) when
 * something is not obvious. Built to be scanned, not read through: the four
 * steps in a sentence each, every word a cover or a button shows in one
 * line, the questions people actually ask (closed until opened, in three
 * groups), the shortcuts as keys, and where to ask the rest. Pills under the
 * title jump to each part. Every sentence describes what the launcher really
 * does — keep it so when the behaviour changes.
 */
Item {
    id: help

    property real contentMargin: 16

    signal allGamesRequested()
    signal libraryRequested()

    function scrollToTop() { helpFlick.contentY = 0 }

    // The part in view (-1 above the first): its pill is lit
    readonly property int _inView: {
        const parts = [stepsPart, legendPart, faqPart, keysPart, supportPart]
        if (helpFlick.atYEnd && helpFlick.contentY > 0) return parts.length - 1
        const line = helpFlick.contentY + 200
        let at = -1
        for (let i = 0; i < parts.length; ++i)
            if (parts[i].y <= line) at = i
        return at
    }

    // Scroll so a part's title sits under the top strip
    function _jump(target) {
        // Where it is on screen now, plus how far the page is scrolled
        const y = helpFlick.contentY + target.mapToItem(helpFlick, 0, 0).y - 28
        jump.to = Math.max(0, Math.min(y, helpFlick.contentHeight - helpFlick.height))
        jump.restart()
    }
    NumberAnimation {
        id: jump
        target: helpFlick
        property: "contentY"
        duration: Dimensions.animSlow
        easing.type: Easing.OutCubic
    }

    // ── Small parts ──────────────────────────────────────────────────────────

    // A part's title, and a line on what it holds
    component Chapter: ColumnLayout {
        id: section
        property string title: ""
        property string text: ""
        Layout.fillWidth: true
        Layout.topMargin: 40
        Layout.bottomMargin: 14
        spacing: 4
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: section.title
            font.family: Dimensions.fontDisplay
            font.pixelSize: Dimensions.title2
            font.weight: Font.Bold
            font.letterSpacing: Dimensions.trackingHeadline
            color: Theme.textPrimary
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        Text {
            visible: section.text !== ""
            Layout.fillWidth: true
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: section.text
            font.pixelSize: Dimensions.callout
            color: Theme.textMuted
        }
    }

    // One of the four steps: its number, its sign, a title and one sentence
    component Step: Card {
        id: step
        property int number: 1
        property string glyph: ""
        property string title: ""
        property string text: ""

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        implicitHeight: stepCol.implicitHeight + 44
        radius: Dimensions.radiusSection

        ColumnLayout {
            id: stepCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 18
                IconTile { size: 42; glyph: step.glyph; tint: Theme.primary }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.alignment: Qt.AlignTop
                    textFormat: Text.PlainText
                    text: step.number
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: Dimensions.title1
                    font.weight: Font.Bold
                    color: Theme.withAlpha(Theme.textPrimary, 0.14)
                }
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                text: step.title
                font.pixelSize: Dimensions.headline
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.25
                text: step.text
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
        }
    }

    // A word the launcher shows, and under it what it means in a line
    component Legend: ColumnLayout {
        id: legend
        property string label: ""
        property color tone: Theme.textSecondary
        property string text: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
        spacing: 8
        StatusBadge { text: legend.label; tint: legend.tone }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 2
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: legend.text
            font.pixelSize: Dimensions.callout
            color: Theme.textSecondary
        }
    }

    // A question that opens to its answer
    component Faq: Item {
        id: faq
        property string question: ""
        property string answer: ""
        property bool open: false

        readonly property bool _isListRow: true
        property bool _isFirst: true
        property bool _isLast: true
        property bool _watched: false

        Layout.fillWidth: true
        implicitHeight: faqCol.implicitHeight + 30

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: faq.question
        Keys.onReturnPressed: faq.open = !faq.open
        Keys.onSpacePressed: faq.open = !faq.open

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            visible: faqHover.hovered || faq.activeFocus || faq.open
            color: faq.open ? Theme.withAlpha(Theme.primary, faqHover.hovered ? 0.08 : 0.05) : Theme.fillQuaternary
            topLeftRadius: faq._isFirst ? Dimensions.radiusLG - 1 : 0
            topRightRadius: faq._isFirst ? Dimensions.radiusLG - 1 : 0
            bottomLeftRadius: faq._isLast ? Dimensions.radiusLG - 1 : 0
            bottomRightRadius: faq._isLast ? Dimensions.radiusLG - 1 : 0
        }
        Rectangle {
            visible: !faq._isFirst
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 18
            height: 1
            color: Theme.separator
        }

        // An open question carries an accent mark on its left
        Rectangle {
            x: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: faq.open ? parent.height - 26 : 0
            radius: 1.5
            color: Theme.primary
            opacity: faq.open ? 1 : 0
            Behavior on height { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        }

        ColumnLayout {
            id: faqCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 18
            anchors.rightMargin: 16
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    text: faq.question
                    font.pixelSize: Dimensions.body
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Rectangle {
                    implicitWidth: 24
                    implicitHeight: 24
                    radius: 12
                    color: faq.open ? Theme.withAlpha(Theme.primary, 0.16) : Theme.fillTertiary
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Icon {
                        anchors.centerIn: parent
                        glyph: Icons.chevronDown
                        size: 14
                        color: faq.open ? Theme.primaryText : Theme.textTertiary
                        rotation: faq.open ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                    }
                }
            }
            // The answer unfolds: the room for it grows, the words settle in
            Item {
                Layout.fillWidth: true
                implicitHeight: faq.open ? answerText.implicitHeight + 10 : 0
                clip: true
                Behavior on implicitHeight { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                Text {
                    id: answerText
                    width: parent.width - 36
                    y: faq.open ? 10 : 2
                    opacity: faq.open ? 1 : 0
                    Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutExpo } }
                    Behavior on opacity { NumberAnimation { duration: faq.open ? Dimensions.animSlow : Dimensions.animFast } }
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    text: faq.answer
                    font.pixelSize: Dimensions.callout
                    color: Theme.textSecondary
                }
            }
        }
        HoverHandler { id: faqHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: faq.open = !faq.open }
    }

    // A key on the keyboard, a little raised
    component Keycap: Rectangle {
        id: cap
        property string label: ""
        implicitWidth: Math.max(28, capText.implicitWidth + 16)
        implicitHeight: 28
        radius: 7
        color: Theme.fillTertiary
        border.width: 1
        border.color: Theme.separator
        // The key's lower lip
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            height: 2
            radius: 1
            color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.35 : 0.12)
        }
        Text {
            id: capText
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            textFormat: Text.PlainText
            text: cap.label
            font.pixelSize: Dimensions.footnote
            font.family: Dimensions.fontText
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }
    }

    // A shortcut: its keys, then what it does
    component KeyShortcut: RowLayout {
        id: shortcut
        property string keys: ""
        property string text: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 12
        Row {
            spacing: 4
            Repeater {
                model: shortcut.keys.split(" ")
                Keycap {
                    required property string modelData
                    label: modelData
                }
            }
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            elide: Text.ElideRight
            text: shortcut.text
            font.pixelSize: Dimensions.callout
            color: Theme.textSecondary
        }
    }

    // A pill under the title that jumps to a part
    // The part in view lights its pill; a pill's icon moves under the
    // pointer and when it is chosen, and the pill rises a little
    component JumpPill: Rectangle {
        id: pill
        property string glyph: ""
        property string text: ""
        property bool active: false
        signal clicked()
        implicitWidth: pillRow.implicitWidth + 28
        implicitHeight: 36
        radius: height / 2
        color: pill.active ? Theme.withAlpha(Theme.primary, pillHover.hovered ? 0.24 : 0.18)
             : pillHover.hovered ? Theme.fillSecondary : Theme.fillTertiary
        border.width: 1
        border.color: pill.active ? Theme.withAlpha(Theme.primary, 0.45) : "transparent"
        Behavior on color { ColorAnimation { duration: Dimensions.animNormal } }
        Behavior on border.color { ColorAnimation { duration: Dimensions.animNormal } }
        transform: Translate {
            y: pillHover.hovered && !pillTap.pressed ? -2 : 0
            Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }
        }
        scale: pillTap.pressed ? 0.95 : 1
        Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
        onActiveChanged: if (active) pillIcon.play()
        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 7
            MotionIcon {
                id: pillIcon
                anchors.verticalCenter: parent.verticalCenter
                glyph: pill.glyph
                size: 15
                color: Theme.primaryText
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: pill.text
                font.pixelSize: Dimensions.callout
                font.weight: pill.active ? Font.DemiBold : Font.Medium
                color: pill.active ? Theme.primaryText : Theme.textPrimary
            }
        }
        HoverHandler {
            id: pillHover
            cursorShape: Qt.PointingHandCursor
            onHoveredChanged: if (hovered) pillIcon.play()
        }
        MouseArea {
            id: pillTap
            anchors.fill: parent
            onClicked: pill.clicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: pill.text
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: helpFlick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 64
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: helpFlick.parent
            anchors.top: helpFlick.top; anchors.right: helpFlick.right; anchors.bottom: helpFlick.bottom
        }

        ColumnLayout {
            id: page
            width: Math.min(helpFlick.width - Dimensions.pageMargin * 2, Dimensions.contentMaxWidth)
            x: (helpFlick.width - width) / 2
            spacing: 0

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Nasıl çalışır?")
                subtitle: qsTr("Oyununu bulur, Türkçe yamasını tek tıkla kurar, istediğin an geri alır.")
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 20
                spacing: 8
                JumpPill { glyph: Icons.listOrdered; text: qsTr("Adımlar"); active: help._inView === 0; onClicked: help._jump(stepsPart) }
                JumpPill { glyph: Icons.tag; text: qsTr("Ekrandakiler"); active: help._inView === 1; onClicked: help._jump(legendPart) }
                JumpPill { glyph: Icons.messageCircle; text: qsTr("Sık sorulanlar"); active: help._inView === 2; onClicked: help._jump(faqPart) }
                JumpPill { glyph: Icons.keyboard; text: qsTr("Kısayollar"); active: help._inView === 3; onClicked: help._jump(keysPart) }
                JumpPill { glyph: Icons.lifeBuoy; text: qsTr("Yardım al"); active: help._inView === 4; onClicked: help._jump(supportPart) }
            }

            // ── The four steps ──
            Chapter {
                id: stepsPart
                title: qsTr("Dört adımda Türkçe oyna")
            }
            GridLayout {
                Layout.fillWidth: true
                columns: page.width > 900 ? 4 : 2
                columnSpacing: 14
                rowSpacing: 14

                Step {
                    number: 1
                    glyph: Icons.scanSearch
                    title: qsTr("Oyununu bulur")
                    text: qsTr("Steam, Epic, GOG ve Xbox'taki oyunlarını kendiliğinden bulur. Bulamadığının klasörünü pencereye sürüklemen yeter.")
                }
                Step {
                    number: 2
                    glyph: Icons.download
                    title: qsTr("Yamayı kurarsın")
                    text: qsTr("\"Türkçe Yamayı Kur\"a basarsın. Launcher yamayı indirir, doğrular ve değişecek dosyaların yedeğini alır.")
                }
                Step {
                    number: 3
                    glyph: Icons.gamepad2
                    title: qsTr("Türkçe oynarsın")
                    text: qsTr("Çoğu oyun hemen Türkçe açılır. Dili ayarlardan seçmek gerekiyorsa kurulumun sonunda söylenir.")
                }
                Step {
                    number: 4
                    glyph: Icons.undo2
                    title: qsTr("İstediğin an geri alırsın")
                    text: qsTr("\"Kaldır\" yamayı siler, yedeklenen orijinal dosyaları geri koyar.")
                }
            }
            RowLayout {
                Layout.topMargin: 16
                spacing: 10
                AppButton {
                    style: "filled"
                    icon: Icons.layoutGrid
                    text: qsTr("Tüm oyunlara göz at")
                    onClicked: help.allGamesRequested()
                }
                AppButton {
                    style: "gray"
                    icon: Icons.gamepad2
                    text: qsTr("Kütüphanemi aç")
                    onClicked: help.libraryRequested()
                }
            }

            // ── Words on screen ──
            Chapter {
                id: legendPart
                title: qsTr("Ekranda gördüklerin")
                text: qsTr("Kapakların ve oyun sayfalarının üzerindeki işaretler.")
            }
            Card {
                Layout.fillWidth: true
                implicitHeight: legendGrid.implicitHeight + 44
                radius: Dimensions.radiusSection

                GridLayout {
                    id: legendGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    columns: page.width > 900 ? 4 : 2
                    columnSpacing: 28
                    rowSpacing: 22

                    Legend {
                        label: qsTr("Yama hazır"); tone: Theme.primary
                        text: qsTr("Oyun bilgisayarında; yamayı tek tıkla kurabilirsin.")
                    }
                    Legend {
                        label: qsTr("Kurulu"); tone: Theme.success
                        text: qsTr("Türkçe yama bu oyunda kurulu.")
                    }
                    Legend {
                        label: qsTr("Güncelleme var"); tone: Theme.warning
                        text: qsTr("Yamanın yeni sürümü çıktı; \"Güncelle\" onu kurar.")
                    }
                    Legend {
                        label: qsTr("Bilgisayarında"); tone: Theme.primary
                        text: qsTr("Oyun bulundu, yaması henüz kurulmadı.")
                    }
                    Legend {
                        label: qsTr("Dış sitede"); tone: Theme.textMuted
                        text: qsTr("Yama başka bir ekibin sitesinde; kurulumu o site anlatır.")
                    }
                    Legend {
                        label: qsTr("Bilgisayarında bulunamadı"); tone: Theme.warning
                        text: qsTr("Klasörünü bir kez göster; launcher hatırlar.")
                    }
                    Legend {
                        label: qsTr("Yama oyunun klasörüne gitmemiş"); tone: Theme.warning
                        text: qsTr("\"Yamayı taşı\" yamayı doğru klasöre yeniden kurar.")
                    }
                    Legend {
                        label: qsTr("Yama dosyaları eksik"); tone: Theme.warning
                        text: qsTr("\"Yamayı Onar\" yamayı yeniden kurar.")
                    }
                }
            }

            // ── Questions ──
            Chapter {
                id: faqPart
                title: qsTr("Sık sorulanlar")
                text: qsTr("Bir soruya dokun, yanıtı açılsın.")
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 22

                ListSection {
                    title: qsTr("Kurulum")
                    Faq {
                        question: qsTr("Oyunum listede yok, ne yapmalıyım?")
                        answer: qsTr("Önce Kütüphanem'deki \"Yeniden tara\"ya bas. Yine çıkmazsa oyunun klasörünü pencerenin herhangi bir yerine sürükleyip bırak ya da \"Oyun ekle\" ile seç: launcher hangi oyun olduğunu bulur ve sayfasını açar. Oyun kataloğumuzda yoksa Tüm Oyunlar'da aratıp yaması olup olmadığına bakabilirsin.")
                    }
                    Faq {
                        question: qsTr("Neden yönetici izni istendi?")
                        answer: qsTr("Oyun C:\\Program Files gibi Windows'un koruduğu bir klasördeyse oraya dosya yazmak için izin gerekir. İzin yalnızca o oyunun klasörü için kullanılır; vermezsen kurulum yapılmaz, oyuna dokunulmaz.")
                    }
                    Faq {
                        question: qsTr("Yama kendi programını açtı, ne yapmalıyım?")
                        answer: qsTr("Bazı yamaları çevirmenin kendi programı kurar. Launcher önce oyunun klasörünü doğrular, programı o klasörle çalıştırır; programın yazdıkları oyunun sayfasında görünür, bir şey sorarsa yanıtını oradan yaz. Program kendi penceresini açtıysa kurulumu orada yapıp pencereyi kapat. Kaldırırken aynı programın geri alma komutu çalışır; o da çalışmazsa \"Kaldır\"a üçüncü kez basınca yama programsız kaldırılır ve mağazanın dosya onarımı başlar.")
                    }
                    Faq {
                        question: qsTr("Bir yamayı kaldırınca oyunuma ne olur?")
                        answer: qsTr("Kurulumda değişen oyun dosyalarının yedeği alınmıştı; kaldırınca bu yedek geri yüklenir ve yamanın eklediği dosyalar silinir. Oyunun kayıtlarına ve ayarlarına dokunulmaz.")
                    }
                }

                ListSection {
                    title: qsTr("Bir sorun mu var?")
                    Faq {
                        question: qsTr("Yama kurulu ama oyun hâlâ İngilizce.")
                        answer: qsTr("Bazı oyunlarda dili oyunun kendi ayarlarından seçmek gerekir; launcher kurulum bittiğinde bunu söyler. Söylemediyse oyunun sayfasında yamanın klasörünü kontrol et: \"Yama oyunun klasörüne gitmemiş\" uyarısı görüyorsan \"Yamayı taşı\"ya bas. Oyun yakın zamanda güncellendiyse yamanın onarılması gerekebilir.")
                    }
                    Faq {
                        question: qsTr("Oyun güncellendi, yama çalışmıyor.")
                        answer: qsTr("Oyun güncellemesi yamanın dosyalarının üzerine yazmış olabilir. Oyunun sayfasını aç: launcher eksik dosyaları fark ederse \"Yama dosyaları eksik\" der, \"Yamayı Onar\"a basınca yama yeniden kurulur. Güncelleme yamayı tamamen bozduysa yeni yama sürümü çıkınca \"Güncelleme var\" görürsün.")
                    }
                    Faq {
                        question: qsTr("Steam'de \"dosyaları doğrula\" yaparsam ne olur?")
                        answer: qsTr("Steam oyunu orijinal hâline getirir ve Türkçe dosyaları siler. Bir dahaki sefere oyunun sayfasında launcher bunu görür ve yamayı yeniden kurmayı önerir. Yama kurulurken bir sorun çıkarsa da sayfadaki \"Oyun dosyalarını doğrula\" oyunu temiz hâline döndürmenin en kolay yoludur.")
                    }
                    Faq {
                        question: qsTr("Antivirüs bir yama dosyasını engelledi.")
                        answer: qsTr("Bazı yamalar oyuna ek bir dosya (ör. .dll) koyar; antivirüsler bunu bazen yanlışlıkla şüpheli bulur. Windows Güvenliği > Koruma geçmişi'nden dosyaya izin ver, oyunun klasörünü dışlamalara ekle ve yamayı yeniden kur. Dosyalar yalnızca makineceviri.org'dan gelir ve launcher her indirmeyi doğrular.")
                    }
                }

                ListSection {
                    title: qsTr("Güvenlik ve diğerleri")
                    Faq {
                        question: qsTr("Online oyunlarda kullanmak güvenli mi?")
                        answer: qsTr("Hile koruması (EasyAntiCheat, BattlEye…) olan oyunlarda launcher kurulumdan önce uyarır. Bu oyunlarda yamayı yalnızca tek oyunculu modda kullan; online modda değiştirilmiş dosyalar hesabının engellenmesine yol açabilir.")
                    }
                    Faq {
                        question: qsTr("Yamalar ücretli mi, kim yapıyor?")
                        answer: qsTr("Hayır. Yamalar gönüllü çevirmenlerin emeğidir ve ücretsizdir; her oyunun sayfasında emeği geçenler yazar. Güvenliğin için yamaları yalnızca makineceviri.org'dan ve bu launcher'dan indir.")
                    }
                    Faq {
                        visible: Companion.available
                        question: qsTr("Launcher'daki küçük robot kim?")
                        answer: qsTr("O Makine, launcher'ın yoldaşı. Ana sayfada öne çıkan oyunların yanındaki kendi köşesinde durur; başka bir sayfaya geçince ya da sayfayı aşağı kaydırınca kenar çubuğuna uçar, dönünce ışınlanarak köşesine gelir. Yaptığın işe eşlik eder: oyunlarını ararken bakınır, yama inerken ilerlemeyi yüzünde gösterir, kurulum bitince sevinir, bir şey ters giderse söyler. Tıklarsan tepki verir; başından ya da elinden tutup sürükleyebilir, fırlatabilirsin. Pencerenin dışına bırakırsan yerine uçarak döner. Üstüne sağ tıklayınca bayrak sallatabilir, ayarlarını açabilir, bir saatliğine gizleyebilir ya da nerede duracağını seçebilirsin: launcher'da, masaüstünde ya da en küçük haliyle sağ altta saatin üstünde. Bir pencere açıldığında yolundan çekilir, bir süre onunla ilgilenmezsen olduğu yerde uyur, sayfa değişse de kımıldamaz, tıklarsan uyanır; launcher başka bir pencerenin altında kaldığında hiç çizilmez, bilgisayarını yormaz. Ayarlar > Görünüm > Makine'den kapatıp açabilir; yerini, ana sayfadaki köşesini, görünüşünü (elleriyle ya da sade), boyunu ve konuşmasını değiştirebilirsin.")
                    }
                    Faq {
                        question: qsTr("Launcher'ın yeni sürümünü nasıl alırım?")
                        answer: qsTr("Yeni sürüm çıkınca kenar çubuğunun altında haber verilir; neyin değiştiğini kenar çubuğundaki Yenilikler'de okuyabilirsin. Launcher'ı Microsoft Store'dan kurduysan \"Store'da güncelle\" Store'u açar, güncelleme oradan gelir. Arşivden (ZIP) kurduysan \"İndir\" indirme sayfasını açar: yeni arşivi eskisinin üzerine çıkar. İki yolda da ayarların ve kurulu yamaların olduğu gibi kalır.")
                    }
                }
            }

            // ── Shortcuts ──
            Chapter {
                id: keysPart
                title: qsTr("Kısayollar")
                text: qsTr("Farenin yan tuşları da geri ve ileri gider.")
            }
            Card {
                Layout.fillWidth: true
                implicitHeight: keysGrid.implicitHeight + 44
                radius: Dimensions.radiusSection

                GridLayout {
                    id: keysGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    columns: page.width > 900 ? 3 : 2
                    columnSpacing: 28
                    rowSpacing: 16

                    KeyShortcut { keys: "Ctrl 1"; text: qsTr("Ana Sayfa") }
                    KeyShortcut { keys: "Ctrl 2"; text: qsTr("Kütüphanem") }
                    KeyShortcut { keys: "Ctrl 3"; text: qsTr("Tüm Oyunlar") }
                    KeyShortcut { keys: "Ctrl F"; text: qsTr("Oyun ara") }
                    KeyShortcut { keys: "Ctrl R"; text: qsTr("Kataloğu yenile, yeniden tara") }
                    KeyShortcut { keys: "Ctrl ,"; text: qsTr("Ayarlar") }
                    KeyShortcut { keys: "Alt ←"; text: qsTr("Geri") }
                    KeyShortcut { keys: "Alt →"; text: qsTr("İleri") }
                    KeyShortcut { keys: "F1"; text: qsTr("Bu sayfa") }
                }
            }

            // ── Still stuck ──
            Card {
                id: supportPart
                Layout.fillWidth: true
                Layout.topMargin: 40
                implicitHeight: supportRow.implicitHeight + 48
                radius: Dimensions.radiusSection
                glow: true
                glowCorner: "bottomLeft"

                RowLayout {
                    id: supportRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 18

                    IconTile {
                        Layout.alignment: Qt.AlignVCenter
                        size: 52
                        glyph: Icons.discord
                        tint: Theme.discordColor
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Hâlâ takıldın mı?")
                            font.family: Dimensions.fontDisplay
                            font.pixelSize: Dimensions.title3
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            text: qsTr("Topluluğumuz ve çevirmenlerimiz Discord'da. Oyunun adını ve gördüğün hata mesajını yazarsan daha hızlı yardım alırsın. Kurulumu adım adım izlemek istersen YouTube kanalımıza bak.")
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                        }
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignVCenter
                        style: "gray"
                        icon: Icons.youtube
                        text: qsTr("Kurulum videoları")
                        onClicked: Qt.openUrlExternally(Dimensions.youtubeUrl)
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignVCenter
                        style: "filled"
                        tint: Theme.discordColor
                        icon: Icons.discord
                        text: qsTr("Discord'a katıl")
                        onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                    }
                }
            }
        }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: helpFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
