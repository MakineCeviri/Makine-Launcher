// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * HelpScreen.qml - "Nasıl çalışır?": what the launcher does, step by step
 *
 * The page a user reaches from the sidebar's "Nasıl çalışır?" (or F1) when
 * something is not obvious: the four steps from finding a game to taking the
 * patch off again, what every word on a cover or a button means, the
 * questions people actually ask us, the shortcuts, and where to ask the rest.
 * Every sentence describes what the launcher really does — keep it so when
 * the behaviour changes.
 */
Item {
    id: help

    property real contentMargin: 16

    signal allGamesRequested()
    signal libraryRequested()

    function scrollToTop() { helpFlick.contentY = 0 }

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

    component Step: Card {
        id: step
        property int number: 1
        property string glyph: ""
        property string title: ""
        property string text: ""

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        implicitHeight: stepCol.implicitHeight + 40
        radius: Dimensions.radiusLG

        ColumnLayout {
            id: stepCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            RowLayout {
                spacing: 10
                IconTile { size: 34; glyph: step.glyph; tint: Theme.primary }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("%1. adım").arg(step.number)
                    font.pixelSize: Dimensions.footnote
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
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
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: step.text
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
        }
    }

    // A word the launcher shows, and what it means
    component Term: Item {
        id: term
        property string label: ""
        property color tone: Theme.textSecondary
        property string text: ""

        readonly property bool _isListRow: true
        property bool _isFirst: true
        property bool _isLast: true
        property bool _watched: false

        Layout.fillWidth: true
        implicitHeight: termRow.implicitHeight + 24

        Rectangle {
            visible: !term._isFirst
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            height: 1
            color: Theme.separator
        }
        RowLayout {
            id: termRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 16
            Item {
                Layout.preferredWidth: 200
                Layout.alignment: Qt.AlignTop
                implicitHeight: badge.implicitHeight
                StatusBadge { id: badge; text: term.label; tint: term.tone }
            }
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.25
                text: term.text
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
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
        implicitHeight: faqCol.implicitHeight + 28

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: faq.question
        Keys.onReturnPressed: faq.open = !faq.open
        Keys.onSpacePressed: faq.open = !faq.open

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            visible: faqHover.hovered || faq.activeFocus
            color: Theme.fillQuaternary
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
            anchors.leftMargin: 16
            height: 1
            color: Theme.separator
        }

        ColumnLayout {
            id: faqCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 14
            spacing: 8

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
                Icon {
                    glyph: Icons.chevronDown
                    size: 15
                    color: Theme.textTertiary
                    rotation: faq.open ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                Layout.fillWidth: true
                visible: faq.open
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: faq.answer
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
        }
        HoverHandler { id: faqHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: faq.open = !faq.open }
    }

    component KeyRow: ListRow {
        id: key
        property string keys: ""
        property string text: ""
        minHeight: 40
        title: key.text
        Rectangle {
            implicitWidth: Math.max(48, keyLabel.implicitWidth + 16)
            implicitHeight: 24
            radius: 6
            color: Theme.fillTertiary
            Text {
                id: keyLabel
                anchors.centerIn: parent
                textFormat: Text.PlainText
                text: key.keys
                font.pixelSize: Dimensions.caption1
                font.weight: Font.DemiBold
                color: Theme.textSecondary
            }
        }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: helpFlick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 48
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
            spacing: 16

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Nasıl çalışır?")
                subtitle: qsTr("Bilgisayarındaki oyunları bulur, Türkçe yamalarını tek tıkla kurar ve istediğin an geri alır. Aşağıda her adımda ne olduğunu, ekranda gördüğün her sözcüğün ne anlama geldiğini ve en çok sorulanları anlattık.")
            }

            // ── The four steps ──
            SectionTitle { text: qsTr("Dört adımda Türkçe oyna") }
            GridLayout {
                Layout.fillWidth: true
                columns: page.width > 900 ? 4 : 2
                columnSpacing: 14
                rowSpacing: 14

                Step {
                    number: 1
                    glyph: Icons.scanSearch
                    title: qsTr("Oyunun bulunur")
                    text: qsTr("Launcher açılınca Steam, Epic, GOG ve Xbox kütüphanelerine ve oyun klasörlerine bakar. Bulamadığı bir oyunun sayfasında \"Oyun Klasörünü Seç ve Kur\"a bas ya da klasörü pencereye sürükle: oyunun klasörü, .exe dosyasının durduğu klasör ya da içindeki herhangi bir klasör olabilir; yamanın gideceği doğru yeri launcher bulur.")
                }
                Step {
                    number: 2
                    glyph: Icons.download
                    title: qsTr("Türkçe yamayı kurarsın")
                    text: qsTr("Oyunun sayfasında \"Türkçe Yamayı Kur\"a bas. Launcher yamayı makineceviri.org'dan indirir ve bozulmadığını doğrular, değiştireceği oyun dosyalarının yedeğini alır, sonra Türkçe dosyaları oyunun klasörüne koyar. Bazı yamalarda önce seçenek ya da oyunun sürümü sorulur.")
                }
                Step {
                    number: 3
                    glyph: Icons.gamepad2
                    title: qsTr("Oyunu Türkçe oynarsın")
                    text: qsTr("Çoğu oyun hemen Türkçe açılır. Bazılarında dili oyunun ayarlarından seçmen ya da oyunu başka bir şekilde başlatman gerekir; böyle bir oyunsa launcher kurulumun sonunda ne yapman gerektiğini söyler.")
                }
                Step {
                    number: 4
                    glyph: Icons.undo2
                    title: qsTr("İstediğin an geri alırsın")
                    text: qsTr("Oyunun sayfasında \"Türkçe yama kurulu\" yazısının yanındaki \"Kaldır\"a bas. Onaylarsan yama kaldırılır ve yedeklenen orijinal dosyalar geri yüklenir. Oyun güncellenip yama bozulursa launcher bunu fark eder ve onarmayı önerir.")
                }
            }

            RowLayout {
                Layout.topMargin: 4
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
            SectionTitle { text: qsTr("Ekranda gördüklerin ne anlama geliyor?") }
            ListSection {
                Term {
                    label: qsTr("Yama hazır"); tone: Theme.primary
                    text: qsTr("Oyun bilgisayarında ve Türkçe yaması launcher'dan kurulabilir. Oyunun sayfasında ya da Kütüphanem'de \"Türkçe Yamayı Kur\"a basman yeter.")
                }
                Term {
                    label: qsTr("Kurulu"); tone: Theme.success
                    text: qsTr("Türkçe yama bu oyunda kurulu. Kaldırmak istersen oyunun sayfasındaki \"Kaldır\"ı kullan; orijinal dosyalar geri gelir.")
                }
                Term {
                    label: qsTr("Güncelleme var"); tone: Theme.warning
                    text: qsTr("Yamanın yeni bir sürümü yayımlandı. \"Güncelle\"ye basınca yeni sürüm kurulur; düzeltmeler ve yeni çeviriler gelir.")
                }
                Term {
                    label: qsTr("Bilgisayarında"); tone: Theme.primary
                    text: qsTr("Oyun bu bilgisayarda bulundu ama Türkçe yaması henüz kurulmadı.")
                }
                Term {
                    label: qsTr("Dış sitede"); tone: Theme.textMuted
                    text: qsTr("Bu oyunun Türkçe yaması başka bir çeviri ekibinin sitesinde yayımlanıyor. Düğme seni o sayfaya götürür; kurulumu o site anlatır, launcher bu yamayı kendisi kurmaz.")
                }
                Term {
                    label: qsTr("Bilgisayarında bulunamadı"); tone: Theme.warning
                    text: qsTr("Launcher oyunu bu bilgisayarda bulamadı. Oyunun sayfasında klasörünü göster; bir kez gösterdiğin klasör hatırlanır.")
                }
                Term {
                    label: qsTr("Yama oyunun klasörüne gitmemiş"); tone: Theme.warning
                    text: qsTr("Yama, oyunun kendi klasörüne değil başka bir klasöre kurulmuş; oyun onu göremez. Sayfadaki \"Yamayı taşı\" yamayı kaldırıp doğru klasöre yeniden kurar.")
                }
                Term {
                    label: qsTr("Yama dosyaları eksik"); tone: Theme.warning
                    text: qsTr("Kurulu yamanın dosyaları eksik: oyun güncellenmiş, mağaza dosyaları doğrulamış ya da antivirüs bir dosyayı silmiş olabilir. \"Yamayı Onar\"a basınca yama yeniden kurulur.")
                }
            }

            // ── Questions ──
            SectionTitle { text: qsTr("Sık sorulanlar") }
            ListSection {
                Faq {
                    question: qsTr("Oyunum listede yok, ne yapmalıyım?")
                    answer: qsTr("Önce Kütüphanem'deki \"Yeniden tara\"ya bas. Yine çıkmazsa oyunun klasörünü pencerenin herhangi bir yerine sürükleyip bırak ya da \"Oyun ekle\" ile seç: launcher hangi oyun olduğunu bulur ve sayfasını açar. Oyun kataloğumuzda yoksa Tüm Oyunlar'da aratıp yaması olup olmadığına bakabilirsin.")
                }
                Faq {
                    question: qsTr("Yama kurulu ama oyun hâlâ İngilizce.")
                    answer: qsTr("Bazı oyunlarda dili oyunun kendi ayarlarından seçmek gerekir; launcher kurulum bittiğinde bunu söyler. Söylemediyse oyunun sayfasında yamanın klasörünü kontrol et: \"Yama oyunun klasörüne gitmemiş\" uyarısı görüyorsan \"Yamayı taşı\"ya bas. Oyun yakın zamanda güncellendiyse yamanın onarılması gerekebilir.")
                }
                Faq {
                    question: qsTr("Yama kendi programını açtı, ne yapmalıyım?")
                    answer: qsTr("Bazı yamaları çevirmenin kendi programı kurar. Launcher önce oyunun klasörünü doğrular, programı o klasörle çalıştırır; programın yazdıkları oyunun sayfasında görünür, bir şey sorarsa yanıtını oradan yaz. Program kendi penceresini açtıysa kurulumu orada yapıp pencereyi kapat. Kaldırırken aynı programın geri alma komutu çalışır; o da çalışmazsa \"Kaldır\"a üçüncü kez basınca yama programsız kaldırılır ve mağazanın dosya onarımı başlar.")
                }
                Faq {
                    question: qsTr("Neden yönetici izni istendi?")
                    answer: qsTr("Oyun C:\\Program Files gibi Windows'un koruduğu bir klasördeyse oraya dosya yazmak için izin gerekir. İzin yalnızca o oyunun klasörü için kullanılır; vermezsen kurulum yapılmaz, oyuna dokunulmaz.")
                }
                Faq {
                    question: qsTr("Antivirüs bir yama dosyasını engelledi.")
                    answer: qsTr("Bazı yamalar oyuna ek bir dosya (ör. .dll) koyar; antivirüsler bunu bazen yanlışlıkla şüpheli bulur. Windows Güvenliği > Koruma geçmişi'nden dosyaya izin ver, oyunun klasörünü dışlamalara ekle ve yamayı yeniden kur. Dosyalar yalnızca makineceviri.org'dan gelir ve launcher her indirmeyi doğrular.")
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
                    question: qsTr("Online oyunlarda kullanmak güvenli mi?")
                    answer: qsTr("Hile koruması (EasyAntiCheat, BattlEye…) olan oyunlarda launcher kurulumdan önce uyarır. Bu oyunlarda yamayı yalnızca tek oyunculu modda kullan; online modda değiştirilmiş dosyalar hesabının engellenmesine yol açabilir.")
                }
                Faq {
                    question: qsTr("Yamalar ücretli mi, kim yapıyor?")
                    answer: qsTr("Hayır. Yamalar gönüllü çevirmenlerin emeğidir ve ücretsizdir; her oyunun sayfasında emeği geçenler yazar. Güvenliğin için yamaları yalnızca makineceviri.org'dan ve bu launcher'dan indir.")
                }
                Faq {
                    question: qsTr("Bir yamayı kaldırınca oyunuma ne olur?")
                    answer: qsTr("Kurulumda değişen oyun dosyalarının yedeği alınmıştı; kaldırınca bu yedek geri yüklenir ve yamanın eklediği dosyalar silinir. Oyunun kayıtlarına ve ayarlarına dokunulmaz.")
                }
            }

            // ── Shortcuts ──
            SectionTitle { text: qsTr("Kısayollar") }
            GridLayout {
                Layout.fillWidth: true
                columns: page.width > 760 ? 2 : 1
                columnSpacing: 14
                rowSpacing: 14

                ListSection {
                    Layout.alignment: Qt.AlignTop
                    KeyRow { keys: "Ctrl+1"; text: qsTr("Ana Sayfa") }
                    KeyRow { keys: "Ctrl+2"; text: qsTr("Kütüphanem") }
                    KeyRow { keys: "Ctrl+3"; text: qsTr("Tüm Oyunlar") }
                    KeyRow { keys: "Ctrl+F"; text: qsTr("Oyun ara") }
                }
                ListSection {
                    Layout.alignment: Qt.AlignTop
                    KeyRow { keys: "Ctrl+R"; text: qsTr("Kataloğu yenile, oyunları yeniden tara") }
                    KeyRow { keys: "Ctrl+,"; text: qsTr("Ayarlar") }
                    KeyRow { keys: "F1"; text: qsTr("Bu sayfa") }
                    KeyRow { keys: "Esc"; text: qsTr("Geri dön") }
                }
            }

            // ── Still stuck ──
            SectionTitle { text: qsTr("Hâlâ takıldın mı?") }
            Body {
                text: qsTr("Topluluğumuz ve çevirmenlerimiz Discord'da. Sorununu yazarken oyunun adını ve gördüğün hata mesajını eklersen daha hızlı yardım alırsın.")
            }
            RowLayout {
                Layout.bottomMargin: 8
                spacing: 10
                AppButton {
                    style: "filled"
                    icon: Icons.messagesSquare
                    text: qsTr("Discord'a katıl")
                    onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                }
                AppButton {
                    style: "gray"
                    icon: Icons.globe
                    text: qsTr("makineceviri.org")
                    onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
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
