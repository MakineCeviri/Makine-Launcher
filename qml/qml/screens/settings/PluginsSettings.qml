import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MakineLauncher 1.0
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
            name: "Makine Live",
            description: qsTr("Gerçek zamanlı ekran OCR ve çeviri overlay sistemi"),
            version: "0.1.0",
            size: "20 MB",
            icon: "\uE890",
            iconFont: "Segoe MDL2 Assets",
            accent: "#0ea5e9",
            features: [
                qsTr("DXGI/GDI Ekran Yakalama"),
                qsTr("RapidOCR Metin Tanıma"),
                qsTr("GPT · DeepL · Google Çeviri"),
                qsTr("Şeffaf Overlay Penceresi")
            ]
        },
        {
            id: "com.makineceviri.texthook",
            name: "Makine TextHook",
            description: qsTr("Oyun belleğinden doğrudan metin çıkarma ve çeviri"),
            version: "0.1.0",
            size: "80 KB",
            icon: "\uE943",
            iconFont: "Segoe MDL2 Assets",
            accent: "#a855f7",
            features: [
                qsTr("x64 Inline Hooking"),
                qsTr("GDI Text Hook (10 fonksiyon)"),
                qsTr("Named Pipe IPC"),
                qsTr("Tekrar Filtresi")
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
            var updateAvail = PluginManager ? PluginManager.hasUpdate(cat.id) : false
            var newVersion = PluginManager ? PluginManager.availableVersion(cat.id) : ""
            var lastErr = PluginManager ? PluginManager.lastPluginError(cat.id) : ""
            result.push({
                id: cat.id, name: cat.name, description: cat.description,
                version: cat.version, size: cat.size, icon: cat.icon,
                accent: cat.accent, features: cat.features,
                installed: installed, enabled: enabled, loaded: loaded,
                hasUpdate: updateAvail, availableVersion: newVersion,
                lastError: lastErr
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

                    // Badge removed per user request
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
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 12
                                color: modelData.accent + "18"
                                border.color: modelData.accent + "30"
                                border.width: 1

                                Text {
                                    textFormat: Text.PlainText
                                    anchors.centerIn: parent
                                    text: modelData.icon
                                    font.family: modelData.iconFont || ""
                                    font.pixelSize: 18
                                    color: modelData.accent
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

                            // Action buttons column
                            ColumnLayout {
                                spacing: Dimensions.spacingXXS

                                // Main action button
                                Rectangle {
                                    Layout.preferredWidth: _btnRow.implicitWidth + 28
                                    Layout.preferredHeight: 36
                                    radius: Dimensions.radiusMD
                                    color: {
                                        if (PluginManager && PluginManager.installing && PluginManager.installingPluginId === modelData.id) return Theme.primary08
                                        if (!modelData.installed) return Theme.primary
                                        if (modelData.enabled) return Theme.primary12
                                        return Theme.primary
                                    }
                                    scale: _btnMouse.pressed ? 0.92 : 1.0
                                    opacity: _btnMouse.containsMouse ? 0.9 : 1.0

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                                    // Install progress bar overlay
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: parent.width * (PluginManager ? PluginManager.installProgress : 0)
                                        radius: Dimensions.radiusMD
                                        color: Theme.primary + "44"
                                        visible: PluginManager && PluginManager.installing && PluginManager.installingPluginId === modelData.id
                                    }

                                    Row {
                                        id: _btnRow
                                        anchors.centerIn: parent
                                        spacing: Dimensions.spacingSM

                                        Text {
                                            textFormat: Text.PlainText
                                            text: {
                                                if (PluginManager && PluginManager.installing && PluginManager.installingPluginId === modelData.id) return "\u21BB"
                                                if (!modelData.installed) return "\u2913"
                                                return modelData.enabled ? "\u2714" : "\u25B6"
                                            }
                                            font.pixelSize: Dimensions.fontSM
                                            color: modelData.installed && modelData.enabled
                                                ? Theme.primary : Theme.textOnColor
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            textFormat: Text.PlainText
                                            text: {
                                                if (PluginManager && PluginManager.installing && PluginManager.installingPluginId === modelData.id)
                                                    return qsTr("Kuruluyor... %1%").arg(
                                                        Math.round((PluginManager.installProgress || 0) * 100))
                                                if (!modelData.installed) return qsTr("Kur") + "  " + modelData.size
                                                return modelData.enabled ? qsTr("Ayarlar") : qsTr("Etkinle\u015Ftir")
                                            }
                                            font.pixelSize: Dimensions.fontSM
                                            font.weight: Font.Medium
                                            color: modelData.installed && modelData.enabled
                                                ? Theme.primary : Theme.textOnColor
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    FocusRing { offset: -1 }

                                    MouseArea {
                                        id: _btnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        enabled: !(PluginManager && PluginManager.installing && PluginManager.installingPluginId === modelData.id)
                                        onClicked: {
                                            if (!modelData.installed) {
                                                PluginManager.installPlugin(modelData.id)
                                                return
                                            }
                                            if (modelData.enabled) {
                                                // Navigate to plugin's settings page
                                                var cats = pluginsRoot.parent ? root.categories : []
                                                // Find plugin settings page index in parent SettingsScreen
                                                var settingsScreen = pluginsRoot.parent
                                                while (settingsScreen && !settingsScreen.categories) settingsScreen = settingsScreen.parent
                                                if (settingsScreen) {
                                                    for (var i = 0; i < settingsScreen.categories.length; i++) {
                                                        if (settingsScreen.categories[i].pluginId === modelData.id) {
                                                            settingsScreen.selectedCategory = i
                                                            return
                                                        }
                                                    }
                                                }
                                            } else {
                                                PluginManager.enablePlugin(modelData.id)
                                            }
                                        }
                                    }
                                }

                                // Update button (shown when update available)
                                Rectangle {
                                    Layout.preferredWidth: _updateRow.implicitWidth + 20
                                    Layout.preferredHeight: 28
                                    radius: Dimensions.radiusFull
                                    color: "#f59e0b22"
                                    border.color: "#f59e0b44"
                                    border.width: 1
                                    visible: modelData.installed && modelData.hasUpdate
                                    scale: _updateMouse.pressed ? 0.92 : 1.0

                                    Row {
                                        id: _updateRow
                                        anchors.centerIn: parent
                                        spacing: 4

                                        Text {
                                            textFormat: Text.PlainText
                                            text: "\u2191 " + qsTr("Güncelle")
                                            font.pixelSize: Dimensions.fontMini
                                            font.weight: Font.DemiBold
                                            color: "#f59e0b"
                                        }
                                    }

                                    MouseArea {
                                        id: _updateMouse
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: PluginManager.installPlugin(modelData.id)
                                    }
                                }

                                // Uninstall button (shown when installed)
                                Text {
                                    textFormat: Text.PlainText
                                    text: qsTr("Kaldır")
                                    font.pixelSize: Dimensions.fontMini
                                    color: _removeMouse.containsMouse ? Theme.error : Theme.textMuted
                                    visible: modelData.installed && !modelData.enabled
                                    Layout.alignment: Qt.AlignHCenter
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    MouseArea {
                                        id: _removeMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: PluginManager.uninstallPlugin(modelData.id, false)
                                    }
                                }
                            }
                        }

                        // Error message (shown when plugin has error)
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.lastError.length > 0
                            text: "\u26A0 " + (modelData.lastError || "")
                            font.pixelSize: Dimensions.fontMini
                            color: Theme.error
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
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

    // Plugin settings moved to dedicated PluginSettingsPage (sidebar)

    // ── Community Plugins Section (GitHub Topic Search) ──
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

                    Item { Layout.fillWidth: true }

                    // GitHub topic badge
                    Rectangle {
                        implicitWidth: _ghLabel.implicitWidth + 20
                        implicitHeight: 26
                        radius: Dimensions.radiusFull
                        color: Theme.textPrimary06

                        Text {
                            id: _ghLabel
                            textFormat: Text.PlainText
                            anchors.centerIn: parent
                            text: "GitHub"
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                        }
                    }
                }
            }

            SettingsDivider {}

            // Loading indicator
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                visible: PluginManager && PluginManager.loadingCommunity

                BusyIndicator {
                    anchors.centerIn: parent
                    width: 24; height: 24
                    running: parent.visible
                }
            }

            // GitHub community plugin cards (top starred)
            Repeater {
                model: PluginManager ? PluginManager.communityPlugins : []

                Rectangle {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: index === 0 ? Dimensions.spacingMD : Dimensions.spacingSM
                    Layout.bottomMargin: index === (PluginManager ? PluginManager.communityPlugins.length - 1 : 0)
                                         ? Dimensions.spacingMD : 0
                    radius: Dimensions.radiusMD
                    color: _cMouse.containsMouse ? Theme.primary06 : Theme.primary04
                    border.color: Theme.primary08; border.width: 1
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginMS
                        spacing: Dimensions.spacingLG

                        // Owner avatar
                        Rectangle {
                            Layout.preferredWidth: 40; Layout.preferredHeight: 40
                            radius: Dimensions.radiusMD; color: Theme.primary12; clip: true

                            Image {
                                anchors.fill: parent
                                source: modelData.ownerAvatar || ""
                                sourceSize: Qt.size(40, 40)
                                asynchronous: true; fillMode: Image.PreserveAspectCrop
                            }
                        }

                        // Plugin info
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Dimensions.spacingXXS

                            RowLayout {
                                spacing: Dimensions.spacingSM

                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.name || ""
                                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }

                                // Open source indicator
                                Rectangle {
                                    implicitWidth: _osLbl.implicitWidth + 12
                                    implicitHeight: 18; radius: Dimensions.radiusFull
                                    color: Theme.textPrimary06

                                    Text {
                                        id: _osLbl; textFormat: Text.PlainText
                                        anchors.centerIn: parent
                                        text: modelData.language || "C++"
                                        font.pixelSize: Dimensions.fontMini; font.weight: Font.Medium
                                        color: Theme.textSecondary
                                    }
                                }
                            }

                            Text {
                                textFormat: Text.PlainText
                                text: modelData.description || ""
                                font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }
                        }

                        // Stars count
                        ColumnLayout {
                            spacing: 2

                            Text {
                                textFormat: Text.PlainText
                                text: "\u2B50 " + (modelData.stars || 0)
                                font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold
                                color: "#fbbf24"
                                Layout.alignment: Qt.AlignRight
                            }

                            Text {
                                textFormat: Text.PlainText
                                text: modelData.owner || ""
                                font.pixelSize: Dimensions.fontMini
                                color: Theme.textMuted
                                Layout.alignment: Qt.AlignRight
                            }
                        }
                    }

                    MouseArea {
                        id: _cMouse; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally(modelData.url || "")
                    }
                }
            }

            // "Show more" button + empty state
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: PluginManager && PluginManager.communityPlugins.length > 0 ? 56 : 100
                Layout.margins: Dimensions.marginML
                radius: Dimensions.radiusMD
                color: _moreMouse.containsMouse ? Theme.primary08 : Theme.primary04
                visible: !PluginManager || !PluginManager.loadingCommunity
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("GitHub'da daha fazla topluluk eklentisi görüntüle")
                activeFocusOnTab: true

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingSM
                    visible: !PluginManager || PluginManager.communityPlugins.length === 0

                    Text {
                        textFormat: Text.PlainText
                        text: "\uD83D\uDD0C"
                        font.pixelSize: Dimensions.headlineLarge
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Topluluk eklentilerini keşfet")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textMuted
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("GitHub'da \"makine-plugin\" topic'i ile paylaşın")
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                // "Show more" text when plugins exist
                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    visible: PluginManager && PluginManager.communityPlugins.length > 0
                    text: qsTr("GitHub'da daha fazla eklenti gör \u2192")
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: _moreMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }

                FocusRing { offset: -1 }

                MouseArea {
                    id: _moreMouse; anchors.fill: parent
                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (PluginManager)
                            PluginManager.openCommunityPage()
                        else
                            Qt.openUrlExternally("https://github.com/topics/makine-plugin")
                    }
                }
            }

            Item { Layout.preferredHeight: Dimensions.marginML }
        }
    }

    // ── Import .makine file ──
    SettingsCard {
        Layout.fillWidth: true

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

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
                        text: qsTr("Eklenti Dosyas\u0131ndan Kur")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr(".makine dosyasını seçerek eklenti yükleyin")
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                    }
                }

                Rectangle {
                    Layout.preferredWidth: _importLabel.implicitWidth + 28
                    Layout.preferredHeight: 36
                    radius: Dimensions.radiusMD
                    color: _importMouse.containsMouse ? Theme.primary12 : Theme.primary08
                    scale: _importMouse.pressed ? 0.92 : 1.0

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                    Text {
                        id: _importLabel
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("Dosya Seç...")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    FocusRing { offset: -1 }

                    MouseArea {
                        id: _importMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: _importDialog.open()
                    }
                }
            }
        }
    }

    // File dialog for .makine import
    Loader {
        id: _importDialogLoader
        active: false
        sourceComponent: Component {
            FileDialog {
                id: _dlg
                title: qsTr("Eklenti Dosyası Seç")
                nameFilters: ["Makine Plugin (*.makine)"]
                onAccepted: {
                    if (selectedFile)
                        PluginManager.installFromFile(selectedFile.toString().replace("file:///", ""))
                }
            }
        }
    }

    // Workaround: open dialog via property
    property var _importDialog: QtObject {
        function open() {
            _importDialogLoader.active = true
            _importDialogLoader.item.open()
        }
    }

    // ── Error Banner (global plugin errors) ──
    Connections {
        target: PluginManager
        function onPluginError(pluginId, error) {
            _errorText.text = (pluginId ? pluginId + ": " : "") + error
            _errorBanner.visible = true
            _errorHideTimer.restart()
        }
    }

    Rectangle {
        id: _errorBanner
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 44 : 0
        radius: Dimensions.radiusMD
        color: Theme.error10
        border.color: Theme.error12
        border.width: 1
        visible: false
        clip: true

        Behavior on Layout.preferredHeight { NumberAnimation { duration: 200 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML

            Text {
                id: _errorText
                textFormat: Text.PlainText
                Layout.fillWidth: true
                font.pixelSize: Dimensions.fontSM
                color: Theme.error
                elide: Text.ElideRight
            }

            Text {
                textFormat: Text.PlainText
                text: "\u2715"
                font.pixelSize: Dimensions.fontMD
                color: Theme.error
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: _errorBanner.visible = false
                }
            }
        }

        Timer {
            id: _errorHideTimer
            interval: 8000
            onTriggered: _errorBanner.visible = false
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
                        text: qsTr("Kendi Eklentinizi Geli\u015Ftirin")
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
                        text: qsTr("D\u00F6k\u00FCmantasyon \u2192")
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
                        onClicked: Qt.openUrlExternally("https://makineceviri.org/docs/plugin-api")
                    }
                }
            }
        }
    }
}
