import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ThemeStep.qml — Accent color picker (Step 2 of onboarding)
 *
 * 10 accent presets in a grid, mini preview card showing live color.
 * Selection immediately persists via SettingsManager.accentPreset.
 */
Item {
    id: root

    signal nextStep()

    // Preset model — id + display color (accentBase from Theme._resolveAccentColors)
    readonly property var presets: [
        { presetId: "purple", color: "#8B5CF6" },
        { presetId: "blue",   color: "#3B82F6" },
        { presetId: "teal",   color: "#14B8A6" },
        { presetId: "green",  color: "#22C55E" },
        { presetId: "rose",   color: "#EC4899" },
        { presetId: "amber",  color: "#F59E0B" },
        { presetId: "red",    color: "#EF4444" },
        { presetId: "sky",    color: "#0EA5E9" },
        { presetId: "indigo", color: "#818CF8" },
        { presetId: "black",  color: "#71717A" }
    ]

    property string selectedPreset: typeof SettingsManager !== "undefined"
                                    ? SettingsManager.accentPreset : "purple"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(parent.width, 460)

        // Title
        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Tarz\u0131n\u0131 Se\u00E7")
            font.pixelSize: 24
            font.weight: Font.Bold
            font.letterSpacing: -0.3
            color: "#FFFFFF"
        }

        Item { Layout.preferredHeight: 8 }

        // Subtitle
        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Sonradan ayarlardan de\u011Fi\u015Ftirebilirsin")
            font.pixelSize: 13
            color: Qt.rgba(1, 1, 1, 0.4)
        }

        Item { Layout.preferredHeight: 32 }

        // Color grid — Flow wraps 5 per row
        Flow {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 5 * 46  // 5 circles × (36px + 10px spacing)
            spacing: 10

            Repeater {
                model: root.presets

                Rectangle {
                    required property var modelData
                    required property int index

                    width: 36; height: 36; radius: 18
                    color: modelData.color
                    border.width: root.selectedPreset === modelData.presetId ? 2 : 0
                    border.color: "#FFFFFF"

                    // Glow ring for selected
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: root.selectedPreset === parent.modelData.presetId ? 1 : 0
                        border.color: Qt.rgba(
                            parent.color.r, parent.color.g, parent.color.b, 0.4)
                        visible: root.selectedPreset === parent.modelData.presetId
                    }

                    // Hover scale
                    scale: colorMa.containsMouse ? 1.1 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }

                    MouseArea {
                        id: colorMa
                        anchors.fill: parent
                        anchors.margins: -4  // larger hit area
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedPreset = parent.modelData.presetId
                            if (typeof SettingsManager !== "undefined")
                                SettingsManager.accentPreset = parent.modelData.presetId
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        // Mini preview card
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 240
            Layout.preferredHeight: 64
            radius: 10
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.06)

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 8

                // Progress bar preview
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Rectangle {
                        width: parent.width * 0.6
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accentBase
                    }
                }

                // Button preview
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: 6
                    color: Qt.rgba(Theme.accentBase.r, Theme.accentBase.g, Theme.accentBase.b, 0.15)
                    border.width: 1
                    border.color: Qt.rgba(Theme.accentBase.r, Theme.accentBase.g, Theme.accentBase.b, 0.3)

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("T\u00FCrk\u00E7e Yap")
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: Theme.accentBase
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 32 }

        // Continue button
        Button {
            id: continueBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 44

            contentItem: Text {
                textFormat: Text.PlainText
                text: qsTr("Devam Et")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 8
                color: continueBtn.hovered
                    ? Qt.darker(Theme.accentBase, 1.15)
                    : Theme.accentBase
            }

            scale: pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 80 } }

            onClicked: root.nextStep()
        }
    }
}
