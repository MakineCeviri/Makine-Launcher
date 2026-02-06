import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0
import "../components"

ColumnLayout {
    id: root
    spacing: 16

    property bool isImporting: false
    property string devStatus: ""

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: devColumn.implicitHeight + 16

        ColumnLayout {
            id: devColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            DevButton {
                title: "Test Verisi Aktar"
                subtitle: "Translation Memory'ye 30 test çevirisi ekle"
                icon: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                isLoading: root.isImporting
                onClicked: {
                    root.isImporting = true
                    root.devStatus = "Test verisi aktarılıyor..."

                    var testData = [
                        ["New Game", "Yeni Oyun"],
                        ["Load Game", "Oyun Yükle"],
                        ["Save Game", "Oyunu Kaydet"],
                        ["Settings", "Ayarlar"],
                        ["Options", "Seçenekler"],
                        ["Exit", "Çıkış"],
                        ["Continue", "Devam Et"],
                        ["Start", "Başla"],
                        ["Quit", "Çık"],
                        ["Yes", "Evet"],
                        ["No", "Hayır"],
                        ["Cancel", "İptal"],
                        ["OK", "Tamam"],
                        ["Back", "Geri"],
                        ["Next", "İleri"],
                        ["Play", "Oyna"],
                        ["Pause", "Duraklat"],
                        ["Resume", "Devam Et"],
                        ["Restart", "Yeniden Başlat"],
                        ["Level", "Seviye"],
                        ["Score", "Puan"],
                        ["Health", "Sağlık"],
                        ["Mana", "Mana"],
                        ["Attack", "Saldırı"],
                        ["Defense", "Savunma"],
                        ["Inventory", "Envanter"],
                        ["Equipment", "Ekipman"],
                        ["Quest", "Görev"],
                        ["Map", "Harita"],
                        ["Skills", "Yetenekler"]
                    ]

                    var added = 0
                    for (var i = 0; i < testData.length; i++) {
                        var entry = testData[i]
                        if (CoreBridge.addTMEntry(entry[0], entry[1], "", "")) {
                            added++
                        }
                    }

                    root.isImporting = false
                    root.devStatus = "✓ " + added + " çeviri eklendi!"
                }
            }

            SettingsDivider {}

            DevButton {
                title: "TM'yi Temizle"
                subtitle: "Tüm Translation Memory verilerini sil"
                icon: "qrc:/qt/qml/MakineAI/resources/icons/delete.svg"
                isDestructive: true
                onClicked: {
                    root.devStatus = "⚠ Bu özellik henüz aktif değil"
                }
            }

            SettingsDivider {}

            DevButton {
                title: "TM İstatistikleri"
                subtitle: "Translation Memory durumunu göster"
                icon: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                onClicked: {
                    var terms = CoreBridge.getAllGlossaryTerms()
                    root.devStatus = "TM/Glossary İstatistikleri:\n" +
                               "Glossary Terimleri: " + terms.length + "\n" +
                               "Durum: Aktif"
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: root.devStatus !== ""
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: statusText.implicitHeight + 32

        TextEdit {
            id: statusText
            anchors.fill: parent
            anchors.margins: 16
            text: root.devStatus
            font.pixelSize: 12
            font.family: Qt.platform.os === "windows" ? "Consolas" : "Courier New"
            color: Qt.rgba(1, 1, 1, 0.7)
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
        }
    }
}
