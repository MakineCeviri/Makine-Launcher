import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: aboutRoot
    spacing: Dimensions.spacingLG

    // Required properties from parent
    required property string description
    required property var developers
    required property var publishers
    required property string releaseDate
    required property string engine
    required property var genres
    required property bool descriptionExpanded

    Layout.fillWidth: true
    visible: description !== "" || developers.length > 0

    // Signal to request description expansion
    signal expandDescription()

    Text {
        textFormat: Text.PlainText
        text: qsTr("Hakkında")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    SettingsDivider { variant: "section" }

    // Description
    Text {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        visible: aboutRoot.description !== ""
        text: aboutRoot.description
        font.pixelSize: Dimensions.fontBody
        color: Theme.textSecondary
        wrapMode: Text.WordWrap; lineHeight: 1.6
        maximumLineCount: aboutRoot.descriptionExpanded ? 9999 : 4
        elide: Text.ElideRight
    }

    Text {
        textFormat: Text.PlainText
        visible: aboutRoot.description !== "" && !aboutRoot.descriptionExpanded
        text: qsTr("Daha fazla göster...")
        font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium
        color: Theme.primary
        opacity: expandMouse.containsMouse ? 1.0 : 0.7
        MouseArea {
            id: expandMouse; anchors.fill: parent; anchors.margins: -4
            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: aboutRoot.expandDescription()
        }
    }

    // Separator between description and details
    SettingsDivider {
        variant: "section"
        visible: aboutRoot.description !== "" && (aboutRoot.developers.length > 0 || aboutRoot.publishers.length > 0)
    }

    // Detail rows
    DetailRow { label: qsTr("Geliştirici"); value: aboutRoot.developers.join(", "); visible: aboutRoot.developers.length > 0 }
    DetailRow { label: qsTr("Yayıncı"); value: aboutRoot.publishers.join(", "); visible: aboutRoot.publishers.length > 0 }
    DetailRow { label: qsTr("Çıkış Tarihi"); value: aboutRoot.releaseDate; visible: aboutRoot.releaseDate !== "" }
    DetailRow { label: qsTr("Motor"); value: aboutRoot.engine; visible: aboutRoot.engine !== "" }
    DetailRow { label: qsTr("Türler"); value: aboutRoot.genres.join(", "); visible: aboutRoot.genres.length > 0 }

    component DetailRow: RowLayout {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        height: 28; spacing: 0
        Text {
            textFormat: Text.PlainText
            Layout.preferredWidth: 110
            text: label
            font.pixelSize: Dimensions.fontBody
            color: Theme.textMuted
            elide: Text.ElideRight
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: value
            font.pixelSize: Dimensions.fontBody
            font.weight: Font.Medium
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
    }
}
