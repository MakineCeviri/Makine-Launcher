import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

Item {
    id: root

    Layout.preferredWidth: 200
    Layout.preferredHeight: 50

    Rectangle {
        id: versionBadge
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        implicitWidth: versionText.implicitWidth + 16
        implicitHeight: 20
        radius: Dimensions.radiusStandard
        color: Qt.rgba(1, 1, 1, 0.04)

        Text {
            id: versionText
            anchors.centerIn: parent
            text: Dimensions.appVersionFull
            font.pixelSize: 10
            font.weight: Font.Medium
            color: Qt.rgba(1, 1, 1, 0.45)
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: versionBadge.bottom
        anchors.topMargin: 6
        text: qsTr("Makine Çeviri Topluluğu")
        font.pixelSize: 10
        font.letterSpacing: 0.3
        color: Qt.rgba(1, 1, 1, 0.3)
    }
}
