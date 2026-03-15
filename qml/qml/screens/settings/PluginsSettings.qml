import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * PluginsSettings.qml - Plugin management panel
 *
 * Shows official and community plugins with install/enable/disable controls.
 * Backend: PluginManager C++ service.
 */
ColumnLayout {
    id: pluginsRoot
    spacing: Dimensions.spacingXL

    // Discovered plugins from PluginManager
    property var discoveredPlugins: PluginManager ? PluginManager.plugins : []

    // Official plugin catalog — always shown, enriched with install/enable state
    property var officialCatalog: [
        {
            id: "com.makineceviri.live",
            name: "MakineAI Live",
            description: qsTr("Gerçek zamanlı ekran OCR ve çeviri overlay sistemi"),
            version: "1.0.0",
            size: "2.4 MB",
            icon: "\uD83D\uDD0D",
            accent: "#0ea5e9",
            features: [
                qsTr("Ekran yakalama (DXGI/GDI)"),
                qsTr("OCR metin tanıma (RapidOCR)"),
                qsTr("Çeviri motorları (DeepL, ChatGPT)"),
                qsTr("Şeffaf overlay penceresi")
            ]
        },
        {
            id: "com.makineceviri.texthook",
            name: "MakineAI TextHook",
            description: qsTr("Oyun belleğinden doğrudan metin çıkarma ve gömülü çeviri"),
            version: "1.0.0",
            size: "1.8 MB",
            icon: "\uD83E\uDE9D",
            accent: "#f59e0b",
            features: [
                qsTr("MinHook inline hooking"),
                qsTr("Engine handlers (Unity, Unreal, RPGMaker)"),
                qsTr("Gömülü çeviri (embedded translation)"),
                qsTr("Live plugin entegrasyonu")
            ]
        }
    ]

    function _isDiscovered(pluginId) {
        if (!PluginManager) return false
        for (var i = 0; i < discoveredPlugins.length; i++)
            if (discoveredPlugins[i].id === pluginId) return true
        return false
    }

    // Merge catalog with live state from PluginManager
    property var officialPlugins: {
        var result = []
        for (var i = 0; i < officialCatalog.length; i++) {
            var cat = officialCatalog[i]
            var installed = _isDiscovered(cat.id)
            var enabled = PluginManager ? PluginManager.isPluginEnabled(cat.id) : false
            var loaded = PluginManager ? PluginManager.isPluginLoaded(cat.id) : false
            result.push({
                id: cat.id, name: cat.name, description: cat.description,
                version: cat.version, size: cat.size, icon: cat.icon,
                accent: cat.accent, features: cat.features,
                installed: installed, enabled: enabled, loaded: loaded
            })
        }
        return result
    }

    // ── Official Plugins Section ──
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            // Section header
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Resmi Eklentiler")
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    // "Official" badge
                    Rectangle {
                        implicitWidth: _officialLabel.implicitWidth + 20
                        implicitHeight: 26
                        radius: Dimensions.radiusFull
                        color: Theme.primary10

                        Text {
                            id: _officialLabel
                            textFormat: Text.PlainText
                            anchors.centerIn: parent
                            text: qsTr("MakineAI")
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.DemiBold
                            color: Theme.primary
                        }
                    }
                }
            }

            SettingsDivider {}

            // Plugin cards
            Repeater {
                model: pluginsRoot.officialPlugins

                Rectangle {
                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: _pluginContent.implicitHeight + Dimensions.marginML * 2
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: index === 0 ? Dimensions.spacingMD : Dimensions.spacingSM
                    Layout.bottomMargin: index === pluginsRoot.officialPlugins.length - 1 ? Dimensions.spacingMD : 0
                    radius: Dimensions.radiusMD
                    color: _pluginMouse.containsMouse ? Theme.primary06 : Theme.primary04
                    border.color: Theme.primary08
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    ColumnLayout {
                        id: _pluginContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingMD

                        // Top row: icon + name + version + toggle
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

                            // Plugin icon
                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: Dimensions.radiusMD
                                color: modelData.accent + "22"
                                border.color: modelData.accent + "44"
                                border.width: 1

                                Text {
                                    textFormat: Text.PlainText
                                    anchors.centerIn: parent
                                    text: modelData.icon
                                    font.pixelSize: Dimensions.fontXL
                                }
                            }

                            // Plugin info
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXXS

                                RowLayout {
                                    spacing: Dimensions.spacingSM

                                    Text {
                                        textFormat: Text.PlainText
                                        text: modelData.name
                                        font.pixelSize: Dimensions.fontMD
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    Rectangle {
                                        implicitWidth: _verLabel.implicitWidth + 12
                                        implicitHeight: 20
                                        radius: Dimensions.radiusFull
                                        color: Theme.textPrimary06

                                        Text {
                                            id: _verLabel
                                            textFormat: Text.PlainText
                                            anchors.centerIn: parent
                                            text: "v" + modelData.version
                                            font.pixelSize: Dimensions.fontMini
                                            font.weight: Font.Medium
                                            color: Theme.textMuted
                                        }
                                    }
                                }

                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.description
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textMuted
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            // Install/Enable button
                            Rectangle {
                                Layout.preferredWidth: _btnRow.implicitWidth + 28
                                Layout.preferredHeight: 36
                                radius: Dimensions.radiusMD
                                color: modelData.installed
                                    ? (modelData.enabled ? Theme.primary12 : Theme.primary)
                                    : Theme.primary
                                scale: _btnMouse.pressed ? 0.92 : 1.0
                                opacity: _btnMouse.containsMouse ? 0.9 : 1.0

                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                                Accessible.role: Accessible.Button
                                Accessible.name: modelData.installed ? qsTr("Disable plugin") : qsTr("Install plugin")
                                activeFocusOnTab: true

                                Row {
                                    id: _btnRow
                                    anchors.centerIn: parent
                                    spacing: Dimensions.spacingSM

                                    Text {
                                        textFormat: Text.PlainText
                                        text: modelData.installed ? (modelData.enabled ? "\u2714" : "\u25B6") : "\u2913"
                                        font.pixelSize: Dimensions.fontSM
                                        color: modelData.installed && modelData.enabled ? Theme.primary : Theme.textOnColor
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        textFormat: Text.PlainText
                                        text: modelData.installed
                                            ? (modelData.enabled ? qsTr("Etkin") : qsTr("Etkinleştir"))
                                            : qsTr("Kur") + "  " + modelData.size
                                        font.pixelSize: Dimensions.fontSM
                                        font.weight: Font.Medium
                                        color: modelData.installed && modelData.enabled ? Theme.primary : Theme.textOnColor
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                FocusRing { offset: -1 }

                                MouseArea {
                                    id: _btnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (!modelData.installed) {
                                            // TODO: Download from CDN
                                            return
                                        }
                                        if (modelData.enabled)
                                            PluginManager.disablePlugin(modelData.id)
                                        else
                                            PluginManager.enablePlugin(modelData.id)
                                    }
                                }
                            }
                        }

                        // Features list
                        Flow {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingSM

                            Repeater {
                                model: modelData.features

                                Rectangle {
                                    required property string modelData
                                    implicitWidth: _featureText.implicitWidth + 16
                                    implicitHeight: 26
                                    radius: Dimensions.radiusFull
                                    color: Theme.textPrimary04

                                    Text {
                                        id: _featureText
                                        textFormat: Text.PlainText
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.pixelSize: Dimensions.fontMini
                                        color: Theme.textSecondary
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: _pluginMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }
            }
        }
    }

    // ── Community Plugins Section ──
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Topluluk Eklentileri")
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                }
            }

            SettingsDivider {}

            // Empty state — coming soon
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 100
                Layout.margins: Dimensions.marginML
                radius: Dimensions.radiusMD
                color: Theme.primary04

                opacity: 0
                scale: 0.95
                Component.onCompleted: _communityAnim.start()
                ParallelAnimation {
                    id: _communityAnim
                    NumberAnimation { target: parent; property: "opacity"; from: 0; to: 1; duration: 400; easing.type: Easing.OutCubic }
                    NumberAnimation { target: parent; property: "scale"; from: 0.95; to: 1; duration: 400; easing.type: Easing.OutCubic }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingSM

                    Text {
                        textFormat: Text.PlainText
                        text: "\uD83D\uDD0C"
                        font.pixelSize: Dimensions.headlineLarge
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Topluluk eklentileri yakında!")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textMuted
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("makineceviri.net/plugins adresinden takip edin")
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Item { Layout.preferredHeight: Dimensions.marginML }
        }
    }

    // ── Plugin SDK Info ──
    SettingsCard {
        Layout.fillWidth: true

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 64

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginML
                anchors.rightMargin: Dimensions.marginML
                spacing: Dimensions.spacingLG

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXXS

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Kendi eklentinizi geliştirin")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Açık kaynak Plugin SDK ile başlayın")
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                    }
                }

                Rectangle {
                    Layout.preferredWidth: _sdkLabel.implicitWidth + 28
                    Layout.preferredHeight: 36
                    radius: Dimensions.radiusMD
                    color: _sdkMouse.containsMouse ? Theme.primary12 : Theme.primary08
                    scale: _sdkMouse.pressed ? 0.92 : 1.0

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Open SDK documentation")
                    activeFocusOnTab: true

                    Text {
                        id: _sdkLabel
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("Dökümantasyon \u2192")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    FocusRing { offset: -1 }

                    MouseArea {
                        id: _sdkMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://makineceviri.net/docs/plugin-api")
                    }
                }
            }
        }
    }
}
