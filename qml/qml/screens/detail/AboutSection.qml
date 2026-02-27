import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
pragma ComponentBehavior: Bound

Rectangle {
    id: aboutRoot

    // Single ViewModel reference — all state accessed via vm
    required property var vm

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding
    // Show patch notes from manifest when available, otherwise Steam description
    readonly property bool _showPatchNotes: vm.fromLibrary && vm.translationNotes !== ""
    readonly property string _displayText: _showPatchNotes ? vm.translationNotes : vm.description

    visible: _displayText !== "" || vm.developersText !== ""

    readonly property int _padding: Dimensions.paddingXL

    radius: Dimensions.radiusSection
    color: Qt.rgba(0.05, 0.05, 0.07, 0.75)
    border.color: Qt.rgba(1, 1, 1, 0.07)
    border.width: 1

    // Rounded mask for blur clipping
    Item {
        id: blurMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: aboutRoot.radius; color: "white" }
    }

    // Blurred game artwork — frosted glass backdrop
    Image {
        id: blurSource
        anchors.centerIn: parent
        width: parent.width + 60
        height: parent.height + 60
        source: aboutRoot.vm.heroUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: blurSource
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: -0.3
        maskEnabled: true
        maskSource: blurMask
        opacity: blurSource.status === Image.Ready ? 0.18 : 0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow } }
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: aboutRoot._padding
        spacing: Dimensions.spacingLG

        Text {
            textFormat: Text.PlainText
            text: aboutRoot._showPatchNotes ? qsTr("Yama Notları") : qsTr("Hakkında")
            font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        SettingsDivider { variant: "section" }

        // Hidden measurement: detects if description overflows 2 lines
        Text {
            id: _descMeasure
            visible: false
            width: contentLayout.width
            text: aboutRoot._displayText
            font.pixelSize: Dimensions.fontBody
            wrapMode: Text.WordWrap; lineHeight: 1.6
            maximumLineCount: 2
        }

        // Description with inline "daha fazla göster"
        Text {
            id: descText
            textFormat: _descMeasure.truncated && !aboutRoot.vm.descriptionExpanded
                ? Text.StyledText : Text.PlainText
            Layout.fillWidth: true
            visible: aboutRoot._displayText !== ""
            text: {
                var desc = aboutRoot._displayText
                if (aboutRoot.vm.descriptionExpanded || !_descMeasure.truncated)
                    return desc

                // Estimate chars that fit in 2 lines, leave room for link
                var avgCharW = Dimensions.fontBody * 0.52
                var charsPerLine = Math.floor(descText.width / avgCharW)
                var cutAt = Math.max(20, charsPerLine * 2 - 24)
                if (cutAt >= desc.length) return desc

                var cut = desc.substring(0, cutAt)
                var sp = cut.lastIndexOf(' ')
                if (sp > cutAt * 0.5) cut = cut.substring(0, sp)

                return cut + "<font color=\"" + Theme.primary + "\">... daha fazla göster</font>"
            }
            font.pixelSize: Dimensions.fontBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap; lineHeight: 1.6
            maximumLineCount: aboutRoot.vm.descriptionExpanded ? 9999 : 2

            MouseArea {
                id: expandMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: _descMeasure.truncated && !aboutRoot.vm.descriptionExpanded
                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                enabled: _descMeasure.truncated && !aboutRoot.vm.descriptionExpanded
                onClicked: aboutRoot.vm.descriptionExpanded = true
            }
        }

        // Separator between description and details (hidden when showing patch notes)
        SettingsDivider {
            variant: "section"
            visible: !aboutRoot._showPatchNotes && aboutRoot.vm.description !== "" && (aboutRoot.vm.developersText !== "" || aboutRoot.vm.publishersText !== "")
        }

        // Detail rows — hidden when showing patch notes only
        DetailRow { label: qsTr("Geliştirici"); value: aboutRoot.vm.developersText; visible: !aboutRoot._showPatchNotes && aboutRoot.vm.developersText !== "" }
        DetailRow { label: qsTr("Yayıncı"); value: aboutRoot.vm.publishersText; visible: !aboutRoot._showPatchNotes && aboutRoot.vm.publishersText !== "" }
        DetailRow { label: qsTr("Çıkış Tarihi"); value: aboutRoot.vm.releaseDate; visible: !aboutRoot._showPatchNotes && aboutRoot.vm.releaseDate !== "" }
        DetailRow { label: qsTr("Motor"); value: aboutRoot.vm.engine; visible: !aboutRoot._showPatchNotes && aboutRoot.vm.engine !== "" }
        DetailRow { label: qsTr("Türler"); value: aboutRoot.vm.genresText; visible: !aboutRoot._showPatchNotes && aboutRoot.vm.genresText !== "" }

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
}
