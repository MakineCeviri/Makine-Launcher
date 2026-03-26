import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PerformanceSettings.qml - Performance and resource settings panel
 */
ColumnLayout {
    id: perfRoot
    spacing: Dimensions.spacingXL

    property bool disableAnimations: !SettingsManager.enableAnimations
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Uygulama Animasyonları")
                description: qsTr("Arayüz animasyonlarını etkinleştir")
                checked: !perfRoot.disableAnimations
                disableAnimations: perfRoot.disableAnimations
                onToggled: perfRoot.disableAnimations = !perfRoot.disableAnimations
            }
        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            // Graphics backend selector
            Item {
                id: backendSetting
                Layout.fillWidth: true
                Layout.preferredHeight: 72

                readonly property string activeApi: SettingsManager.activeGraphicsApi()
                // Only show restart if user actually changed the setting this session
                property bool userChanged: false
                readonly property bool needsRestart: {
                    if (!userChanged) return false
                    var cfg = SettingsManager.graphicsBackend
                    if (cfg === "" || cfg === "auto") return false
                    if (cfg === "vulkan" && activeApi !== "Vulkan") return true
                    if (cfg === "d3d11" && activeApi !== "Direct3D 11") return true
                    if (cfg === "opengl" && activeApi !== "OpenGL") return true
                    return false
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML
                    spacing: Dimensions.spacingXL

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingXS

                        Label {
                            textFormat: Text.PlainText
                            text: qsTr("Grafik API")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: backendSetting.needsRestart
                                ? qsTr("Yeniden başlatma gerekli!")
                                : qsTr("Aktif: %1").arg(backendSetting.activeApi)
                            font.pixelSize: Dimensions.fontBody
                            color: backendSetting.needsRestart ? Theme.warning : Theme.textMuted
                            font.weight: backendSetting.needsRestart ? Font.DemiBold : Font.Normal
                        }
                    }

                    // Backend selector buttons
                    Rectangle {
                        Layout.preferredWidth: backendRow.width + 8
                        Layout.preferredHeight: 36
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        radius: Dimensions.radiusMD
                        color: Theme.primary06

                        Row {
                            id: backendRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingXS

                            property string current: SettingsManager.graphicsBackend

                            Repeater {
                                model: [
                                    { id: "opengl", label: "OpenGL" },
                                    { id: "d3d11", label: "D3D11" },
                                    { id: "vulkan", label: "Vulkan" }
                                ]

                                Rectangle {
                                    required property var modelData
                                    width: backendLbl.width + 20; height: 28
                                    radius: Dimensions.radiusMD
                                    color: backendRow.current === modelData.id
                                        ? Theme.primary20
                                        : backendBtnMouse.containsMouse
                                            ? Theme.primary08
                                            : "transparent"
                                    border.color: backendRow.current === modelData.id
                                        ? Theme.primary40
                                        : "transparent"
                                    border.width: 1

                                    Label {
                                        textFormat: Text.PlainText
                                        id: backendLbl
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.pixelSize: Dimensions.fontBody
                                        font.weight: backendRow.current === modelData.id ? Font.DemiBold : Font.Medium
                                        color: backendRow.current === modelData.id ? Theme.primary : Theme.textSecondary
                                    }

                                    MouseArea {
                                        id: backendBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            SettingsManager.graphicsBackend = modelData.id
                                            backendSetting.userChanged = true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SettingsDivider {}

            DisabledSetting {
                title: qsTr("Donanım Hızlandırma")
                description: qsTr("GPU kullanarak daha hızlı çeviri işleme")
            }

            SettingsDivider {}

            DisabledSetting {
                title: qsTr("Global Önbellek")
                description: qsTr("Çevirileri tüm oyunlar için paylaş")
            }
        }
    }
}
