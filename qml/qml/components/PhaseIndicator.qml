import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * PhaseIndicator.qml - Çeviri aşama göstergesi (Native Qt)
 */
Rectangle {
    id: root
    property int currentPhase: 0

    Layout.fillWidth: true
    implicitHeight: 80
    color: Qt.rgba(1, 1, 1, 0.03)
    radius: Dimensions.radiusStandard
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    property var phases: [
        { name: qsTr("Tespit"), icon: "🔍" },
        { name: qsTr("Çıkarma"), icon: "📦" },
        { name: qsTr("Eşleştirme"), icon: "🔗" },
        { name: qsTr("İnceleme"), icon: "👁" },
        { name: qsTr("Uygulama"), icon: "⚙" },
        { name: qsTr("Bitti"), icon: "✓" }
    ]

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        Repeater {
            model: root.phases

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                property bool isActive: index + 1 === root.currentPhase
                property bool isCompleted: index + 1 < root.currentPhase
                property bool isFinal: index === 5 && root.currentPhase === 6

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    // Dot with icon
                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: isCompleted || isFinal ? Theme.success :
                               isActive ? Theme.gold :
                               Qt.rgba(1, 1, 1, 0.15)

                        Behavior on color { ColorAnimation { duration: 300 } }

                        Text {
                            anchors.centerIn: parent
                            text: isCompleted ? "✓" : modelData.icon
                            font.pixelSize: 14
                            color: isCompleted || isFinal ? "white" :
                                   isActive ? Theme.lightTextPrimary : Theme.textMuted

                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                    }

                    // Phase name
                    Text {
                        text: modelData.name
                        font.pixelSize: 12
                        font.weight: isActive ? Font.DemiBold : Font.Normal
                        color: isCompleted || isFinal ? Theme.success :
                               isActive ? Theme.gold :
                               Theme.textMuted

                        Behavior on color { ColorAnimation { duration: 300 } }
                    }

                    // Connection line (not for last item)
                    Rectangle {
                        visible: index < 5
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 2
                        color: isCompleted ? Theme.success : Qt.rgba(1, 1, 1, 0.1)

                        Behavior on color { ColorAnimation { duration: 400; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }
    }
}
