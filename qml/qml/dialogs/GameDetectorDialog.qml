import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MakineAI 1.0

/**
 * GameDetectorDialog.qml - Game detection configuration dialog
 */
Popup {
    id: root

    property var appWindow: null  // Required for FolderDialog on Windows
    property bool isDark: true
    property var selectedGame: null
    property string searchQuery: ""

    // Scanning state
    property bool isScanning: true
    property string statusMessage: qsTr("Oyun kütüphaneleri taranıyor...")
    property real progress: 0.0
    property string errorMessage: ""

    // Games data
    property var installedGames: []
    property var sources: ({})
    property var filteredGames: []

    signal gameSelected(var game)
    signal dialogClosed()

    // Radius constants
    readonly property real radiusLarge: 4
    readonly property real radiusMedium: 4
    readonly property real radiusSmall: 4
    readonly property real radiusTiny: 2

    width: 600
    height: Math.min(700, parent ? parent.height - 40 : 660)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    modal: true
    closePolicy: Popup.CloseOnEscape

    // Folder dialog - parentWindow required on Windows Qt6
    FolderDialog {
        id: folderDialog
        title: qsTr("Oyun Klasörünü Seçin")
        parentWindow: root.appWindow || ApplicationWindow.window
        onAccepted: addManualFolder(selectedFolder)
    }

    // GameService connections
    Connections {
        target: GameService

        function onIsScanningChanged() {
            root.isScanning = GameService.isScanning
        }

        function onScanStatusChanged() {
            root.statusMessage = GameService.scanStatus || qsTr("Taranıyor...")
        }

        function onScanProgressChanged() {
            root.progress = GameService.scanProgress || 0
        }

        function onGamesChanged() {
            var games = GameService.games
            var detected = []
            for (var i = 0; i < games.length; i++) {
                var g = games[i]
                detected.push({
                    id: g.id || "",
                    appId: g.steamAppId || g.id || "",
                    name: g.name || "Unknown",
                    source: g.source || "Unknown",
                    installPath: g.installPath || "",
                    engine: g.engine || "",
                    headerImageUrl: g.headerImageUrl || "",
                    isVerified: g.isVerified || false,
                    hasTranslation: g.hasTranslation || false
                })
            }
            root.installedGames = detected
            root.filteredGames = detected
            root.isScanning = false

            var srcMap = {}
            for (var j = 0; j < detected.length; j++) {
                srcMap[detected[j].source] = true
            }
            root.sources = srcMap
        }

        function onScanCompleted(count) {
            root.isScanning = false
            root.statusMessage = count + " oyun bulundu"
        }

        function onScanError(error) {
            root.isScanning = false
            root.errorMessage = error
        }
    }

    function startScan() {
        root.errorMessage = ""
        root.isScanning = true
        root.progress = 0.1
        root.statusMessage = qsTr("Oyun kütüphaneleri taranıyor...")
        GameService.scanAllLibraries()
    }

    function addManualFolder(folderUrl) {
        var folderPath = folderUrl.toString().replace("file:///", "")
        var folderName = folderPath.split("/").pop() || folderPath.split("\\").pop()

        var manualGame = {
            id: "manual_" + Date.now(),
            appId: "manual_" + Date.now(),
            name: folderName,
            source: "manual",
            installPath: folderPath,
            engine: "",
            headerImageUrl: "",
            isVerified: false,
            hasTranslation: false
        }

        var newGames = root.installedGames.slice()
        newGames.unshift(manualGame)
        root.installedGames = newGames
        root.filteredGames = newGames
        root.selectedGame = manualGame

        var srcMap = Object.assign({}, root.sources)
        srcMap["manual"] = true
        root.sources = srcMap
    }

    function selectGame(game) {
        root.selectedGame = game
    }

    function updateFilter() {
        if (!root.searchQuery || root.searchQuery.length === 0) {
            root.filteredGames = root.installedGames
            return
        }
        var query = root.searchQuery.toLowerCase()
        var result = []
        for (var i = 0; i < root.installedGames.length; i++) {
            var game = root.installedGames[i]
            if (game.name && game.name.toLowerCase().indexOf(query) >= 0) {
                result.push(game)
            }
        }
        root.filteredGames = result
    }

    onSearchQueryChanged: updateFilter()

    // Start scan when dialog opens, not when component is created
    onOpened: {
        startScan()
    }

    background: Rectangle {
        radius: root.radiusLarge
        color: root.isDark
            ? Qt.rgba(0.08, 0.08, 0.12, 0.95)
            : Qt.rgba(0.95, 0.95, 0.97, 0.95)
        border.color: root.isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
        border.width: 1
    }

    contentItem: Column {
        spacing: 0

        // Header
        Rectangle {
            width: parent.width
            height: 72
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                // Gradient icon
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: root.radiusSmall
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.splashGold }
                        GradientStop { position: 1.0; color: Theme.pink }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "🎮"
                        font.pixelSize: 18
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: qsTr("Oyun Kütüphanesi")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                    }

                    Text {
                        text: root.isScanning ? root.statusMessage : (root.installedGames.length + " oyun bulundu")
                        font.pixelSize: 12
                        color: root.isDark ? Theme.textMuted : Theme.lightTextMuted
                    }
                }

                // Close button
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: root.radiusSmall
                    color: closeBtn.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    scale: closeBtn.pressed ? 0.9 : 1.0
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Close")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { root.dialogClosed(); root.close() }
                    Keys.onSpacePressed: { root.dialogClosed(); root.close() }

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 14
                        color: root.isDark ? Theme.textMuted : Theme.lightTextMuted
                    }

                    MouseArea {
                        id: closeBtn
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.dialogClosed()
                            root.close()
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
            }
        }

        // Content
        Item {
            width: parent.width
            height: root.height - 72 - footerRect.height

            // Scanning state
            Column {
                anchors.centerIn: parent
                spacing: 24
                visible: root.isScanning

                Item {
                    width: 80
                    height: 80
                    anchors.horizontalCenter: parent.horizontalCenter

                    // Background track circle
                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, 36, 0, 2 * Math.PI)
                            ctx.strokeStyle = root.isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
                            ctx.lineWidth = 4
                            ctx.lineCap = "round"
                            ctx.stroke()
                        }
                    }

                    // Progress arc
                    Canvas {
                        id: progressArc
                        anchors.fill: parent
                        property real progressValue: root.progress

                        Behavior on progressValue { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                        onProgressValueChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (progressValue <= 0.001) return

                            var startAngle = -Math.PI / 2
                            var endAngle = startAngle + progressValue * 2 * Math.PI
                            var cx = width / 2, cy = height / 2, r = 36

                            // Glow layer
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle)
                            ctx.strokeStyle = Theme.withAlpha(Theme.primary, 0.3)
                            ctx.lineWidth = 8
                            ctx.lineCap = "round"
                            ctx.stroke()

                            // Main arc
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle)
                            ctx.strokeStyle = Theme.primary
                            ctx.lineWidth = 4
                            ctx.lineCap = "round"
                            ctx.stroke()
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: Math.round(root.progress * 100) + "%"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.statusMessage
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Bu işlem birkaç saniye sürebilir")
                    font.pixelSize: 13
                    color: root.isDark ? Theme.textMuted : Theme.lightTextMuted
                }
            }

            // Games list state
            Column {
                anchors.fill: parent
                visible: !root.isScanning

                // Search bar
                Item {
                    width: parent.width
                    height: 54

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 12
                        anchors.bottomMargin: 8
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: root.radiusSmall
                            color: root.isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.05)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                Text {
                                    text: "🔍"
                                    font.pixelSize: 14
                                }

                                TextInput {
                                    id: searchInput
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                                    font.pixelSize: 13
                                    clip: true
                                    Accessible.role: Accessible.EditableText
                                    Accessible.name: qsTr("Search games")
                                    onTextChanged: root.searchQuery = text

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Oyun ara...")
                                        font.pixelSize: 13
                                        color: root.isDark ? Theme.textMuted : Theme.lightTextMuted
                                        visible: searchInput.text.length === 0
                                    }
                                }
                            }
                        }

                        // Manual add button
                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: root.radiusSmall
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Add game manually")
                            activeFocusOnTab: true
                            Keys.onReturnPressed: folderDialog.open()
                            Keys.onSpacePressed: folderDialog.open()
                            color: manualBtn.containsMouse
                                ? (root.isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1))
                                : (root.isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.05))
                            scale: manualBtn.pressed ? 0.92 : 1.0

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: "➕"
                                font.pixelSize: 14
                            }

                            MouseArea {
                                id: manualBtn
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: folderDialog.open()
                            }

                            ToolTip {
                                visible: manualBtn.containsMouse
                                text: qsTr("Manuel oyun ekle")
                                delay: 500
                            }
                        }

                        // Refresh button
                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: root.radiusSmall
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Refresh game list")
                            activeFocusOnTab: true
                            Keys.onReturnPressed: root.startScan()
                            Keys.onSpacePressed: root.startScan()
                            color: refreshBtn.containsMouse
                                ? (root.isDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1))
                                : (root.isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.05))
                            scale: refreshBtn.pressed ? 0.92 : 1.0

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: "🔄"
                                font.pixelSize: 14
                            }

                            MouseArea {
                                id: refreshBtn
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.startScan()
                            }

                            ToolTip {
                                visible: refreshBtn.containsMouse
                                text: qsTr("Yeniden tara")
                                delay: 500
                            }
                        }
                    }
                }

                // Games ListView
                ListView {
                    id: gamesList
                    width: parent.width
                    height: parent.height - 54
                    clip: true
                    model: root.filteredGames
                    spacing: 8

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        width: gamesList.width - 32
                        height: 68
                        x: 16
                        radius: root.radiusMedium
                        color: {
                            var isSelected = root.selectedGame && root.selectedGame.id === modelData.id
                            if (isSelected) return Theme.withAlpha(Theme.primary, 0.15)
                            if (gameTileMouse.containsMouse) return root.isDark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                            return root.isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.03)
                        }
                        border.color: {
                            var isSelected = root.selectedGame && root.selectedGame.id === modelData.id
                            if (isSelected) return Theme.withAlpha(Theme.primary, 0.5)
                            return root.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
                        }
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            // Game image placeholder
                            Rectangle {
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 38
                                radius: root.radiusSmall
                                color: {
                                    var src = modelData.source || ""
                                    if (src === "steam") return Qt.rgba(0.106, 0.157, 0.22, 0.3)
                                    if (src === "epic") return Qt.rgba(0.184, 0.184, 0.184, 0.3)
                                    if (src === "gog") return Qt.rgba(0.525, 0.196, 0.541, 0.3)
                                    return Theme.withAlpha(Theme.primary, 0.3)
                                }
                                clip: true

                                Image {
                                    id: gameThumbImage
                                    anchors.fill: parent
                                    source: modelData.source === "steam" && modelData.appId
                                        ? "https://steamcdn-a.akamaihd.net/steam/apps/" + modelData.appId + "/capsule_231x87.jpg"
                                        : ""
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        var src = modelData.source || ""
                                        if (src === "steam") return "🎮"
                                        if (src === "epic") return "🏪"
                                        if (src === "gog") return "☁"
                                        return "📁"
                                    }
                                    font.pixelSize: 18
                                    visible: gameThumbImage.status !== Image.Ready
                                }
                            }

                            // Game info
                            Column {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    text: modelData.name || "Unknown"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                                    elide: Text.ElideRight
                                    width: parent.width
                                }

                                Text {
                                    text: modelData.source === "steam" ? "App ID: " + modelData.appId : (modelData.installPath || "")
                                    font.pixelSize: 11
                                    color: root.isDark ? Theme.textMuted : Theme.lightTextMuted
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                }
                            }

                            // Source badge
                            Rectangle {
                                Layout.preferredWidth: sourceBadgeText.width + 12
                                Layout.preferredHeight: 18
                                radius: root.radiusTiny
                                color: {
                                    var src = (modelData.source || "").toLowerCase()
                                    if (src === "steam") return Qt.rgba(0.106, 0.157, 0.22, 0.8)
                                    if (src === "epic") return Qt.rgba(0.184, 0.184, 0.184, 0.8)
                                    if (src === "gog") return Qt.rgba(0.525, 0.196, 0.541, 0.8)
                                    if (src === "manual") return Theme.withAlpha(Theme.primary, 0.8)
                                    return Qt.rgba(0.5, 0.5, 0.5, 0.8)
                                }

                                Text {
                                    id: sourceBadgeText
                                    anchors.centerIn: parent
                                    text: {
                                        var src = (modelData.source || "").toLowerCase()
                                        if (src === "steam") return "Steam"
                                        if (src === "epic") return "Epic"
                                        if (src === "gog") return "GOG"
                                        if (src === "manual") return "Manuel"
                                        return qsTr("Diğer")
                                    }
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    color: "white"
                                }
                            }

                            // Selected checkmark
                            Text {
                                visible: root.selectedGame && root.selectedGame.id === modelData.id
                                text: "✓"
                                font.pixelSize: 16
                                color: Theme.primary
                            }
                        }

                        MouseArea {
                            id: gameTileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectGame(modelData)
                        }
                    }

                    // Empty state
                    Item {
                        anchors.centerIn: parent
                        visible: gamesList.count === 0
                        width: parent.width

                        Column {
                            anchors.centerIn: parent
                            spacing: 16

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "🔍"
                                font.pixelSize: 40
                                opacity: 0.5
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.searchQuery.length > 0 ? qsTr("Sonuç bulunamadı") : qsTr("Yüklü oyun bulunamadı")
                                font.pixelSize: 16
                                color: root.isDark ? Theme.textSecondary : Theme.lightTextSecondary
                            }
                        }
                    }
                }
            }
        }

        // Footer (animated slide-up)
        Rectangle {
            id: footerRect
            width: parent.width
            height: root.selectedGame !== null ? 100 : 0
            color: "transparent"
            clip: true
            visible: height > 0

            Behavior on height {
                NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: root.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Column {
                    Layout.fillWidth: true
                    spacing: 4

                    Row {
                        spacing: 8

                        Text {
                            text: root.selectedGame ? root.selectedGame.name : ""
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: root.isDark ? Theme.textPrimary : Theme.lightTextPrimary
                            elide: Text.ElideRight
                            width: Math.min(implicitWidth, 300)
                        }

                        Rectangle {
                            visible: root.selectedGame !== null
                            width: footerSourceText.width + 12
                            height: 18
                            radius: root.radiusTiny
                            color: Theme.primary

                            Text {
                                id: footerSourceText
                                anchors.centerIn: parent
                                text: {
                                    if (!root.selectedGame) return ""
                                    var src = (root.selectedGame.source || "").toLowerCase()
                                    if (src === "steam") return "Steam"
                                    if (src === "epic") return "Epic"
                                    if (src === "gog") return "GOG"
                                    if (src === "manual") return "Manuel"
                                    return qsTr("Diğer")
                                }
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                color: "white"
                            }
                        }
                    }

                    Text {
                        text: root.selectedGame && root.selectedGame.isVerified
                            ? qsTr("Onaylı çeviri reçetesi mevcut")
                            : qsTr("Deneysel çeviri modu kullanılacak")
                        font.pixelSize: 12
                        color: root.selectedGame && root.selectedGame.isVerified ? Theme.success : Theme.warning
                    }
                }

                Rectangle {
                    Layout.preferredWidth: startBtnRow.width + 48
                    Layout.preferredHeight: 48
                    radius: root.radiusMedium
                    color: startBtn.containsMouse ? Theme.primaryHover : Theme.primary
                    scale: startBtn.pressed ? 0.96 : 1.0
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Start translation")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { if (root.selectedGame) { root.gameSelected(root.selectedGame); root.close() } }
                    Keys.onSpacePressed: { if (root.selectedGame) { root.gameSelected(root.selectedGame); root.close() } }

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                    Row {
                        id: startBtnRow
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: "🌐"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: qsTr("Çeviriyi Başlat")
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: startBtn
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.selectedGame) {
                                root.gameSelected(root.selectedGame)
                                root.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
