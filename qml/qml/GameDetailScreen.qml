import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * GameDetailScreen.qml
 *
 * Features:
 * - Blurred background hero image
 * - Cover image 280x130, borderRadius 16
 * - Steam details (description, metacritic, platforms)
 * - Recipe info with quality badges
 * - Backup management section
 * - Screenshots horizontal scroll (180px height)
 */
Item {
    id: root

    // Game data properties
    property string gameId: ""
    property string gameName: "Game Name"
    property string steamAppId: ""
    property string imageUrl: ""
    property string heroImageUrl: ""
    property bool verified: false
    property string engine: ""

    // Steam details
    property string description: ""
    property var developers: []
    property var publishers: []
    property string releaseDate: ""
    property var genres: []
    property int metacriticScore: 0
    property bool hasWindows: true
    property bool hasMac: false
    property bool hasLinux: false
    property string price: ""
    property int discountPercent: 0
    property bool hasSteamDetails: false

    // Recipe info
    property string recipeEngine: ""
    property string recipeQuality: ""
    property int qualityScore: 0
    property string recipeVersion: ""
    property int stringCount: 0
    property string coverage: ""
    property int fileCount: 0
    property string author: ""
    property bool hasRecipe: false

    // Screenshots
    property var screenshots: []

    // Loading state
    property bool isLoadingSteamDetails: false

    signal backClicked()
    signal translateClicked()
    signal steamStoreClicked()
    signal restoreClicked()

    function resetDetails() {
        description = ""
        developers = []
        publishers = []
        releaseDate = ""
        genres = []
        metacriticScore = 0
        hasWindows = true
        hasMac = false
        hasLinux = false
        price = ""
        discountPercent = 0
        hasSteamDetails = false
        screenshots = []
        isLoadingSteamDetails = false
        hasRecipe = false
        recipeEngine = ""
        recipeQuality = ""
        qualityScore = 0
        recipeVersion = ""
        stringCount = 0
        coverage = ""
        fileCount = 0
        author = ""
    }

    function populateSteamDetails(details) {
        description = details.description || ""
        developers = details.developers || []
        publishers = details.publishers || []
        releaseDate = details.releaseDate || ""
        genres = details.genres || []
        metacriticScore = details.metacriticScore || 0
        hasWindows = details.hasWindows !== undefined ? details.hasWindows : true
        hasMac = details.hasMac || false
        hasLinux = details.hasLinux || false
        price = details.price || ""
        discountPercent = details.discountPercent || 0
        screenshots = details.screenshots || []
        if (details.backgroundUrl && details.backgroundUrl !== "")
            heroImageUrl = details.backgroundUrl
        hasSteamDetails = true
        isLoadingSteamDetails = false
    }

    onSteamAppIdChanged: {
        if (steamAppId === "") return

        // Try sync cache first
        var cached = GameService.getSteamDetails(steamAppId)
        if (cached && cached.description !== undefined) {
            populateSteamDetails(cached)
        } else {
            isLoadingSteamDetails = true
        }

        // Always fetch async (will no-op if cache is fresh)
        GameService.fetchSteamDetails(steamAppId)
    }

    onGameIdChanged: {
        if (gameId === "") return

        var recipe = GameService.getRecipeInfo(gameId)
        if (recipe && recipe.hasRecipe) {
            hasRecipe = true
            recipeVersion = recipe.version || ""
        }
    }

    Connections {
        target: GameService
        function onSteamDetailsFetched(appId, details) {
            if (appId === root.steamAppId) {
                root.populateSteamDetails(details)
            }
        }
        function onSteamDetailsFetchError(appId, error) {
            if (appId === root.steamAppId) {
                root.isLoadingSteamDetails = false
            }
        }
    }

    // Quality color helper
    function getQualityColor(score) {
        if (score >= 90) return Theme.scoreExcellent
        if (score >= 75) return Theme.scoreGood
        if (score >= 60) return Theme.scoreFair
        return Theme.scorePoor
    }

    // Quality label helper
    function getQualityLabel(quality) {
        var lower = quality.toLowerCase()
        if (lower === "professional") return qsTr("Profesyonel")
        if (lower === "verified") return qsTr("Doğrulanmış")
        if (lower === "community") return qsTr("Topluluk")
        if (lower === "ai") return qsTr("Yapay Zeka")
        return quality
    }

    // ===== BACKGROUND WITH BLUR =====
    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        // Hero background image (blurred)
        Image {
            id: heroBackground
            anchors.fill: parent
            source: root.heroImageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: 0.3
            visible: root.heroImageUrl !== ""
        }

        // Gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bgPrimary, 0.3) }
                GradientStop { position: 0.5; color: Theme.withAlpha(Theme.bgPrimary, 0.85) }
                GradientStop { position: 1.0; color: Theme.bgPrimary }
            }
        }
    }

    // ===== FLOATING APP BAR BUTTONS =====
    // Back button
    Rectangle {
        id: backButton
        x: 16
        y: 16
        width: 40
        height: 40
        radius: Dimensions.radiusStandard
        color: backBtnMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0, 0, 0, 0.3)
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Back")
        z: 100

        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "\u2190"  // Left arrow
            font.pixelSize: 20
            color: "white"
        }

        MouseArea {
            id: backBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.backClicked()
        }

        ToolTip {
            visible: backBtnMouse.containsMouse
            text: qsTr("Back")
            delay: 400
        }
    }

    // Open in new button
    Rectangle {
        id: openInNewButton
        x: parent.width - 56
        y: 16
        width: 40
        height: 40
        radius: Dimensions.radiusStandard
        color: openNewMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0, 0, 0, 0.3)
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Open on Steam")
        z: 100

        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "\u2197"  // Arrow upper right
            font.pixelSize: 18
            color: "white"
        }

        MouseArea {
            id: openNewMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.steamAppId !== "") {
                    Qt.openUrlExternally("https://store.steampowered.com/app/" + root.steamAppId)
                }
                root.steamStoreClicked()
            }
        }

        ToolTip {
            visible: openNewMouse.containsMouse
            text: qsTr("Open on Steam")
            delay: 400
        }
    }

    // ===== MAIN SCROLL CONTENT =====
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "transparent" }
            contentItem: Rectangle {
                implicitWidth: 8
                radius: Dimensions.radiusStandard
                color: parent.pressed ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.15)
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 40

            Item { Layout.preferredHeight: 80 }

            // ===== HERO SECTION =====
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 32

                // Cover image
                Rectangle {
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 130
                    radius: Dimensions.radiusStandard
                    color: Theme.surfaceActive
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: root.imageUrl
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(300, 260)
                        asynchronous: true
                        cache: true
                        visible: root.imageUrl !== ""
                    }

                    // Placeholder
                    Text {
                        anchors.centerIn: parent
                        text: gameName.substring(0, 2).toUpperCase()
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        color: Theme.textMuted
                        visible: root.imageUrl === ""
                    }
                }

                // Info column
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    // Verified badge
                    Rectangle {
                        visible: root.verified
                        Layout.preferredWidth: verifiedRow.width + 24
                        Layout.preferredHeight: 32
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(Theme.primary, 0.15)
                        border.color: Theme.withAlpha(Theme.primary, 0.3)
                        border.width: 1

                        Row {
                            id: verifiedRow
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: "\u2713"
                                font.pixelSize: 14
                                color: Theme.primary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: qsTr("Onaylı Türkçe Çeviri")
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: Theme.primary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Item { Layout.preferredHeight: root.verified ? 16 : 0 }

                    // Game name
                    Text {
                        Layout.fillWidth: true
                        text: root.gameName
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        color: "white"
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.preferredHeight: 24 }

                    // Action buttons
                    RowLayout {
                        spacing: 12

                        // Steam button
                        Rectangle {
                            Layout.preferredWidth: steamBtnContent.width + 48
                            Layout.preferredHeight: 48
                            radius: Dimensions.radiusStandard
                            color: steamBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.1)
                            border.color: Qt.rgba(1, 1, 1, 0.2)
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Row {
                                id: steamBtnContent
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: "\uD83D\uDECD"  // Storefront
                                    font.pixelSize: 18
                                    color: "white"
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: qsTr("Steam'de Aç")
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: "white"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: steamBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.steamAppId !== "") {
                                        Qt.openUrlExternally("https://store.steampowered.com/app/" + root.steamAppId)
                                    }
                                }
                            }
                        }

                        // Translate button
                        Rectangle {
                            Layout.preferredWidth: translateBtnContent.width + 48
                            Layout.preferredHeight: 48
                            radius: Dimensions.radiusStandard
                            color: translateBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Row {
                                id: translateBtnContent
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: "\uD83C\uDF10"  // Globe
                                    font.pixelSize: 18
                                    color: "white"
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
                                id: translateBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.translateClicked()
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== STEAM DETAILS SECTION =====
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 32
                visible: root.hasSteamDetails || root.isLoadingSteamDetails

                // Loading indicator
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12
                    visible: root.isLoadingSteamDetails && !root.hasSteamDetails

                    BusyIndicator {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        running: visible
                    }

                    Text {
                        text: qsTr("Steam bilgileri yükleniyor...")
                        font.pixelSize: 14
                        color: Theme.textMuted
                    }
                }

                // About section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.description !== ""

                    // Section title
                    Text {
                        text: qsTr("Hakkında")
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: "white"
                    }

                    // Glass card
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: descriptionText.height + 40
                        radius: Dimensions.radiusStandard
                        color: Qt.rgba(1, 1, 1, 0.05)
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1

                        Text {
                            id: descriptionText
                            anchors.fill: parent
                            anchors.margins: 20
                            text: root.description
                            font.pixelSize: 14
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            lineHeight: 1.6
                        }
                    }
                }

                // Details and Rating row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24

                    // Left - Details
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: qsTr("Detaylar")
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: "white"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: detailsColumn.height + 40
                            radius: Dimensions.radiusStandard
                            color: Qt.rgba(1, 1, 1, 0.05)
                            border.color: Qt.rgba(1, 1, 1, 0.1)
                            border.width: 1

                            ColumnLayout {
                                id: detailsColumn
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 0

                                InfoRow {
                                    label: qsTr("Geliştirici")
                                    value: root.developers.join(", ")
                                    visible: root.developers.length > 0
                                }

                                InfoRow {
                                    label: qsTr("Yayıncı")
                                    value: root.publishers.join(", ")
                                    visible: root.publishers.length > 0
                                }

                                InfoRow {
                                    label: qsTr("Çıkış Tarihi")
                                    value: root.releaseDate
                                    visible: root.releaseDate !== ""
                                }

                                InfoRow {
                                    label: qsTr("Türler")
                                    value: root.genres.join(", ")
                                    visible: root.genres.length > 0
                                }

                                InfoRow {
                                    label: qsTr("Motor")
                                    value: root.engine
                                    visible: root.engine !== ""
                                }
                            }
                        }
                    }

                    // Right - Rating
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: qsTr("Değerlendirme")
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: "white"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: ratingColumn.height + 40
                            radius: Dimensions.radiusStandard
                            color: Qt.rgba(1, 1, 1, 0.05)
                            border.color: Qt.rgba(1, 1, 1, 0.1)
                            border.width: 1

                            ColumnLayout {
                                id: ratingColumn
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16

                                // Metacritic
                                RowLayout {
                                    spacing: 12

                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: Dimensions.radiusStandard
                                        color: {
                                            if (root.metacriticScore >= 75) return Theme.scoreExcellent
                                            if (root.metacriticScore >= 50) return Theme.scoreFair
                                            if (root.metacriticScore > 0) return Theme.scoreBad
                                            return Theme.textMuted
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.metacriticScore > 0 ? root.metacriticScore : "--"
                                            font.pixelSize: 20
                                            font.weight: Font.Bold
                                            color: "white"
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 2

                                        Text {
                                            text: "Metacritic"
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                            color: "white"
                                        }

                                        Text {
                                            text: {
                                                if (root.metacriticScore >= 75) return qsTr("Çok Olumlu")
                                                if (root.metacriticScore >= 50) return qsTr("Karışık")
                                                if (root.metacriticScore > 0) return qsTr("Olumsuz")
                                                return qsTr("Puan yok")
                                            }
                                            font.pixelSize: 12
                                            color: Theme.textMuted
                                        }
                                    }
                                }

                                // Platforms
                                RowLayout {
                                    spacing: 12

                                    Text {
                                        text: qsTr("Platformlar:")
                                        font.pixelSize: 13
                                        color: Theme.textMuted
                                    }

                                    Text {
                                        text: "\uD83D\uDDA5"  // Windows
                                        font.pixelSize: 18
                                        visible: root.hasWindows
                                    }

                                    Text {
                                        text: "\uD83C\uDF4E"  // Apple
                                        font.pixelSize: 18
                                        visible: root.hasMac
                                    }

                                    Text {
                                        text: "\uD83D\uDCBB"  // Linux
                                        font.pixelSize: 18
                                        visible: root.hasLinux
                                    }
                                }

                                // Price
                                RowLayout {
                                    visible: root.price !== ""
                                    spacing: 8

                                    Rectangle {
                                        visible: root.discountPercent > 0
                                        Layout.preferredWidth: discountText.width + 16
                                        Layout.preferredHeight: 24
                                        radius: Dimensions.radiusStandard
                                        color: Theme.success

                                        Text {
                                            id: discountText
                                            anchors.centerIn: parent
                                            text: "-" + root.discountPercent + "%"
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                            color: "white"
                                        }
                                    }

                                    Text {
                                        text: root.price === "" ? qsTr("Ücretsiz") : root.price
                                        font.pixelSize: 16
                                        font.weight: Font.Bold
                                        color: root.discountPercent > 0 ? Theme.success : Theme.textPrimary
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ===== RECIPE SECTION =====
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 12

                Text {
                    text: qsTr("Çeviri Bilgileri")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "white"
                }

                // Recipe card (when recipe exists)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: recipeContent.height + 40
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1
                    visible: root.hasRecipe

                    ColumnLayout {
                        id: recipeContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16

                        // Badges row
                        RowLayout {
                            spacing: 8

                            // Engine badge
                            Rectangle {
                                Layout.preferredWidth: engineBadgeRow.width + 20
                                Layout.preferredHeight: 28
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.primary, 0.15)

                                Row {
                                    id: engineBadgeRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Text {
                                        text: "\u2728"
                                        font.pixelSize: 12
                                        color: Theme.primary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        text: root.recipeEngine
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: Theme.primary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            // Quality badge
                            Rectangle {
                                Layout.preferredWidth: qualityBadgeText.width + 20
                                Layout.preferredHeight: 28
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(getQualityColor(root.qualityScore), 0.15)

                                Text {
                                    id: qualityBadgeText
                                    anchors.centerIn: parent
                                    text: getQualityLabel(root.recipeQuality)
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: getQualityColor(root.qualityScore)
                                }
                            }
                        }

                        InfoRow { label: qsTr("Reçete Sürümü"); value: "v" + root.recipeVersion }
                        InfoRow { label: qsTr("Metin Sayısı"); value: root.stringCount + " " + qsTr("adet") }
                        InfoRow { label: qsTr("Kapsam"); value: root.coverage }
                        InfoRow { label: qsTr("Dosya Sayısı"); value: root.fileCount + " " + qsTr("dosya") }
                        InfoRow { label: qsTr("Hazırlayan"); value: root.author; visible: root.author !== "" }
                    }
                }

                // No recipe card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1
                    visible: !root.hasRecipe

                    Row {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            text: "\u2139"
                            font.pixelSize: 18
                            color: Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: qsTr("Bu oyun için henüz çeviri reçetesi bulunmuyor.")
                            font.pixelSize: 13
                            color: Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ===== BACKUP SECTION =====
            ColumnLayout {
                id: backupSection
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 12

                // Backup data from BackupManager
                property var gameBackups: root.gameId !== "" ? BackupManager.getBackupsForGame(root.gameId) : []
                property bool hasBackups: gameBackups.length > 0
                property var latestBackup: root.gameId !== "" ? BackupManager.getLatestBackup(root.gameId) : ({})

                Connections {
                    target: BackupManager
                    function onBackupsChanged() {
                        backupSection.gameBackups = root.gameId !== "" ? BackupManager.getBackupsForGame(root.gameId) : []
                        backupSection.latestBackup = root.gameId !== "" ? BackupManager.getLatestBackup(root.gameId) : ({})
                        backupSection.hasBackups = backupSection.gameBackups.length > 0
                    }
                    function onBackupRestored(gId) {
                        if (gId === root.gameId)
                            root.restoreClicked()
                    }
                }

                Text {
                    text: qsTr("Yedekleme Yönetimi")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "white"
                }

                // Main backup card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: backupContent.height + 40
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1

                    ColumnLayout {
                        id: backupContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Çeviri uygulamadan önce oyun dosyaları otomatik olarak yedeklenir.")
                            font.pixelSize: 13
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                        }

                        // Restore in progress indicator
                        RowLayout {
                            visible: BackupManager.isRestoring
                            spacing: 12

                            BusyIndicator {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                running: visible
                            }

                            Text {
                                text: BackupManager.restoreStatus
                                font.pixelSize: 13
                                color: Theme.primary
                            }
                        }

                        // Has backups state
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            visible: backupSection.hasBackups && !BackupManager.isRestoring

                            // Latest backup info
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: latestInfoCol.height + 24
                                radius: Dimensions.radiusSmall
                                color: Qt.rgba(1, 1, 1, 0.04)

                                ColumnLayout {
                                    id: latestInfoCol
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 6

                                    RowLayout {
                                        spacing: 8

                                        Rectangle {
                                            Layout.preferredWidth: latestBadgeText.width + 16
                                            Layout.preferredHeight: 22
                                            radius: Dimensions.radiusSmall
                                            color: Theme.withAlpha(Theme.success, 0.15)

                                            Text {
                                                id: latestBadgeText
                                                anchors.centerIn: parent
                                                text: qsTr("Son Yedek")
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                                color: Theme.success
                                            }
                                        }

                                        Text {
                                            text: backupSection.latestBackup.date || ""
                                            font.pixelSize: 12
                                            color: Theme.textMuted
                                        }
                                    }

                                    RowLayout {
                                        spacing: 16

                                        Text {
                                            text: (backupSection.latestBackup.sizeFormatted || "0 B")
                                            font.pixelSize: 12
                                            color: Theme.textSecondary
                                        }

                                        Text {
                                            property int fc: backupSection.latestBackup.fileCount || 0
                                            text: fc + " " + qsTr("dosya")
                                            font.pixelSize: 12
                                            color: Theme.textSecondary
                                            visible: fc > 0
                                        }

                                        Text {
                                            property int bc: backupSection.gameBackups.length
                                            text: bc + " " + qsTr("yedek mevcut")
                                            font.pixelSize: 12
                                            color: Theme.textMuted
                                        }
                                    }
                                }
                            }

                            // Action buttons
                            RowLayout {
                                spacing: 12

                                // Restore latest button
                                Rectangle {
                                    Layout.preferredWidth: restoreBtnContent.width + 40
                                    Layout.preferredHeight: 44
                                    radius: Dimensions.radiusStandard
                                    color: restoreBtnMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.2) : Theme.withAlpha(Theme.warning, 0.1)
                                    border.color: Theme.withAlpha(Theme.warning, restoreBtnMouse.containsMouse ? 0.4 : 0.2)
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Restore latest backup")

                                    Row {
                                        id: restoreBtnContent
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            text: "\u21BA"
                                            font.pixelSize: 16
                                            color: Theme.warning
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: qsTr("Orijinale Dön")
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            color: Theme.warning
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: restoreBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var latest = BackupManager.getLatestBackup(root.gameId)
                                            if (latest && latest.id) {
                                                BackupManager.restoreBackup(latest.id)
                                            }
                                        }
                                    }
                                }

                                // Delete all backups button
                                Rectangle {
                                    Layout.preferredWidth: deleteBtnContent.width + 40
                                    Layout.preferredHeight: 44
                                    radius: Dimensions.radiusStandard
                                    color: deleteBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                                    border.color: Qt.rgba(1, 1, 1, deleteBtnMouse.containsMouse ? 0.15 : 0.08)
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Delete all backups")

                                    Row {
                                        id: deleteBtnContent
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            text: "\uD83D\uDDD1"
                                            font.pixelSize: 14
                                            color: Theme.textMuted
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: qsTr("Yedekleri Sil")
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            color: Theme.textMuted
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: deleteBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var backups = BackupManager.getBackupsForGame(root.gameId)
                                            for (var i = 0; i < backups.length; i++) {
                                                BackupManager.deleteBackup(backups[i].id)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // No backups state
                        RowLayout {
                            visible: !backupSection.hasBackups && !BackupManager.isRestoring
                            spacing: 8

                            Text {
                                text: "\u2139"
                                font.pixelSize: 16
                                color: Theme.textMuted
                                Layout.alignment: Qt.AlignTop
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Bu oyun için henüz yedek bulunmuyor. Çeviri uygulandığında otomatik olarak oluşturulacak.")
                                font.pixelSize: 13
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // ===== SCREENSHOTS SECTION =====
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 12
                visible: root.screenshots.length > 0

                Text {
                    text: qsTr("Ekran Görüntüleri")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "white"
                }

                // Horizontal scroll
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 192
                    clip: true

                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                    Row {
                        spacing: 12

                        Repeater {
                            model: root.screenshots

                            Rectangle {
                                width: 320
                                height: 180
                                radius: Dimensions.radiusStandard
                                color: Theme.surfaceActive
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: modelData
                                    fillMode: Image.PreserveAspectCrop
                                    sourceSize: Qt.size(640, 360)
                                    asynchronous: true
                                    cache: true
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 32 }
        }
    }

    // ===== INFO ROW COMPONENT =====
    component InfoRow: Item {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 32

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Text {
                Layout.preferredWidth: 100
                text: label
                font.pixelSize: 13
                color: Theme.textMuted
            }

            Text {
                Layout.fillWidth: true
                text: value
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
        }
    }
}
