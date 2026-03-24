import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ReadyStep.qml - Third/final step of onboarding wizard
 * Shows success checkmark and finish button.
 */
Item {
    id: readyStep

    signal finished()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(parent.width, 460)

        // Checkmark circle
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 32
            color: Qt.rgba(0.06, 0.73, 0.51, 0.1)
            border.color: Qt.rgba(0.06, 0.73, 0.51, 0.2)
            border.width: 1

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "\u2713"
                font.pixelSize: 28
                font.weight: Font.Bold
                color: Theme.success
            }
        }

        Item { Layout.preferredHeight: 20 }

        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Haz\u0131rs\u0131n!")
            font.pixelSize: 28
            font.weight: Font.Bold
            font.letterSpacing: -0.5
            color: "#FFFFFF"
        }

        Item { Layout.preferredHeight: 8 }

        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 380
            text: qsTr("Bir oyun se\u00e7, T\u00fcrk\u00e7e \u00e7evirisini y\u00fckle, oyna.")
            font.pixelSize: 15
            color: Qt.rgba(1, 1, 1, 0.5)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }

        Item { Layout.preferredHeight: 32 }

        // Finish button
        Button {
            id: finishBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 240
            Layout.preferredHeight: 48

            contentItem: Text {
                textFormat: Text.PlainText
                text: qsTr("Ba\u015flayal\u0131m")
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: finishBtn.hovered ? "#E04898" : "#D63D8C" }
                    GradientStop { position: 1.0; color: finishBtn.hovered ? "#0891B2" : "#06B6D4" }
                }
            }

            scale: pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 80 } }

            onClicked: readyStep.finished()
        }
    }
}
