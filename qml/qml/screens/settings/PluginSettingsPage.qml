import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * PluginSettingsPage.qml — Generic settings page for any plugin.
 * Receives pluginId and renders manifest-defined settings dynamically.
 * Also shows plugin-specific controls (OCR buttons for Live plugin).
 */
ColumnLayout {
    id: pluginPage
    spacing: Dimensions.spacingXL

    property string pluginId: ""

    // Plugin info from PluginManager
    readonly property var info: PluginManager ? PluginManager.pluginInfo(pluginId) : ({})
    readonly property var settings: info.settings || []
    readonly property bool isLive: pluginId === "com.makineceviri.live"

    // ── Plugin Info Header ──
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Dimensions.spacingSM

        RowLayout {
            spacing: Dimensions.spacingMD

            // Status indicator
            Rectangle {
                width: 8; height: 8; radius: 4
                color: info.loaded ? Theme.success : Theme.textMuted
            }

            Text {
                textFormat: Text.PlainText
                text: info.loaded ? qsTr("Eklenti aktif ve çalışıyor") : qsTr("Eklenti devre dışı")
                font.pixelSize: Dimensions.fontSM
                color: info.loaded ? Theme.success : Theme.textMuted
            }

            Item { Layout.fillWidth: true }

            Text {
                textFormat: Text.PlainText
                text: "v" + (info.version || "?")
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
            }
        }
    }

    // ── OCR Controls (only for Live plugin) — at the TOP ──
    SettingsCard {
        Layout.fillWidth: true
        visible: pluginPage.isLive && info.loaded

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                    textFormat: Text.PlainText
                    text: qsTr("OCR Kontrolleri")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }

            SettingsDivider {}

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: _ocrBtns.implicitHeight + Dimensions.marginML * 2

                RowLayout {
                    id: _ocrBtns
                    anchors.fill: parent
                    anchors.margins: Dimensions.marginML
                    spacing: Dimensions.spacingMD

                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        radius: Dimensions.radiusMD
                        color: _rgnM.containsMouse ? Theme.primary12 : Theme.primary08
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Text {
                            anchors.centerIn: parent; textFormat: Text.PlainText
                            text: OcrController.captureRegion.width > 0
                                ? "\uE890 " + OcrController.captureRegion.width + "\u00D7" + OcrController.captureRegion.height
                                : "\uE890 B\u00F6lge Se\u00E7"
                            font.family: "Segoe MDL2 Assets"; font.pixelSize: Dimensions.fontSM
                            color: Theme.textPrimary
                        }
                        MouseArea { id: _rgnM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: OcrController.setRegionSelecting(true) }
                    }

                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        radius: Dimensions.radiusMD
                        color: OcrController.ocrActive ? Theme.error10 : (_sM.containsMouse ? Theme.success10 : Theme.primary08)
                        border.color: OcrController.ocrActive ? Theme.error : Theme.success; border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Row {
                            anchors.centerIn: parent; spacing: Dimensions.spacingSM
                            Rectangle { width: 8; height: 8; radius: 4
                                color: OcrController.ocrActive ? Theme.error : Theme.success
                                anchors.verticalCenter: parent.verticalCenter
                                SequentialAnimation on opacity {
                                    running: OcrController.processing &&
                                             SettingsManager.enableAnimations &&
                                             Qt.application.state === Qt.ApplicationActive
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 400 }
                                    NumberAnimation { to: 1.0; duration: 400 }
                                } }
                            Text { textFormat: Text.PlainText
                                text: OcrController.ocrActive ? qsTr("Durdur") : qsTr("Ba\u015Flat")
                                font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium
                                color: OcrController.ocrActive ? Theme.error : Theme.success }
                        }
                        MouseArea { id: _sM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { OcrController.toggleOcr(); if (OcrController.ocrActive) OcrController.setOverlayVisible(true) } }
                    }

                    Rectangle {
                        Layout.preferredWidth: 40; Layout.preferredHeight: 40
                        radius: Dimensions.radiusMD
                        color: OcrController.overlayVisible ? Theme.primary12 : (_oM.containsMouse ? Theme.primary08 : Theme.primary04)
                        border.color: OcrController.overlayVisible ? Theme.primary : "transparent"; border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Text { anchors.centerIn: parent; text: "\uE8A1"; font.family: "Segoe MDL2 Assets"; font.pixelSize: 16
                            color: OcrController.overlayVisible ? Theme.primary : Theme.textSecondary }
                        MouseArea { id: _oM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: OcrController.setOverlayVisible(!OcrController.overlayVisible) }
                    }
                }
            }

            Text {
                Layout.fillWidth: true; Layout.leftMargin: Dimensions.marginML; Layout.bottomMargin: Dimensions.marginML
                visible: OcrController.ocrActive; textFormat: Text.PlainText
                text: OcrController.processing ? qsTr("OCR \u00E7al\u0131\u015F\u0131yor...") : qsTr("Bekleniyor")
                font.pixelSize: Dimensions.fontMini; color: Theme.textMuted
            }
        }
    }

    // ── Settings Form ──
    SettingsCard {
        Layout.fillWidth: true
        visible: settings.length > 0 && info.loaded

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                    textFormat: Text.PlainText
                    text: qsTr("Yapılandırma")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }

            SettingsDivider {}

            Repeater {
                model: pluginPage.settings

                Item {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Dimensions.marginML
                        anchors.rightMargin: Dimensions.marginML
                        spacing: Dimensions.spacingXL

                        // Left: label + description (left-aligned)
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                textFormat: Text.PlainText
                                text: modelData.label || modelData.key
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                            }

                            Text {
                                textFormat: Text.PlainText
                                text: modelData.key
                                font.pixelSize: Dimensions.fontMini
                                color: Theme.textMuted
                                visible: (modelData.label || "") !== ""
                            }
                        }

                        // Right: control (right-aligned)
                        Loader {
                            Layout.preferredWidth: 220
                            Layout.alignment: Qt.AlignRight
                            Layout.preferredHeight: 36

                            sourceComponent: {
                                if (modelData.type === "toggle") return _toggleC
                                if (modelData.type === "select") return _selectC
                                if (modelData.type === "password") return _passC
                                return _textC
                            }

                            Component {
                                id: _toggleC
                                Switch {
                                    checked: {
                                        var val = PluginManager.getPluginSetting(pluginPage.pluginId, modelData.key)
                                        return val === "true" || val === "1"
                                    }
                                    onToggled: PluginManager.setPluginSetting(pluginPage.pluginId, modelData.key, checked ? "true" : "false")

                                    indicator: Rectangle {
                                        implicitWidth: 48; implicitHeight: 26
                                        x: parent.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 13
                                        color: parent.checked ? Theme.primary : Theme.primary12
                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                        Rectangle {
                                            x: parent.parent.checked ? parent.width - width - 3 : 3
                                            y: 3; width: 20; height: 20; radius: 10
                                            color: Theme.textOnColor
                                            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                                        }
                                    }
                                }
                            }

                            Component {
                                id: _selectC
                                ComboBox {
                                    model: modelData.options || []
                                    currentIndex: {
                                        var val = PluginManager.getPluginSetting(pluginPage.pluginId, modelData.key)
                                        if (!val) val = modelData["default"] || ""
                                        var opts = modelData.options || []
                                        for (var i = 0; i < opts.length; i++)
                                            if (opts[i] === val) return i
                                        return 0
                                    }
                                    onCurrentIndexChanged: {
                                        var opts = modelData.options || []
                                        if (currentIndex >= 0 && currentIndex < opts.length)
                                            PluginManager.setPluginSetting(pluginPage.pluginId, modelData.key, opts[currentIndex])
                                    }
                                }
                            }

                            Component {
                                id: _passC
                                TextField {
                                    echoMode: TextInput.Password
                                    placeholderText: "API Key..."
                                    text: PluginManager.getPluginSetting(pluginPage.pluginId, modelData.key) || ""
                                    font.pixelSize: Dimensions.fontSM
                                    onEditingFinished: PluginManager.setPluginSetting(pluginPage.pluginId, modelData.key, text)
                                    background: Rectangle {
                                        radius: Dimensions.radiusMD; color: Theme.primary06
                                        border.color: parent.activeFocus ? Theme.primary : Theme.primary12; border.width: 1
                                    }
                                }
                            }

                            Component {
                                id: _textC
                                TextField {
                                    text: PluginManager.getPluginSetting(pluginPage.pluginId, modelData.key) || modelData["default"] || ""
                                    font.pixelSize: Dimensions.fontSM
                                    onEditingFinished: PluginManager.setPluginSetting(pluginPage.pluginId, modelData.key, text)
                                    background: Rectangle {
                                        radius: Dimensions.radiusMD; color: Theme.primary06
                                        border.color: parent.activeFocus ? Theme.primary : Theme.primary12; border.width: 1
                                    }
                                }
                            }
                        }
                    }

                    // Divider between items
                    SettingsDivider {
                        anchors.bottom: parent.bottom
                        visible: index < pluginPage.settings.length - 1
                    }
                }
            }

            Item { Layout.preferredHeight: Dimensions.spacingSM }
        }
    }

    // ── Not Loaded State ──
    SettingsCard {
        Layout.fillWidth: true
        visible: !info.loaded

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 100

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingSM

                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Eklenti y\u00FCklenmedi")
                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                    color: Theme.textMuted; Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Eklentiler sayfas\u0131ndan etkinle\u015Ftirin")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textSecondary; Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
