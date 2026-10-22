// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * GeneralSettings.qml - "Genel": start-up, games, privacy, updates, storage.
 *
 * Controls bind DIRECTLY to SettingsManager — no mirror properties. An
 * imperative write to a `property: SettingsManager.x` intermediate breaks
 * that binding, so resetToDefaults()/external changes never showed up.
 */
ColumnLayout {
    id: generalRoot
    spacing: 28

    signal clearCacheRequested()
    signal resetSettingsRequested()

    readonly property int _state: UpdateService.state
    readonly property bool _downloading: _state === UpdateService.Downloading || _state === UpdateService.Verifying

    ListSection {
        title: qsTr("Başlangıç")

        ListRow {
            icon: Icons.power
            iconTint: "#30D158"
            title: qsTr("Windows ile başlat")
            subtitle: qsTr("Bilgisayar açıldığında launcher kendiliğinden açılır.")
            AppSwitch {
                checked: SettingsManager.startWithWindows
                animate: SettingsManager.enableAnimations
                accessibleName: qsTr("Windows ile başlat")
                onToggled: SettingsManager.startWithWindows = !SettingsManager.startWithWindows
            }
        }
        ListRow {
            icon: Icons.chevronsDown
            iconTint: Theme.textMuted
            title: qsTr("Kapatınca tepside çalışsın")
            subtitle: qsTr("Pencereyi kapattığında launcher saatin yanında bir simge olarak çalışmayı sürdürür; süren indirme ve kurulumlar yarıda kalmaz.")
            AppSwitch {
                checked: SettingsManager.minimizeToTray
                animate: SettingsManager.enableAnimations
                accessibleName: qsTr("Kapatınca tepside çalışsın")
                onToggled: SettingsManager.minimizeToTray = !SettingsManager.minimizeToTray
            }
        }
    }

    ListSection {
        title: qsTr("Oyunlar")

        ListRow {
            icon: Icons.scanSearch
            iconTint: Theme.primary
            title: qsTr("Çalıştırdığın oyunu tanı")
            subtitle: qsTr("Açıkken bir oyun başlattığında launcher onu fark eder; Türkçe yaması varsa kütüphanene ekler ve haber verir.")
            AppSwitch {
                checked: SettingsManager.autoDetectGames
                animate: SettingsManager.enableAnimations
                accessibleName: qsTr("Çalıştırdığın oyunu tanı")
                onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
            }
        }
    }

    ListSection {
        title: qsTr("Gizlilik")
        footer: qsTr("Raporda yalnızca neyin bozulduğu yazar: hangi işlem, hangi hata, hangi oyunda. Bilgisayarındaki klasör yolları ve kullanıcı adın rapora girmeden silinir.")

        ListRow {
            icon: Icons.shieldCheck
            iconTint: "#5E5CE6"
            title: qsTr("Çökme ve hata raporlarını gönder")
            subtitle: qsTr("Bir şey ters giderse bize kişisel bilgi içermeyen bir rapor (Sentry) gider, sorunu bulup düzeltiriz. Kapatırsan hiçbiri gönderilmez.")
            AppSwitch {
                checked: SettingsManager.sendCrashReports
                animate: SettingsManager.enableAnimations
                accessibleName: qsTr("Çökme ve hata raporlarını gönder")
                onToggled: SettingsManager.sendCrashReports = !SettingsManager.sendCrashReports
            }
        }
    }

    ListSection {
        title: qsTr("Güncellemeler")

        ListRow {
            icon: Icons.circleArrowDown
            iconTint: Theme.primary
            title: qsTr("Makine Launcher %1").arg(Dimensions.appVersion)
            subtitle: {
                switch (generalRoot._state) {
                case UpdateService.Checking:    return qsTr("Yeni sürüm var mı diye bakılıyor…")
                case UpdateService.Available:   return qsTr("Yeni sürüm hazır: %1. İndirince launcher kendini yeniler; kurulu yamalarına dokunulmaz.").arg(UpdateService.version)
                case UpdateService.Downloading: return qsTr("İndiriliyor… %%1").arg(Math.round(UpdateService.progress * 100))
                case UpdateService.Verifying:   return qsTr("İndirilen dosya doğrulanıyor…")
                case UpdateService.Ready:       return qsTr("Güncelleme hazır. Launcher kapanıp yeni sürümle açılır.")
                case UpdateService.Installing:  return qsTr("Güncelleme kuruluyor…")
                default:
                    return UpdateService.error ? qsTr("Denetlenemedi: %1").arg(UpdateService.error)
                                               : qsTr("En yeni sürümü kullanıyorsun.")
                }
            }

            Spinner { visible: generalRoot._state === UpdateService.Checking || generalRoot._state === UpdateService.Installing }
            AppButton {
                visible: generalRoot._state === UpdateService.Idle
                size: "small"
                style: "gray"
                text: UpdateService.error ? qsTr("Tekrar dene") : qsTr("Denetle")
                onClicked: UpdateService.check()
            }
            AppButton {
                visible: generalRoot._state === UpdateService.Available
                size: "small"
                style: "filled"
                text: qsTr("İndir ve kur")
                onClicked: UpdateService.download()
            }
            AppButton {
                visible: generalRoot._downloading
                size: "small"
                style: "gray"
                text: qsTr("Vazgeç")
                onClicked: UpdateService.cancel()
            }
            AppButton {
                visible: generalRoot._state === UpdateService.Ready
                size: "small"
                style: "filled"
                text: qsTr("Şimdi kur")
                onClicked: UpdateService.install()
            }
        }

        // Download progress
        Item {
            Layout.fillWidth: true
            visible: generalRoot._downloading
            implicitHeight: 40

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16 + 28 + 12
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.fillTertiary

                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, UpdateService.progress))
                        height: parent.height
                        radius: 3
                        color: Theme.primary
                        Behavior on width { NumberAnimation { duration: Dimensions.animMedium; easing.type: Easing.OutCubic } }
                    }
                }
                Text {
                    textFormat: Text.PlainText
                    text: {
                        const total = UpdateService.totalBytes / (1024 * 1024)
                        return qsTr("%1 / %2 MB").arg((total * UpdateService.progress).toFixed(1)).arg(total.toFixed(1))
                    }
                    font.pixelSize: Dimensions.footnote
                    font.features: { "tnum": 1 }
                    color: Theme.textMuted
                }
            }
        }
    }

    ListSection {
        title: qsTr("Depolama")

        ListRow {
            id: cacheRow
            property bool cleared: false
            icon: Icons.hardDrive
            iconTint: Theme.textMuted
            title: qsTr("Önbellek")
            subtitle: qsTr("Kapak resimleri ve katalogun bir kopyası: %1. Temizlersen gerektikçe yeniden indirilir; indirilen veriler etkilenmez.")
                      .arg(ImageCache.cacheSizeFormatted)
            AppButton {
                size: "small"
                style: cacheRow.cleared ? "plain" : "gray"
                icon: cacheRow.cleared ? Icons.check : ""
                tint: Theme.success
                enabled: !cacheRow.cleared
                text: cacheRow.cleared ? qsTr("Temizlendi") : qsTr("Temizle")
                onClicked: {
                    SettingsManager.clearCache()
                    ManifestSync.syncCatalog()
                }
            }
            Timer {
                id: clearedTimer
                interval: 2500
                onTriggered: cacheRow.cleared = false
            }
            Connections {
                target: SettingsManager
                function onCacheClearCompleted(success, message) {
                    if (success) {
                        cacheRow.cleared = true
                        clearedTimer.restart()
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 6

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            style: "destructiveTinted"
            size: "small"
            icon: Icons.rotateCcw
            text: qsTr("Ayarları sıfırla")
            onClicked: generalRoot.resetSettingsRequested()
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            text: qsTr("Seçimlerin ilk günkü haline döner; rapor gönderme tercihin olduğu gibi kalır. Kurulu yamalara ve kütüphanene dokunulmaz.")
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
        }
    }
}
