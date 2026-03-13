pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * InfoTilesBar.qml - Apple App Store-style horizontal metadata bar
 *
 * Displays key game metadata (engine, developer, release date, genre,
 * Metacritic score) in a row of equally-spaced tiles separated by thin
 * vertical dividers. Tiles and the entire bar auto-hide when empty.
 */
Item {
    id: root

    required property var vm

    // Derived values — computed once, referenced by tiles and visibility guard
    readonly property string _engine:     vm.engine          ?? ""
    readonly property string _developer:  vm.developersText  ?? ""
    readonly property string _release:    vm.releaseDate     ?? ""
    // First genre only: take text up to the first comma (or the whole string)
    readonly property string _genre: {
        var g = vm.genresText ?? ""
        var comma = g.indexOf(",")
        return comma > 0 ? g.substring(0, comma).trim() : g
    }
    readonly property string _metacritic: (typeof vm.metacriticScore === "number" && vm.metacriticScore > 0)
                                          ? String(vm.metacriticScore) : ""

    // Bar is hidden when every tile value is empty
    readonly property bool _hasAnyValue: _engine     !== ""
                                      || _developer  !== ""
                                      || _release    !== ""
                                      || _genre      !== ""
                                      || _metacritic !== ""

    implicitHeight: 56
    visible: _hasAnyValue

    // -------------------------------------------------------------------------
    // Inline tile component — unique name avoids shadowing shared components
    // -------------------------------------------------------------------------
    component InfoTile: Item {
        id: tile

        property string label: ""
        property string value: ""

        // Hide the whole tile (and its adjacent separator) when value is empty
        visible: value !== ""

        Layout.fillWidth: true
        implicitHeight: 56

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Dimensions.spacingXL * 2
            spacing: Dimensions.spacingXS

            // Uppercase caption label
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.label
                font.pixelSize: Dimensions.fontCaption
                font.letterSpacing: 0.8
                font.capitalization: Font.AllUppercase
                color: Theme.textMuted
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }

            // Value — medium weight, clipped when narrow
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.value
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // -------------------------------------------------------------------------
    // Thin vertical separator between tiles
    // -------------------------------------------------------------------------
    component TileSeparator: Rectangle {
        width: 1
        height: 28
        color: Theme.textPrimary06
        // Centered vertically by RowLayout's alignment
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
