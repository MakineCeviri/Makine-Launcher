// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"

/**
 * InfoTilesBar.qml - the App Store's information strip: a few facts side by
 * side, each a small label over its value, hairlines between them.
 *
 * While Steam's store data loads it says so; when there is none (a game not
 * on Steam) it says that this does not affect the patch, with a retry.
 */
Card {
    id: root

    required property var vm

    // "Unknown" is the scanner's word for "no idea", not an engine name.
    readonly property string _engine:     (vm.engine ?? "").toLowerCase() === "unknown" ? "" : (vm.engine ?? "")
    readonly property string _developer:  vm.developersText  ?? ""
    readonly property string _release:    vm.releaseDate     ?? ""
    readonly property string _genre: {
        var g = vm.genresText ?? ""
        var comma = g.indexOf(",")
        return comma > 0 ? g.substring(0, comma).trim() : g
    }
    readonly property string _metacritic: (typeof vm.metacriticScore === "number" && vm.metacriticScore > 0)
                                          ? String(vm.metacriticScore) : ""

    readonly property bool _hasAnyValue: _engine !== "" || _developer !== "" || _release !== ""
                                      || _genre !== "" || _metacritic !== ""
    readonly property bool _loading: vm.isLoadingSteamDetails && !vm.hasSteamDetails
    readonly property bool _failed: vm.steamFetchFailed && !vm.hasSteamDetails

    Layout.fillWidth: true
    implicitHeight: _hasAnyValue ? 78 : 52
    visible: _hasAnyValue || _loading || _failed
    radius: Dimensions.radiusLG

    component InfoTile: Item {
        id: tile
        property string label: ""
        property string value: ""
        visible: value !== ""
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 24
            spacing: 5

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.label
                font.pixelSize: Dimensions.caption2
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                font.capitalization: Font.AllUppercase
                color: Theme.textMuted
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.value
                font.pixelSize: Dimensions.headline
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    component TileSeparator: Rectangle {
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredWidth: 1
        Layout.preferredHeight: 34
        color: Theme.separator
    }

    RowLayout {
        anchors.fill: parent
        visible: root._hasAnyValue
        spacing: 0

        InfoTile { label: qsTr("Geliştirici");  value: root._developer }
        TileSeparator { visible: root._developer !== "" && (root._release !== "" || root._genre !== "" || root._metacritic !== "" || root._engine !== "") }
        InfoTile { label: qsTr("Çıkış tarihi"); value: root._release }
        TileSeparator { visible: root._release !== "" && (root._genre !== "" || root._metacritic !== "" || root._engine !== "") }
        InfoTile { label: qsTr("Tür");          value: root._genre }
        TileSeparator { visible: root._genre !== "" && (root._metacritic !== "" || root._engine !== "") }
        InfoTile { label: qsTr("Metacritic");   value: root._metacritic }
        TileSeparator { visible: root._metacritic !== "" && root._engine !== "" }
        InfoTile { label: qsTr("Motor");        value: root._engine }
    }

    // Steam's data: on its way, or not there
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 10
        visible: !root._hasAnyValue
        spacing: 10

        Spinner { visible: root._loading; size: 15 }
        Icon { visible: !root._loading; glyph: Icons.info; size: 16; color: Theme.textMuted }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: root._loading ? qsTr("Oyun bilgileri yükleniyor…")
                                : qsTr("Steam mağaza bilgileri yok (Epic ya da başka mağaza). Yamayı kurmayı etkilemez.")
            elide: Text.ElideRight
            font.pixelSize: Dimensions.callout
            color: Theme.textMuted
        }
        AppButton {
            visible: !root._loading
            size: "small"
            style: "plain"
            text: qsTr("Tekrar dene")
            onClicked: {
                root.vm.steamFetchFailed = false
                root.vm.isLoadingSteamDetails = true
                GameService.fetchSteamDetails(root.vm.steamAppId)
            }
        }
    }
}
