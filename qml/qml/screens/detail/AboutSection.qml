import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

ColumnLayout {
    id: aboutRoot
    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL
    Layout.rightMargin: Dimensions.marginXL
    spacing: Dimensions.spacingLG

    // Required properties from parent
    required property string description
    required property var developers
    required property var publishers
    required property string releaseDate
    required property string engine
    required property var genres
    required property bool descriptionExpanded

    visible: description !== "" || developers.length > 0

    // Signal to request description expansion
    signal expandDescription()

    Text {
        textFormat: Text.PlainText
        text: qsTr("Hakkında")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    // Description card
    Rectangle {
        Layout.fillWidth: true
        visible: aboutRoot.description !== ""
        implicitHeight: aboutCol.height + Dimensions.marginML * 2
        radius: Dimensions.radiusStandard
        color: Theme.glassBackground
        border.color: Theme.glassBorder; border.width: 1

        ColumnLayout {
            id: aboutCol
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: Dimensions.marginML
            spacing: Dimensions.spacingMD

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: aboutRoot.description
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap; lineHeight: 1.6
                maximumLineCount: aboutRoot.descriptionExpanded ? 9999 : 4
                elide: Text.ElideRight
            }

            Text {
                textFormat: Text.PlainText
                visible: !aboutRoot.descriptionExpanded
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
        }
    }

    // Details card (developer, publisher, etc.)
    Rectangle {
        Layout.fillWidth: true
        visible: aboutRoot.developers.length > 0 || aboutRoot.publishers.length > 0
        implicitHeight: detailsCol.height + Dimensions.marginML * 2
        radius: Dimensions.radiusStandard
        color: Theme.glassBackground
        border.color: Theme.glassBorder; border.width: 1

        ColumnLayout {
            id: detailsCol
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: Dimensions.marginML
            spacing: 0

            DetailRow { label: qsTr("Geliştirici"); value: aboutRoot.developers.join(", "); visible: aboutRoot.developers.length > 0 }
            DetailRow { label: qsTr("Yayıncı"); value: aboutRoot.publishers.join(", "); visible: aboutRoot.publishers.length > 0 }
            DetailRow { label: qsTr("Çıkış Tarihi"); value: aboutRoot.releaseDate; visible: aboutRoot.releaseDate !== "" }
            DetailRow { label: qsTr("Motor"); value: aboutRoot.engine; visible: aboutRoot.engine !== "" }
            DetailRow { label: qsTr("Türler"); value: aboutRoot.genres.join(", "); visible: aboutRoot.genres.length > 0 }
        }
    }

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
