pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * InfoTilesBar.qml - Apple App Store-style horizontal metadata bar
 *
 * Displays key game metadata in a subtle container with equally-spaced
 * tiles separated by thin vertical dividers. Auto-hides when empty.
 */
Rectangle {
    id: root

    required property var vm

    // Derived values — computed once, referenced by tiles and visibility guard
    readonly property string _engine:     vm.engine          ?? ""
    readonly property string _developer:  vm.developersText  ?? ""
    readonly property string _release:    vm.releaseDate     ?? ""
    // First genre only
    readonly property string _genre: {
        var g = vm.genresText ?? ""
        var comma = g.indexOf(",")
        return comma > 0 ? g.substring(0, comma).trim() : g
    }
    readonly property string _metacritic: (typeof vm.metacriticScore === "number" && vm.metacriticScore > 0)
                                          ? String(vm.metacriticScore) : ""

    readonly property bool _hasAnyValue: _engine     !== ""
                                      || _developer  !== ""
                                      || _release    !== ""
                                      || _genre      !== ""
                                      || _metacritic !== ""

    Layout.fillWidth: true
    implicitHeight: 64
    visible: _hasAnyValue
    radius: Dimensions.radiusSection
    color: Theme.textPrimary03
    border.color: Theme.textPrimary06
    border.width: 1

    // -------------------------------------------------------------------------
    // Inline tile component
    // -------------------------------------------------------------------------
    component InfoTile: Item {
        id: tile
        property string label: ""
        property string value: ""
        visible: value !== ""
        Layout.fillWidth: true
        implicitHeight: 64

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Dimensions.spacingLG * 2
            spacing: Dimensions.spacingSM

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.label
                font.pixelSize: Dimensions.fontMini
                font.letterSpacing: 1.0
                font.capitalization: Font.AllUppercase
                color: Theme.textMuted
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.value
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // -------------------------------------------------------------------------
    // Vertical separator
    // -------------------------------------------------------------------------
    component TileSeparator: Rectangle {
        Layout.alignment: Qt.AlignVCenter
        width: 1
        height: 32
        color: Theme.textPrimary08
    }

    // -------------------------------------------------------------------------
    // Tile row
    // -------------------------------------------------------------------------
    RowLayout {
        anchors.fill: parent
        spacing: 0

        InfoTile { label: qsTr("MOTOR");         value: root._engine    }
        TileSeparator { visible: root._engine !== "" && root._developer !== "" }
        InfoTile { label: qsTr("GELİŞTİRİCİ");  value: root._developer }
        TileSeparator { visible: root._developer !== "" && root._release !== "" }
        InfoTile { label: qsTr("ÇIKIŞ TARİHİ"); value: root._release   }
        TileSeparator { visible: root._release !== "" && root._genre !== "" }
        InfoTile { label: qsTr("TÜR");           value: root._genre     }
        TileSeparator { visible: root._genre !== "" && root._metacritic !== "" }
        InfoTile { label: qsTr("METACRITIC");    value: root._metacritic }
    }
}
