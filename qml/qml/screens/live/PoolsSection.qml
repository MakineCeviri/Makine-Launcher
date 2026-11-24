// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * PoolsSection — "Oyun havuzları": the approved translations Live keeps on this
 * PC. Contract (the plugin's state.pools):
 *   { refreshing, updatedAt (unix s | null),
 *     items: [{slug, name, local, remote | null, state: current|outdated|missing|
 *              downloading|error|empty, error, playing}] }
 * Methods: pools.refresh {}, pools.download {slug}, pools.downloadAll {}.
 * Hidden when the plugin does not send `pools`.
 */
ColumnLayout {
    id: pools

    // The page (MakineLiveScreen): _state, _call, _busyMethod
    property var screen: null

    readonly property var _p: screen && screen._state && screen._state.pools ? screen._state.pools : null
    readonly property var _items: {
        if (!_p || !Array.isArray(_p.items)) return []
        // The game being played first, the rest as the plugin lists them
        const playing = _p.items.filter(i => i.playing === true)
        const rest = _p.items.filter(i => i.playing !== true)
        return playing.concat(rest)
    }
    readonly property int _toUpdate: _items.filter(i => i.state === "missing" || i.state === "outdated").length
    readonly property bool _refreshing: !!_p && _p.refreshing === true
    readonly property bool _downloading: _items.some(i => i.state === "downloading")
    property bool all: false
    readonly property int _shown: 6

    function _n(v) { return Number(v || 0).toLocaleString(Qt.locale("tr_TR"), "f", 0) }
    function _sub(it) {
        if (it.state === "current") return qsTr("%1 onaylı satır").arg(_n(it.local))
        if (it.state === "outdated") {
            const fresh = it.remote !== null && it.remote !== undefined && it.remote > it.local ? it.remote - it.local : 0
            const news = fresh > 0 ? qsTr("%1 yeni satır var").arg(_n(fresh)) : qsTr("Yeni satırlar var")
            return it.local > 0 ? qsTr("%1 onaylı satır · %2").arg(_n(it.local)).arg(news) : news
        }
        if (it.state === "missing") return qsTr("İndirilmedi")
        if (it.state === "downloading") return qsTr("İndiriliyor…")
        if (it.state === "error") return it.error || qsTr("İndirilemedi")
        if (it.state === "empty") return qsTr("Henüz onaylı satır yok")
        return ""
    }
    function _updated(ts) {
        if (!ts) return qsTr("Henüz denetlenmedi")
        return qsTr("Son denetim: %1").arg(Qt.formatDateTime(new Date(ts * 1000), "dd.MM.yyyy HH:mm"))
    }

    visible: !!_p
    Layout.fillWidth: true
    spacing: 10

    ListSection {
        title: qsTr("Oyun havuzları")
        footer: qsTr("Onaylı çeviriler bilgisayarına iner; Live önce bunlara bakar, bulamadığını sunucuya sorar. Yeni onaylananlar kendiliğinden iner.")

        ListRow {
            icon: pools._toUpdate > 0 ? Icons.cloudDownload : Icons.packageCheck
            iconTint: pools._toUpdate > 0 ? Theme.primary : Theme.success
            title: pools._refreshing ? qsTr("Havuzlar denetleniyor…")
                 : pools._toUpdate > 0 ? qsTr("%1 havuz indirilmeyi ya da güncellemeyi bekliyor").arg(pools._toUpdate)
                 : qsTr("Havuzların hepsi güncel")
            subtitle: pools._updated(pools._p ? pools._p.updatedAt : null)
            AppButton {
                visible: pools._toUpdate > 0
                style: "tinted"
                text: qsTr("Hepsini güncelle")
                busy: pools.screen && pools.screen._busyMethod === "pools.downloadAll"
                enabled: !pools._refreshing
                onClicked: pools.screen._call("pools.downloadAll")
            }
            AppButton {
                style: "gray"
                text: qsTr("Yenile")
                busy: pools._refreshing || (pools.screen && pools.screen._busyMethod === "pools.refresh")
                onClicked: pools.screen._call("pools.refresh")
            }
        }

        Repeater {
            model: pools.all ? pools._items : pools._items.slice(0, pools._shown)
            ListRow {
                id: poolRow
                required property var modelData
                icon: modelData.state === "error" ? Icons.circleAlert
                    : modelData.state === "current" ? Icons.circleCheck : Icons.gamepad2
                iconTint: modelData.state === "error" ? Theme.warning
                        : modelData.state === "current" ? Theme.success : Theme.primary
                title: modelData.name || modelData.slug || ""
                subtitle: pools._sub(modelData)

                StatusBadge {
                    visible: poolRow.modelData.playing === true
                    text: qsTr("Oynuyorsun")
                    tint: Theme.primary
                }
                Spinner {
                    visible: poolRow.modelData.state === "downloading"
                    size: 16
                }
                RowLayout {
                    visible: poolRow.modelData.state === "current"
                    spacing: 6
                    Icon { glyph: Icons.check; size: 14; color: Theme.success }
                    Text {
                        text: qsTr("Güncel")
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.footnote
                        font.weight: Font.DemiBold
                        color: Theme.success
                    }
                }
                AppButton {
                    visible: poolRow.modelData.state === "outdated" || poolRow.modelData.state === "missing"
                             || poolRow.modelData.state === "error"
                    style: poolRow.modelData.state === "error" ? "gray" : "tinted"
                    size: "small"
                    text: poolRow.modelData.state === "outdated" ? qsTr("Güncelle")
                        : poolRow.modelData.state === "error" ? qsTr("Tekrar dene") : qsTr("İndir")
                    enabled: !pools._refreshing
                    onClicked: pools.screen._call("pools.download", { slug: poolRow.modelData.slug })
                }
            }
        }

        ListRow {
            visible: pools._items.length > pools._shown
            icon: Icons.chevronDown
            iconTint: Theme.textMuted
            title: pools.all ? qsTr("Daha az göster") : qsTr("Tümünü göster (%1)").arg(pools._items.length)
            clickable: true
            onClicked: pools.all = !pools.all
        }
    }
}
