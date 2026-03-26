import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * DisplaySettings.qml - Display settings panel
 * Contains: theme, accent color, UI scale, animations, graphics API, resolution toggle
 */
ColumnLayout {
    id: displayRoot
    spacing: Dimensions.spacingXL

    // Inline theme toggle — dark/light selector row
    component InlineThemeSetting: Item {
        property bool isDarkTheme: SettingsManager.isDarkMode
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
                RowLayout {
                    spacing: Dimensions.spacingSM
                    Label {
                        textFormat: Text.PlainText
                        text: qsTr("Tema")
                        font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                        color: Theme.textPrimary
                    }
                    Rectangle {
                        width: _ykLbl.width + 10; height: 16; radius: 8
                        color: Theme.primary12
                        Label {
                            textFormat: Text.PlainText
                            id: _ykLbl; anchors.centerIn: parent
                            text: qsTr("Yakında")
                            font.pixelSize: Dimensions.fontCaption
                            font.weight: Font.DemiBold; color: Theme.textMuted
                        }
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true; text: qsTr("Uygulama görünümünü seç")
                    font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: _themeRow.width + 8
                Layout.preferredHeight: 40
                radius: Dimensions.radiusMD
                color: Theme.primary06
                Row {
                    id: _themeRow
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingXS
                    // Light theme - disabled
                    Rectangle {
                        width: _lightRow.width + 28; height: 32
                        radius: Dimensions.radiusMD
                        color: "transparent"; opacity: 0.4
                        Row {
                            id: _lightRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM
                            Label {
                                textFormat: Text.PlainText
                                text: qsTr("Açık")
                                font.pixelSize: Dimensions.fontBody; font.weight: Font.Medium
                                color: Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                    // Dark theme - active
                    Rectangle {
                        width: _darkRow.width + 28; height: 32
                        radius: Dimensions.radiusMD
                        color: Theme.primary15
                        border.color: Theme.primary35
                        border.width: 1
                        Row {
                            id: _darkRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM
                            Label {
                                textFormat: Text.PlainText
                                text: qsTr("Koyu")
                                font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // TEMA & RENK
    // =========================================================================

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            InlineThemeSetting {}

            SettingsDivider {}

            // Accent color picker
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: _accentCol.implicitHeight + 2 * Dimensions.marginML

                ColumnLayout {
                    id: _accentCol
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML
                    anchors.topMargin: Dimensions.marginMS
                    anchors.bottomMargin: Dimensions.marginMS
                    spacing: Dimensions.spacingLG

                    Label {
                        textFormat: Text.PlainText
                        text: qsTr("Vurgu Rengi")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: Dimensions.spacingLG
                        rowSpacing: Dimensions.spacingLG

                        Repeater {
                            model: SettingsManager.accentPresets()

                            Rectangle {
                                required property var modelData
                                required property int index

                                property bool isSelected: SettingsManager.accentPreset === modelData.id

                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                radius: Dimensions.radiusMD
                                color: _presetMouse.containsMouse
                                    ? Theme.primary06
                                    : (isSelected ? Theme.primary06 : "transparent")
                                border.color: isSelected
                                    ? Theme.withAlpha(modelData.colors[2], 0.6)
                                    : (_presetMouse.containsMouse ? Theme.primary12 : Theme.primary06)
                                border.width: isSelected ? 1.5 : 1

                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    // 5-tone color strip
                                    Row {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 14
                                        spacing: 2

                                        Repeater {
                                            model: modelData.colors
                                            Rectangle {
                                                required property string modelData
                                                required property int index
                                                width: (parent.width - 8) / 5
                                                height: 14
                                                radius: index === 0 ? 4 : (index === 4 ? 4 : 2)
                                                color: modelData
                                            }
                                        }
                                    }

                                    Label {
                                        textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        font.pixelSize: Dimensions.fontCaption
                                        font.weight: isSelected ? Font.DemiBold : Font.Normal
                                        color: isSelected ? Theme.textPrimary : Theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: _presetMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: SettingsManager.accentPreset = modelData.id
                                }
                            }
                        }
                    }
                }
            }

        }
    }

    // =========================================================================
    // ANİMASYON & PERFORMANS
    // =========================================================================

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Uygulama Animasyonlar\u0131")
                description: qsTr("Aray\u00fcz animasyonlar\u0131n\u0131 etkinle\u015Ftir")
                checked: SettingsManager.enableAnimations
                onToggled: SettingsManager.enableAnimations = checked
            }

            SettingsDivider {}

            // Graphics backend selector
            Item {
                id: backendSetting
                Layout.fillWidth: true
                Layout.preferredHeight: 72

                readonly property string activeApi: SettingsManager.activeGraphicsApi()
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
                                ? qsTr("Yeniden ba\u015Flatma gerekli!")
                                : qsTr("Aktif: %1").arg(backendSetting.activeApi)
                            font.pixelSize: Dimensions.fontBody
                            color: backendSetting.needsRestart ? Theme.warning : Theme.textMuted
                            font.weight: backendSetting.needsRestart ? Font.DemiBold : Font.Normal
                        }
                    }

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
                                        : backendBtnMouse.containsMouse ? Theme.primary08 : "transparent"
                                    border.color: backendRow.current === modelData.id ? Theme.primary40 : "transparent"
                                    border.width: 1

                                    Label {
                                        id: backendLbl; textFormat: Text.PlainText
                                        anchors.centerIn: parent; text: modelData.label
                                        font.pixelSize: Dimensions.fontBody
                                        font.weight: backendRow.current === modelData.id ? Font.DemiBold : Font.Medium
                                        color: backendRow.current === modelData.id ? Theme.primary : Theme.textSecondary
                                    }

                                    MouseArea {
                                        id: backendBtnMouse; anchors.fill: parent
                                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: { SettingsManager.graphicsBackend = modelData.id; backendSetting.userChanged = true }
                                    }
                                }
                            }
                        }
                    }
                }
            }

        }
    }
}
