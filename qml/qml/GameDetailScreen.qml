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
        if (lower === "professional") return "Profesyonel"
        if (lower === "verified") return "Doğrulanmış"
        if (lower === "community") return "Topluluk"
        if (lower === "ai") return "Yapay Zeka"
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
                                text: "Onaylı Türkçe Çeviri"
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
                                    text: "Steam'de Aç"
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
                                    text: "Çeviriyi Başlat"
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
                        text: "Steam bilgileri yükleniyor..."
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
                        text: "Hakkında"
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
                            text: "Detaylar"
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
                                    label: "Geliştirici"
                                    value: root.developers.join(", ")
                                    visible: root.developers.length > 0
                                }

                                InfoRow {
                                    label: "Yayıncı"
                                    value: root.publishers.join(", ")
                                    visible: root.publishers.length > 0
                                }

                                InfoRow {
                                    label: "Çıkış Tarihi"
                                    value: root.releaseDate
                                    visible: root.releaseDate !== ""
                                }

                                InfoRow {
                                    label: "Türler"
                                    value: root.genres.join(", ")
                                    visible: root.genres.length > 0
                                }
                            }
                        }
                    }

                    // Right - Rating
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "Değerlendirme"
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
                                                if (root.metacriticScore >= 75) return "Çok Olumlu"
                                                if (root.metacriticScore >= 50) return "Karışık"
                                                if (root.metacriticScore > 0) return "Olumsuz"
                                                return "Puan yok"
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
                                        text: "Platformlar:"
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
                                        text: root.price === "" ? "Ücretsiz" : root.price
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
                    text: "Çeviri Bilgileri"
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

                        InfoRow { label: "Reçete Sürümü"; value: "v" + root.recipeVersion }
                        InfoRow { label: "Metin Sayısı"; value: root.stringCount + " adet" }
                        InfoRow { label: "Kapsam"; value: root.coverage }
                        InfoRow { label: "Dosya Sayısı"; value: root.fileCount + " dosya" }
                        InfoRow { label: "Hazırlayan"; value: root.author; visible: root.author !== "" }
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
                            text: "Bu oyun için henüz çeviri reçetesi bulunmuyor."
                            font.pixelSize: 13
                            color: Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ===== BACKUP SECTION =====
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
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

                    // Header
                    RowLayout {
                        spacing: 12

                        Text {
                            text: "\uD83D\uDCC1"  // Folder
                            font.pixelSize: 18
                        }

                        Text {
                            text: "Yedekleme Yönetimi"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: "white"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Çeviri uygulamadan önce oyun dosyaları otomatik olarak yedeklenir."
                        font.pixelSize: 13
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                    }

                    // Restore button
                    Rectangle {
                        Layout.preferredWidth: restoreBtnContent.width + 40
                        Layout.preferredHeight: 44
                        radius: Dimensions.radiusStandard
                        color: restoreBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
                        border.color: restoreBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        Row {
                            id: restoreBtnContent
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: "\u21BA"  // Refresh
                                font.pixelSize: 16
                                color: restoreBtnMouse.containsMouse ? "white" : Theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: "Yedeği Geri Yükle"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: restoreBtnMouse.containsMouse ? "white" : Theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: restoreBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.restoreClicked()
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
                    text: "Ekran Görüntüleri"
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
