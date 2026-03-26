import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * UpdateBanner — Game update impact warning banner.
 *
 * Shows when translation is broken or files are lost after a game update.
 * Provides a repair action button.
 */
Rectangle {
    id: banner

    required property var vm

    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
    Layout.topMargin: 56
    visible: banner.vm.impactLevel !== "" && banner.vm.impactLevel !== "safe" && banner.vm.impactLevel !== "unknown"
    implicitHeight: bannerContent.height + Dimensions.marginML * 2
    radius: Dimensions.radiusLG
    color: banner.vm.impactLevel === "broken"
        ? Theme.error08
        : Theme.warning08
    border.color: banner.vm.impactLevel === "broken"
        ? Theme.error25
        : Theme.warning25
    border.width: 1

    ColumnLayout {
        id: bannerContent
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.margins: Dimensions.marginML
        spacing: Dimensions.spacingLG

        RowLayout {
            spacing: Dimensions.spacingLG

            Text {
                textFormat: Text.PlainText
                text: "\u26A0"
                font.pixelSize: Dimensions.fontTitle
                color: banner.vm.impactLevel === "broken"
                    ? Theme.error : Theme.warning
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXXS
                Text {
                    textFormat: Text.PlainText
                    text: banner.vm.impactLevel === "broken"
                        ? qsTr("Oyun Güncellendi \u2014 Çeviri Bozulmuş")
                        : qsTr("Bazı Çeviri Dosyaları Eksik")
                    font.pixelSize: Dimensions.fontBody
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: banner.vm.updateImpact ? banner.vm.updateImpact.summary : ""
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            spacing: Dimensions.spacingLG

            // Repair button
            Rectangle {
                implicitWidth: repairRow.width + 32; implicitHeight: 38
                radius: Dimensions.radiusStandard
                color: repairMouse.containsMouse
                    ? Theme.accent20
                    : Theme.accent10
                border.color: Theme.accent30; border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Row {
                    id: repairRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                    Text {
                        textFormat: Text.PlainText
                        text: "\u2699"
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Onar")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.DemiBold
                        color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: repairMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        GameService.recoverTranslation(banner.vm.gameId)
                        banner.vm.updateImpact = null
                    }
                }
            }
        }
    }
}
