import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * ClickableRow.qml - Tıklanabilir satır bileşeni
 */
Rectangle {
    id: root

    property alias contentItem: contentLoader.sourceComponent
    property bool showArrow: true

    signal clicked()

    implicitHeight: 52
    radius: Dimensions.radiusStandard
    color: mouseArea.containsMouse ? Theme.withAlpha(Theme.textMuted, 0.05) : "transparent"

    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Loader {
            id: contentLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Image {
            visible: root.showArrow
            source: "qrc:/MakineAI/resources/icons/arrow_right.svg"
            sourceSize: Qt.size(16, 16)
            opacity: mouseArea.containsMouse ? 0.8 : 0.4

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
