import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
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

    visible: _displayText !== ""

    readonly property int _padding: Dimensions.paddingXL

    // Truncate text to ~2 lines with inline "daha fazla göster" link
    function _truncatedText(w) {
        var desc = _displayText
        var avgCharW = Dimensions.fontBody * 0.52
        var charsPerLine = Math.floor(w / avgCharW)
        var cutAt = Math.max(20, charsPerLine * 2 - 24)
        if (cutAt >= desc.length) return desc
        var cut = desc.substring(0, cutAt)
        var sp = cut.lastIndexOf(' ')
        if (sp > cutAt * 0.5) cut = cut.substring(0, sp)
        return cut + "<font color=\"" + Theme.primary + "\">... daha fazla göster</font>"
    }

    radius: Dimensions.radiusSection
    color: Theme.textPrimary03
    border.color: Theme.textPrimary06
    border.width: 1

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

        // Hidden measurement — detects if full text overflows 2 lines
        Text {
            id: _measure
            textFormat: Text.PlainText
            visible: false
            width: contentLayout.width
            text: aboutRoot._displayText
            font.pixelSize: Dimensions.fontBody
            wrapMode: Text.WordWrap; lineHeight: 1.6
            maximumLineCount: 2
        }

        // Description with inline "daha fazla göster" at end of line 2
        Text {
            id: descText
            textFormat: _measure.truncated && !aboutRoot.vm.descriptionExpanded
                ? Text.StyledText : Text.PlainText
            Layout.fillWidth: true
            visible: aboutRoot._displayText !== ""
            text: aboutRoot.vm.descriptionExpanded || !_measure.truncated
                ? aboutRoot._displayText
                : aboutRoot._truncatedText(descText.width)
            font.pixelSize: Dimensions.fontBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap; lineHeight: 1.6
            maximumLineCount: aboutRoot.vm.descriptionExpanded ? 9999 : 2

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: _measure.truncated && !aboutRoot.vm.descriptionExpanded
                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                enabled: _measure.truncated && !aboutRoot.vm.descriptionExpanded
                onClicked: aboutRoot.vm.descriptionExpanded = true
            }
        }
    }
}
