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
 * The page a user reaches from the "?" in the navigation bar (or F1) when
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

    function scrollToTop() { flick.contentY = 0 }

    // ── Small parts ──────────────────────────────────────────────────────────
    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: Dimensions.spacingXL
        textFormat: Text.PlainText
        font.pixelSize: Dimensions.fontXL
        font.weight: Font.Bold
        color: Theme.textPrimary
    }

    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        lineHeight: 1.45
        font.pixelSize: Dimensions.fontMD
        color: Theme.textSecondary
    }

    component Step: Rectangle {
        id: step
        property int number: 1
        property string glyph: ""
        property string title: ""
        property string text: ""

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        implicitHeight: stepCol.implicitHeight + Dimensions.paddingXL * 2
        radius: Dimensions.radiusLG
        color: Theme.textPrimary03
        border.width: 1
        border.color: Theme.textPrimary06

        ColumnLayout {
            id: stepCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Dimensions.paddingXL
            spacing: Dimensions.spacingLG

            RowLayout {
                spacing: Dimensions.spacingLG
                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 22
                    color: Theme.primary15
                    Text {
                        anchors.centerIn: parent
                        textFormat: Text.PlainText
                        text: step.glyph
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 18
                        color: Theme.accentLight
                    }
                }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("ADIM %1").arg(step.number)
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.4
                    color: Theme.textMuted
                }
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                text: step.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.45
                text: step.text
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
            }
        }
    }

    // A word the launcher shows, and what it means
    component Term: RowLayout {
        id: term
        property string label: ""
        property color tone: Theme.textSecondary
        property string text: ""

        Layout.fillWidth: true
        spacing: Dimensions.spacingXL

        Item {
            Layout.preferredWidth: 220
            Layout.alignment: Qt.AlignTop
            implicitHeight: pill.height

            Rectangle {
                id: pill
                width: pillRow.implicitWidth + 20
                height: 28
                radius: 14
                color: Theme.withAlpha(term.tone, 0.12)
                Row {
                    id: pillRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7; height: 7; radius: 3.5
                        color: term.tone
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: term.label
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 4
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            lineHeight: 1.4
            text: term.text
            font.pixelSize: Dimensions.fontMD
            color: Theme.textSecondary
        }
    }

    // A question that opens to its answer
    component Faq: Rectangle {
        id: faq
        property string question: ""
        property string answer: ""
        property bool open: false

        Layout.fillWidth: true
        implicitHeight: faqCol.implicitHeight + Dimensions.paddingLG * 2
        radius: Dimensions.radiusLG
        color: faqHover.hovered || faq.open ? Theme.textPrimary06 : Theme.textPrimary03
        border.width: faq.activeFocus ? 2 : 1
        border.color: faq.activeFocus ? Theme.accentLight : Theme.textPrimary06
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: faq.question
        Keys.onReturnPressed: faq.open = !faq.open
        Keys.onSpacePressed: faq.open = !faq.open

        ColumnLayout {
            id: faqCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Dimensions.paddingLG
            anchors.leftMargin: Dimensions.paddingXL
            spacing: Dimensions.spacingMD

            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingLG
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    text: faq.question
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: faq.open ? "\uE70E" : "\uE70D"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
            }
            Text {
                Layout.fillWidth: true
                visible: faq.open
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.45
                text: faq.answer
                font.pixelSize: Dimensions.fontMD
                color: Theme.textSecondary
            }
        }
        HoverHandler { id: faqHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: faq.open = !faq.open }
    }

    component KeyRow: RowLayout {
        id: key
        property string keys: ""
        property string text: ""
        Layout.fillWidth: true
        spacing: Dimensions.spacingLG
        Rectangle {
            Layout.preferredWidth: Math.max(64, keyLabel.implicitWidth + 20)
            implicitHeight: 28
            radius: Dimensions.radiusMD
            color: Theme.textPrimary06
            border.width: 1
            border.color: Theme.textPrimary12
            Text {
                id: keyLabel
                anchors.centerIn: parent
                textFormat: Text.PlainText
                text: key.keys
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: key.text
            font.pixelSize: Dimensions.fontMD
            color: Theme.textSecondary
        }
    }

    component LinkButton: Rectangle {
        id: lb
        property string glyph: ""
        property string text: ""
        property bool filled: false
        signal clicked()

        implicitWidth: lbRow.implicitWidth + Dimensions.paddingXL * 1.5
        implicitHeight: 40
        radius: Dimensions.radiusMD
        color: lb.filled ? (lbMouse.containsMouse ? Theme.primaryHover : Theme.primary)
                         : (lbMouse.containsMouse ? Theme.textPrimary12 : Theme.textPrimary06)
        border.width: lb.activeFocus ? 2 : 0
        border.color: Theme.accentLight
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: lb.text
        Keys.onReturnPressed: lb.clicked()
        Keys.onSpacePressed: lb.clicked()

        Row {
            id: lbRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingMD
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: lb.glyph !== ""
                textFormat: Text.PlainText
                text: lb.glyph
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 13
                color: lb.filled ? Theme.textOnColor : Theme.textSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: lb.text
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold
                color: lb.filled ? Theme.textOnColor : Theme.textPrimary
            }
        }
        MouseArea {
            id: lbMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: lb.clicked()
        }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: help.contentMargin
        contentHeight: page.implicitHeight + Dimensions.spacingPage
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: StyledScrollBar {}

        SectionContainer {
            id: page
            width: Math.min(flick.width, Dimensions.contentMaxWidth)
            x: (flick.width - width) / 2
            glowPosition: "top-right"
            contentSpacing: Dimensions.spacingLG

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: qsTr("Makine Launcher nasıl çalışır?")
                font.pixelSize: Dimensions.fontHero
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Body {
                text: qsTr("Bilgisayarındaki oyunları bulur, Türkçe yamalarını tek tıkla kurar ve istediğin an geri alır. Aşağıda her adımda ne olduğunu, ekranda gördüğün her sözcüğün ne anlama geldiğini ve en çok sorulanları anlattık.")
            }

            // ── The four steps ──
            SectionTitle { text: qsTr("Dört adımda Türkçe oyna") }
            GridLayout {
                Layout.fillWidth: true
                columns: help.width > 1000 ? 4 : 2
                columnSpacing: Dimensions.spacingLG
                rowSpacing: Dimensions.spacingLG

                Step {
                    number: 1
                    glyph: "\uE721"
                    title: qsTr("Oyunun bulunur")
                    text: qsTr("Launcher açılınca Steam, Epic, GOG ve Xbox kütüphanelerine ve oyun klasörlerine bakar. Bulamadığı bir oyunun sayfasında \"Oyun Klasörünü Seç ve Kur\"a bas ya da klasörü pencereye sürükle: oyunun klasörü, .exe dosyasının durduğu klasör ya da içindeki herhangi bir klasör olabilir; yamanın gideceği doğru yeri launcher bulur.")
                }
                Step {
                    number: 2
                    glyph: "\uE896"
                    title: qsTr("Türkçe yamayı kurarsın")
                    text: qsTr("Oyunun sayfasındaki mor düğmeye bas. Launcher yamayı makineceviri.org'dan indirir ve bozulmadığını doğrular, değiştireceği oyun dosyalarının yedeğini alır, sonra Türkçe dosyaları oyunun klasörüne koyar. Bazı yamalarda önce seçenek ya da oyunun sürümü sorulur.")
                }
                Step {
                    number: 3
                    glyph: "\uE7FC"
                    title: qsTr("Oyunu Türkçe oynarsın")
                    text: qsTr("Çoğu oyun hemen Türkçe açılır. Bazılarında dili oyunun ayarlarından seçmen ya da oyunu başka bir şekilde başlatman gerekir; böyle bir oyunsa launcher kurulumun sonunda ne yapman gerektiğini söyler.")
                }
                Step {
                    number: 4
                    glyph: "\uE7A7"
                    title: qsTr("İstediğin an geri alırsın")
                    text: qsTr("Oyunun sayfasında \"Türkçe Yama Kurulu\" düğmesinin üzerine gelince \"Yamayı Kaldır\" olur. Onaylarsan yama kaldırılır ve yedeklenen orijinal dosyalar geri yüklenir. Oyun güncellenip yama bozulursa launcher bunu fark eder ve onarmayı önerir.")
                }
            }

            RowLayout {
                Layout.topMargin: Dimensions.spacingSM
                spacing: Dimensions.spacingMD
                LinkButton {
                    filled: true
                    glyph: "\uE8F1"
                    text: qsTr("Tüm oyunlara göz at")
                    onClicked: help.allGamesRequested()
                }
                LinkButton {
                    glyph: "\uF12B"
                    text: qsTr("Kütüphanemi aç")
                    onClicked: help.libraryRequested()
                }
            }

            // ── Words on screen ──
            SectionTitle { text: qsTr("Ekranda gördüklerin ne anlama geliyor?") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingLG

                Term {
                    label: qsTr("Yama hazır"); tone: Theme.accentLight
                    text: qsTr("Oyun bilgisayarında ve Türkçe yaması launcher'dan kurulabilir. Oyunun sayfasında ya da Kütüphanem'de \"Türkçe Yamayı Kur\"a basman yeter.")
                }
                Term {
                    label: qsTr("Kurulu"); tone: Theme.success
                    text: qsTr("Türkçe yama bu oyunda kurulu. Kaldırmak istersen oyunun sayfasındaki düğmeyi kullan; orijinal dosyalar geri gelir.")
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
                    label: qsTr("Dış sitede"); tone: Theme.textSecondary
                    text: qsTr("Bu oyunun Türkçe yaması başka bir çeviri ekibinin sitesinde yayımlanıyor. Düğme seni o sayfaya götürür; kurulumu o site anlatır, launcher bu yamayı kendisi kurmaz.")
                }
                Term {
                    label: qsTr("Tespit edilemedi"); tone: Theme.warning
                    text: qsTr("Launcher oyunu bu bilgisayarda bulamadı. Oyunun sayfasında klasörünü göster; bir kez gösterdiğin klasör hatırlanır.")
                }
                Term {
                    label: qsTr("Yanlış klasörde"); tone: Theme.warning
                    text: qsTr("Yama, oyunun kendi klasörüne değil başka bir klasöre kurulmuş; oyun onu göremez. Sayfadaki \"Yamayı taşı\" yamayı kaldırıp doğru klasöre yeniden kurar.")
                }
                Term {
                    label: qsTr("Onarım gerekli"); tone: Theme.warning
                    text: qsTr("Kurulu yamanın dosyaları eksik: oyun güncellenmiş, mağaza dosyaları doğrulamış ya da antivirüs bir dosyayı silmiş olabilir. Düğmeye basınca yama yeniden kurulur.")
                }
            }

            // ── Questions ──
            SectionTitle { text: qsTr("Sık sorulanlar") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingMD

                Faq {
                    question: qsTr("Oyunum listede yok, ne yapmalıyım?")
                    answer: qsTr("Önce Kütüphanem'deki \"Yeniden tara\"ya bas. Yine çıkmazsa oyunun klasörünü pencerenin herhangi bir yerine sürükleyip bırak ya da \"Oyun ekle\" ile seç: launcher hangi oyun olduğunu bulur ve sayfasını açar. Oyun kataloğumuzda yoksa Tüm Oyunlar'da aratıp yaması olup olmadığına bakabilirsin.")
                }
                Faq {
                    question: qsTr("Yama kurulu ama oyun hâlâ İngilizce.")
                    answer: qsTr("Bazı oyunlarda dili oyunun kendi ayarlarından seçmek gerekir; launcher kurulum bittiğinde bunu söyler. Söylemediyse oyunun sayfasında yamanın klasörünü kontrol et: \"Yama oyunun klasörüne gitmemiş\" uyarısı görüyorsan \"Yamayı taşı\"ya bas. Oyun yakın zamanda güncellendiyse yamanın onarılması gerekebilir.")
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
                    answer: qsTr("Oyun güncellemesi yamanın dosyalarının üzerine yazmış olabilir. Oyunun sayfasını aç: launcher eksik dosyaları fark ederse \"Onarım gerekli\" der, düğmeye basınca yama yeniden kurulur. Güncelleme yamayı tamamen bozduysa yeni yama sürümü çıkınca \"Güncelleme var\" görürsün.")
                }
                Faq {
                    question: qsTr("Steam'de \"dosyaları doğrula\" yaparsam ne olur?")
                    answer: qsTr("Steam oyunu orijinal hâline getirir ve Türkçe dosyaları siler. Bir dahaki sefere oyunun sayfasında launcher bunu görür ve yamayı yeniden kurmayı önerir. Yama kurulurken bir sorun çıkarsa da sayfadaki \"Oyun dosyalarını doğrula / onar\" oyunu temiz hâline döndürmenin en kolay yoludur.")
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
                columns: help.width > 900 ? 2 : 1
                columnSpacing: Dimensions.spacingXXL
                rowSpacing: Dimensions.spacingMD

                KeyRow { keys: "Ctrl+1"; text: qsTr("Ana Sayfa") }
                KeyRow { keys: "Ctrl+2"; text: qsTr("Kütüphanem") }
                KeyRow { keys: "Ctrl+3"; text: qsTr("Tüm Oyunlar") }
                KeyRow { keys: "Ctrl+F"; text: qsTr("Oyun ara") }
                KeyRow { keys: "Ctrl+R"; text: qsTr("Yama güncellemelerini denetle") }
                KeyRow { keys: "Ctrl+,"; text: qsTr("Ayarlar") }
                KeyRow { keys: "F1"; text: qsTr("Bu sayfa") }
                KeyRow { keys: "Esc"; text: qsTr("Geri dön") }
            }

            // ── Still stuck ──
            SectionTitle { text: qsTr("Hâlâ takıldın mı?") }
            Body {
                text: qsTr("Topluluğumuz ve çevirmenlerimiz Discord'da. Sorununu yazarken oyunun adını ve gördüğün hata mesajını eklersen daha hızlı yardım alırsın.")
            }
            RowLayout {
                Layout.bottomMargin: Dimensions.spacingMD
                spacing: Dimensions.spacingMD
                LinkButton {
                    filled: true
                    glyph: "\uE8BD"
                    text: qsTr("Discord'a katıl")
                    onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                }
                LinkButton {
                    glyph: "\uE774"
                    text: qsTr("makineceviri.org")
                    onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
                }
            }
        }
    }
}
