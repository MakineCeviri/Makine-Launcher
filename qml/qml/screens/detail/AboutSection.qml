import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: aboutRoot
    spacing: Dimensions.spacingLG

    // Single ViewModel reference — all state accessed via vm
    required property var vm

    Layout.fillWidth: true
    visible: vm.description !== "" || vm.developers.length > 0

    Text {
        textFormat: Text.PlainText
        text: qsTr("Hakkında")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    SettingsDivider { variant: "section" }

    // Description
    Text {
        id: descText
        textFormat: Text.PlainText
        Layout.fillWidth: true
        visible: aboutRoot.vm.description !== ""
        text: aboutRoot.vm.description
        font.pixelSize: Dimensions.fontBody
        color: Theme.textSecondary
        wrapMode: Text.WordWrap; lineHeight: 1.6
        maximumLineCount: aboutRoot.vm.descriptionExpanded ? 9999 : 4
        elide: Text.ElideRight
    }

    Text {
        textFormat: Text.PlainText
        visible: aboutRoot.vm.description !== "" && !aboutRoot.vm.descriptionExpanded && descText.truncated
        text: qsTr("Daha fazla göster...")
        font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium
        color: Theme.primary
        opacity: expandMouse.containsMouse ? 1.0 : 0.7
        MouseArea {
            id: expandMouse; anchors.fill: parent; anchors.margins: -4
            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: aboutRoot.vm.descriptionExpanded = true
        }
    }

    // Separator between description and details
    SettingsDivider {
        variant: "section"
        visible: aboutRoot.vm.description !== "" && (aboutRoot.vm.developers.length > 0 || aboutRoot.vm.publishers.length > 0)
    }

    // Detail rows
    DetailRow { label: qsTr("Geliştirici"); value: aboutRoot.vm.developers.join(", "); visible: aboutRoot.vm.developers.length > 0 }
    DetailRow { label: qsTr("Yayıncı"); value: aboutRoot.vm.publishers.join(", "); visible: aboutRoot.vm.publishers.length > 0 }
    DetailRow { label: qsTr("Çıkış Tarihi"); value: aboutRoot.vm.releaseDate; visible: aboutRoot.vm.releaseDate !== "" }
    DetailRow { label: qsTr("Motor"); value: aboutRoot.vm.engine; visible: aboutRoot.vm.engine !== "" }
    DetailRow { label: qsTr("Türler"); value: aboutRoot.vm.genres.join(", "); visible: aboutRoot.vm.genres.length > 0 }

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
