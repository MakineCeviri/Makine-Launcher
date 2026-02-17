import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * PerformanceSettings.qml - Performance and resource settings panel
 */
ColumnLayout {
    id: perfRoot
    spacing: Dimensions.spacingXL

    property bool disableAnimations: !SettingsManager.enableAnimations
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations

    // -- Local component overrides (pixel-match SettingsScreen inline versions) --
    component SettingsCard: Rectangle {
        default property alias content: _cc.data
        implicitHeight: _cc.implicitHeight
        radius: Dimensions.radiusStandard
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        ColumnLayout { id: _cc; anchors.fill: parent; spacing: 0 }
    }

    component SettingsDivider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.withAlpha(Theme.textPrimary, 0.04)
    }

    component ToggleSetting: Item {
        id: _toggleRoot
        property string title: ""
        property string description: ""
        property bool checked: false
        signal toggled()
        activeFocusOnTab: true
        Keys.onReturnPressed: toggled()
        Keys.onSpacePressed: toggled()
        Accessible.role: Accessible.CheckBox
        Accessible.name: title
        Accessible.description: description
        Accessible.checked: checked
        Accessible.onToggleAction: toggled()
        Layout.fillWidth: true
        Layout.preferredHeight: 72
        Rectangle {
            anchors.fill: parent
            color: _toggleMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.02) : "transparent"
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
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
                    Layout.fillWidth: true; text: title
                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                    color: Theme.textPrimary; elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true; text: description
                    font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                id: _toggleTrack
                Layout.preferredWidth: Dimensions.toggleWidth
                Layout.preferredHeight: Dimensions.toggleHeight
                radius: Dimensions.toggleRadius
                color: checked ? Theme.primary : Theme.withAlpha(Theme.textPrimary, 0.1)
                property bool showGlow: _toggleMouse.containsMouse || _toggleRoot.activeFocus
                border.color: showGlow
                    ? (checked ? Theme.withAlpha(Theme.primary, 0.6) : Theme.withAlpha(Theme.textPrimary, 0.3))
                    : "transparent"
                border.width: 1.5
                scale: _toggleMouse.containsMouse ? 1.05 : 1.0
                Behavior on color {
                    ColorAnimation {
                        duration: perfRoot.disableAnimations ? 0 : 200
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
                Rectangle {
                    width: Dimensions.toggleKnobSize
                    height: Dimensions.toggleKnobSize
                    radius: Dimensions.toggleKnobRadius
                    color: Theme.textOnColor
                    x: checked ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    Rectangle {
                        anchors.fill: parent; anchors.margins: -1
                        radius: Dimensions.radiusStandard
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.bgPrimary, 0.15)
                        border.width: 1; z: -1
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: perfRoot.disableAnimations ? 0 : 200
                            easing.type: Easing.OutCubic
                        }
                    }
                    scale: _toggleMouse.pressed ? 0.85 : 1.0
                    Behavior on scale { NumberAnimation { duration: Dimensions.animVeryFast } }
                }
                MouseArea {
                    id: _toggleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: _toggleRoot.toggled()
                }
            }
        }
    }

    component DisabledSetting: Item {
        property string title: ""
        property string description: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 72
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
            spacing: Dimensions.spacingXL
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXS
                Label {
                    text: title; font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium; color: Theme.textMuted
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
                Label {
                    text: description; font.pixelSize: Dimensions.fontBody
                    color: Theme.withAlpha(Theme.textMuted, 0.7)
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: _yLbl.width + 24
                Layout.preferredHeight: 28; radius: 14
                color: Theme.withAlpha(Theme.textPrimary, 0.08)
                Label {
                    id: _yLbl; anchors.centerIn: parent
                    text: qsTr("Yakında")
                    font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }
        }
    }
    // -- End local component overrides --

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Uygulama Animasyonları")
                description: qsTr("Arayüz animasyonlarını etkinleştir")
                checked: !perfRoot.disableAnimations
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
                readonly property bool needsRestart: {
                    var cfg = SettingsManager.graphicsBackend
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
                            text: qsTr("Grafik Backend")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }

                        Label {
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
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(Theme.textPrimary, 0.06)

                        Row {
                            id: backendRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingXS

                            property string current: SettingsManager.graphicsBackend

                            Repeater {
                                model: [
                                    { id: "vulkan", label: "Vulkan" },
                                    { id: "d3d11", label: "D3D11" },
                                    { id: "opengl", label: "OpenGL" }
                                ]

                                Rectangle {
                                    required property var modelData
                                    width: backendLbl.width + 20; height: 28
                                    radius: Dimensions.radiusStandard
                                    color: backendRow.current === modelData.id
                                        ? Theme.withAlpha(Theme.primary, 0.20)
                                        : backendBtnMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.08)
                                            : "transparent"
                                    border.color: backendRow.current === modelData.id
                                        ? Theme.withAlpha(Theme.primary, 0.40)
                                        : "transparent"
                                    border.width: 1

                                    Label {
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
                                        onClicked: SettingsManager.graphicsBackend = modelData.id
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
