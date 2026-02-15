import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * GameDetailScreen.qml — Modern cinematic game detail page
 *
 * Layout: Full-width hero with parallax → glass cards below
 * Sections: Hero, Quick Stats, About, Screenshots, Translation Status,
 *           Runtime (Unity), Backup Management
 */
Item {
    id: root

    // ===== INPUT PROPERTIES (set by Main.qml) =====
    property string gameId: ""
    property string gameName: "Game Name"
    property string steamAppId: ""
    property string imageUrl: ""
    property string heroImageUrl: ""
    property bool verified: false
    property string engine: ""
    property bool hasTranslation: false
    property bool isEditorsPick: false
    property string editorsNote: ""

    // ===== STEAM DATA =====
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
    property var screenshots: []
    property bool isLoadingSteamDetails: false
    property bool steamFetchFailed: false

    // ===== CONTRIBUTORS =====
    property var contributors: []  // [{name, role}]

    // ===== RUNTIME (BepInEx) =====
    property bool isUnityGame: false
    property bool runtimeNeeded: false
    property bool runtimeInstalled: false
    property bool runtimeUpToDate: false
    property string bepinexVersion: ""
    property string xunityVersion: ""
    property string unityBackend: ""
    property string unityVersion: ""
    property bool hasAntiCheat: false
    property string antiCheatName: ""
    property bool isInstallingRuntime: false

    // ===== MANUAL GAME FLAG =====
    property bool isManualGame: false

    // ===== INSTALL STATE =====
    property bool isInstallingTranslation: false
    property real installProgress: 0
    property string installStatus: ""

    // ===== UI STATE =====
    property bool descriptionExpanded: false
    property real scrollY: 0
    property int _reveal: -1

    signal backClicked()
    signal translateClicked()

    // ===== DATA LOGIC =====

    function resetDetails() {
        description = ""; developers = []; publishers = []
        releaseDate = ""; genres = []; metacriticScore = 0
        hasWindows = true; hasMac = false; hasLinux = false
        price = ""; discountPercent = 0; hasSteamDetails = false
        screenshots = []; isLoadingSteamDetails = false; steamFetchFailed = false
        contributors = []
        isEditorsPick = false; editorsNote = ""
        isUnityGame = false; runtimeNeeded = false; runtimeInstalled = false
        runtimeUpToDate = false; bepinexVersion = ""; xunityVersion = ""
        unityBackend = ""; unityVersion = ""; hasAntiCheat = false
        antiCheatName = ""; isInstallingRuntime = false
        isInstallingTranslation = false; installProgress = 0; installStatus = ""
        isManualGame = false
        descriptionExpanded = false
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
        var cached = GameService.getSteamDetails(steamAppId)
        if (cached && cached.description !== undefined)
            populateSteamDetails(cached)
        else
            isLoadingSteamDetails = true
        GameService.fetchSteamDetails(steamAppId)
    }

    onGameIdChanged: {
        if (gameId === "") return

        var d = GameService.getGameDetails(gameId)

        // Contributors
        contributors = d.contributors || []

        // Unity runtime
        isUnityGame = d.isUnityGame || false
        runtimeNeeded = d.runtimeNeeded || false
        runtimeInstalled = d.runtimeInstalled || false
        runtimeUpToDate = d.runtimeUpToDate || false
        bepinexVersion = d.bepinexVersion || ""
        xunityVersion = d.xunityVersion || ""
        unityBackend = d.unityBackend || "unknown"
        unityVersion = d.unityVersion || ""
        hasAntiCheat = d.hasAntiCheat || false
        antiCheatName = d.antiCheatName || ""

        // Reset scroll and start staggered entrance
        mainFlick.contentY = 0
        root._reveal = -1
        _contentSlide.y = 20
        _slideAnim.restart()
        _stagger.restart()
    }

    Connections {
        target: GameService
        function onSteamDetailsFetched(appId, details) {
            if (appId === root.steamAppId) root.populateSteamDetails(details)
        }
        function onSteamDetailsFetchError(appId, error) {
            if (appId === root.steamAppId) {
                root.isLoadingSteamDetails = false
                root.steamFetchFailed = true
            }
        }
        function onRuntimeInstallFinished(gId, success, error) {
            if (gId === root.gameId) {
                root.isInstallingRuntime = false
                if (success) {
                    var rt = GameService.getRuntimeStatus(root.gameId)
                    if (rt) {
                        root.runtimeInstalled = rt.installed || false
                        root.runtimeUpToDate = rt.upToDate || false
                        root.bepinexVersion = rt.bepinexVersion || ""
                        root.xunityVersion = rt.xunityVersion || ""
                    }
                }
            }
        }
        function onTranslationInstallStarted(gId) {
            if (gId === root.gameId) {
                root.isInstallingTranslation = true
                root.installProgress = 0
                root.installStatus = qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallProgress(gId, progress, status) {
            if (gId === root.gameId) {
                root.installProgress = progress
                root.installStatus = status || qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallCompleted(gId, success, message) {
            if (gId === root.gameId) {
                root.isInstallingTranslation = false
                root.installProgress = 0
                root.installStatus = ""
            }
        }
    }


    // =========================================================================
    // BACKGROUND — Hero image with parallax + gradient overlay
    // =========================================================================

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        Image {
            id: heroBackground
            anchors.fill: parent
            source: root.heroImageUrl !== "" ? root.heroImageUrl : root.imageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            opacity: 0.30
            visible: source !== ""
            transform: Translate { y: -root.scrollY * 0.15 }
        }

        // Game logo overlay — full-width, fading top-to-bottom
        Image {
            id: gameLogo
            anchors.left: parent.left
            anchors.right: parent.right
            y: 20 - root.scrollY * 0.06
            height: parent.height * 0.6
            fillMode: Image.PreserveAspectFit
            source: root.steamAppId !== "" ? "https://cdn.akamai.steamstatic.com/steam/apps/" + root.steamAppId + "/logo.png" : ""
            asynchronous: true; cache: true
            visible: status === Image.Ready
            opacity: 0
            states: State {
                name: "visible"; when: gameLogo.status === Image.Ready
                PropertyChanges { target: gameLogo; opacity: 0.15 }
            }
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        }

        // Fade mask: logo fades out toward the bottom
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.20; color: Theme.withAlpha(Theme.bgPrimary, 0.30) }
                GradientStop { position: 0.45; color: Theme.withAlpha(Theme.bgPrimary, 0.80) }
                GradientStop { position: 0.65; color: Theme.bgPrimary }
            }
        }

        // Main gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bgPrimary, 0.15) }
                GradientStop { position: 0.35; color: Theme.withAlpha(Theme.bgPrimary, 0.75) }
                GradientStop { position: 0.65; color: Theme.bgPrimary }
            }
        }
    }

    // =========================================================================
    // FLOATING BACK BUTTON — bookmark tab style
    // =========================================================================

    Rectangle {
        id: backBtn
        x: 0; y: Dimensions.marginML
        width: backMouse.containsMouse ? 46 : 42
        height: 38
        topLeftRadius: 0; bottomLeftRadius: 0
        topRightRadius: 12; bottomRightRadius: 12
        color: backMouse.containsMouse
            ? Theme.withAlpha(Theme.bgPrimary, 0.82)
            : Theme.withAlpha(Theme.bgPrimary, 0.50)
        border { color: Theme.glassBorder; width: 1 }
        z: Dimensions.zDialog

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // Hide left border (flush with edge)
        Rectangle {
            x: -1; y: 0; width: 2; height: parent.height
            color: parent.color
        }

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Geri")
        activeFocusOnTab: true
        Keys.onReturnPressed: root.backClicked()
        Keys.onSpacePressed: root.backClicked()

        Text {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: -1
            text: "\u2190"
            font.pixelSize: Dimensions.fontXL
            color: backMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }
        MouseArea {
            id: backMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.backClicked()
        }
    }

    // =========================================================================
    // CONTENT ENTRANCE ANIMATION
    // =========================================================================

    // Global slide + per-section staggered fade
    NumberAnimation {
        id: _slideAnim
        target: _contentSlide; property: "y"
        from: 20; to: 0; duration: Dimensions.animSlow
        easing.type: Easing.OutCubic
    }

    Timer {
        id: _stagger
        interval: 80; repeat: true
        onTriggered: {
            root._reveal++
            if (root._reveal >= 6) stop()
        }
    }

    // =========================================================================
    // MAIN CONTENT
    // =========================================================================

    Flickable {
        id: mainFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        onContentYChanged: root.scrollY = contentY

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "transparent" }
            contentItem: Rectangle {
                implicitWidth: 6; radius: 3
                color: parent.pressed ? Theme.scrollbarThumbHover : Theme.scrollbarThumb
            }
        }

        ColumnLayout {
            id: contentCol
            width: mainFlick.width
            spacing: 0
            transform: Translate { id: _contentSlide }

            // =================================================================
            // HERO SECTION
            // =================================================================

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                opacity: root._reveal >= 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                // Cover image (tall, left side, overlaps below)
                Rectangle {
                    id: coverFrame
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginXL
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -40
                    width: 240; height: 340
                    radius: Dimensions.radiusStandard
                    color: Theme.surfaceActive
                    clip: true
                    z: 2

                    Image {
                        id: coverImg
                        anchors.fill: parent
                        source: root.imageUrl
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(440, 620)
                        asynchronous: true; cache: true
                        visible: false
                        property bool triedFallback: false
                        onStatusChanged: {
                            if (status === Image.Error && !triedFallback && root.steamAppId !== "") {
                                triedFallback = true
                                source = "https://cdn.akamai.steamstatic.com/steam/apps/" + root.steamAppId + "/header.jpg"
                            }
                        }
                    }

                    // Rounded mask
                    Item {
                        id: coverMask
                        anchors.fill: parent; visible: false; layer.enabled: true
                        Rectangle { anchors.fill: parent; radius: Dimensions.radiusStandard; color: "white" }
                    }

                    MultiEffect {
                        id: coverEffect
                        anchors.fill: parent
                        source: coverImg
                        maskEnabled: true; maskSource: coverMask
                        opacity: coverImg.status === Image.Ready ? 1.0 : 0
                        Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                    }

                    // Placeholder
                    Text {
                        anchors.centerIn: parent
                        visible: coverImg.status !== Image.Ready
                        text: root.gameName.length >= 2 ? root.gameName.substring(0, 2).toUpperCase() : "?"
                        font.pixelSize: Dimensions.fontHero
                        font.weight: Font.Bold
                        color: Theme.textMuted
                    }

                    // Glass border (subtle, always visible)
                    Rectangle {
                        anchors.fill: parent; radius: parent.radius
                        color: "transparent"
                        border.color: Theme.glassBorder; border.width: 1
                    }
                }

                // Info column (right of cover, bottom-aligned)
                ColumnLayout {
                    anchors.left: coverFrame.right
                    anchors.leftMargin: Dimensions.marginLG
                    anchors.right: parent.right
                    anchors.rightMargin: Dimensions.marginXL
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Dimensions.marginMD
                    spacing: Dimensions.spacingLG

                    // Badge row
                    Row {
                        spacing: Dimensions.spacingMD

                        // Verified badge
                        Rectangle {
                            visible: root.verified
                            width: verifiedRow.width + 20; height: 26
                            radius: Dimensions.radiusFull
                            color: Theme.verifiedBg

                            Row {
                                id: verifiedRow
                                anchors.centerIn: parent; spacing: Dimensions.spacingSM
                                Text { text: "\u2713"; font.pixelSize: Dimensions.fontSM; color: Theme.verifiedText; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("Onaylı Çeviri"); font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold; color: Theme.verifiedText; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }

                        // Editor's pick badge
                        Rectangle {
                            visible: root.isEditorsPick
                            width: editorsPickRow.width + 20; height: 26
                            radius: Dimensions.radiusFull
                            color: Theme.withAlpha(Theme.warning, 0.12)
                            border.color: Theme.withAlpha(Theme.warning, 0.25); border.width: 1

                            Row {
                                id: editorsPickRow
                                anchors.centerIn: parent; spacing: Dimensions.spacingSM
                                Text { text: "\u2B50"; font.pixelSize: Dimensions.fontCaption; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("Editörün Seçimi"); font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold; color: Theme.warning; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // Game name
                    Text {
                        Layout.fillWidth: true
                        text: root.gameName
                        font.pixelSize: Dimensions.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: Dimensions.letterSpacingHeadline
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    // Editor's note (play recommendation)
                    Text {
                        Layout.fillWidth: true
                        visible: root.isEditorsPick && root.editorsNote !== ""
                        text: "\u201C" + root.editorsNote + "\u201D"
                        font.pixelSize: Dimensions.fontBody
                        font.italic: true
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        opacity: 0.85
                    }

                    // Action buttons
                    RowLayout {
                        spacing: Dimensions.spacingLG

                        // Primary CTA (only for manually selected games with translation package)
                        Rectangle {
                            id: ctaBtn
                            visible: root.isManualGame && root.hasTranslation
                            implicitWidth: ctaRow.width + 36; implicitHeight: 42
                            radius: Dimensions.radiusStandard
                            color: root.isInstallingTranslation
                                ? Theme.withAlpha(Theme.primary, 0.7)
                                : (ctaMouse.containsMouse ? Theme.primaryHover : Theme.primary)
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            Accessible.role: Accessible.Button
                            Accessible.name: root.isInstallingTranslation
                                ? root.installStatus
                                : (root.hasTranslation ? qsTr("Çeviriyi Kur") : qsTr("Çeviriyi Başlat"))

                            // Progress underlay
                            Rectangle {
                                visible: root.isInstallingTranslation
                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                width: parent.width * root.installProgress
                                radius: parent.radius
                                color: Theme.primary
                                Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                            }

                            Row {
                                id: ctaRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                Text {
                                    text: root.isInstallingTranslation ? "\u23F3" : (root.hasTranslation ? "\uD83C\uDF10" : "\u270F")
                                    font.pixelSize: Dimensions.fontMD; color: Theme.textOnColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: root.isInstallingTranslation
                                        ? (root.installProgress > 0 ? qsTr("%1%").arg(Math.round(root.installProgress * 100)) : root.installStatus)
                                        : (root.hasTranslation ? qsTr("Çeviriyi Kur") : qsTr("Çeviriyi Başlat"))
                                    font.pixelSize: Dimensions.fontMD; font.weight: Font.DemiBold
                                    color: Theme.textOnColor; anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            MouseArea {
                                id: ctaMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: root.isInstallingTranslation ? Qt.WaitCursor : Qt.PointingHandCursor
                                enabled: !root.isInstallingTranslation
                                onClicked: root.translateClicked()
                            }
                        }

                        // No translation available notice (manual games without package)
                        Rectangle {
                            visible: root.isManualGame && !root.hasTranslation
                            implicitWidth: noTransRow.width + 36; implicitHeight: 42
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.textMuted, 0.08)
                            border.color: Theme.withAlpha(Theme.textMuted, 0.15); border.width: 1

                            Row {
                                id: noTransRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                Text {
                                    text: "\u26A0"
                                    font.pixelSize: Dimensions.fontMD; color: Theme.textMuted
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: qsTr("Bu oyun için Türkçe yama mevcut değil")
                                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                                    color: Theme.textMuted
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }

                        // Steam link — branded button with logo
                        Rectangle {
                            id: steamBtn
                            visible: root.steamAppId !== ""
                            implicitWidth: steamContent.width + 28; implicitHeight: 42
                            radius: Dimensions.radiusStandard
                            color: steamMouse.containsMouse ? "#2a475e" : "#1b2838"
                            border.color: steamMouse.containsMouse
                                ? Theme.withAlpha("#66c0f4", 0.50)
                                : Theme.withAlpha("#66c0f4", 0.20)
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 180 } }
                            Behavior on border.color { ColorAnimation { duration: 180 } }

                            Accessible.role: Accessible.Link
                            Accessible.name: qsTr("Steam'de Aç")

                            Row {
                                id: steamContent; anchors.centerIn: parent; spacing: 8

                                // Steam logo (real SVG)
                                Image {
                                    id: steamSvgSrc
                                    source: "qrc:/qt/qml/MakineAI/resources/icons/steam.svg"
                                    sourceSize: Qt.size(20, 20)
                                    width: 20; height: 20
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: false
                                }
                                MultiEffect {
                                    source: steamSvgSrc
                                    width: 20; height: 20
                                    anchors.verticalCenter: parent.verticalCenter
                                    brightness: steamMouse.containsMouse ? 0.35 : 0.0
                                    Behavior on brightness { NumberAnimation { duration: 180 } }
                                }

                                Text {
                                    text: "Steam"
                                    font.pixelSize: Dimensions.fontMD; font.weight: Font.DemiBold
                                    color: steamMouse.containsMouse ? "#c5e8ff" : "#66c0f4"
                                    anchors.verticalCenter: parent.verticalCenter
                                    Behavior on color { ColorAnimation { duration: 180 } }
                                }
                            }

                            MouseArea {
                                id: steamMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Qt.openUrlExternally("https://store.steampowered.com/app/" + root.steamAppId)
                            }
                        }

                    }
                }
            }

            // =================================================================
            // QUICK STATS ROW
            // =================================================================

            Item { Layout.preferredHeight: 56; Layout.fillWidth: true }

            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                spacing: Dimensions.spacingMD
                visible: root.hasSteamDetails
                opacity: root._reveal >= 1 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                // Metacritic
                Rectangle {
                    visible: root.metacriticScore > 0
                    width: mcRow.width + 20; height: 30
                    radius: Dimensions.radiusFull
                    property color mc: root.metacriticScore >= 75 ? Theme.scoreExcellent : root.metacriticScore >= 50 ? Theme.scoreFair : Theme.scorePoor
                    color: Theme.withAlpha(mc, 0.10)
                    border.color: Theme.withAlpha(mc, 0.20); border.width: 1
                    Row {
                        id: mcRow; anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        Text { text: "Metacritic"; font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: root.metacriticScore.toString(); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: parent.parent.mc; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                // Price
                Rectangle {
                    visible: root.price !== ""
                    width: priceRow.width + 20; height: 30
                    radius: Dimensions.radiusFull
                    property color pc: root.discountPercent > 0 ? Theme.success : Theme.textSecondary
                    color: Theme.withAlpha(pc, 0.10)
                    border.color: Theme.withAlpha(pc, 0.20); border.width: 1
                    Row {
                        id: priceRow; anchors.centerIn: parent; spacing: Dimensions.spacingSM
                        Text { visible: root.discountPercent > 0; text: "-" + root.discountPercent + "%"; font.pixelSize: Dimensions.fontCaption; font.weight: Font.Bold; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: root.price; font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: parent.parent.pc; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                // Genres
                Rectangle {
                    visible: root.genres.length > 0
                    width: genreText.width + 20; height: 30
                    radius: Dimensions.radiusFull
                    color: Theme.withAlpha(Theme.textPrimary, 0.05)
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.08); border.width: 1
                    Text { id: genreText; anchors.centerIn: parent; text: root.genres.slice(0, 2).join(", "); font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary }
                }

                // Platforms
                Rectangle {
                    visible: root.hasWindows || root.hasMac || root.hasLinux
                    width: platText.width + 20; height: 30
                    radius: Dimensions.radiusFull
                    color: Theme.withAlpha(Theme.textPrimary, 0.05)
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.08); border.width: 1
                    Text {
                        id: platText; anchors.centerIn: parent
                        text: { var p = []; if (root.hasWindows) p.push("Win"); if (root.hasMac) p.push("Mac"); if (root.hasLinux) p.push("Linux"); return p.join(" / ") }
                        font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary
                    }
                }
            }

            // =================================================================
            // ABOUT SECTION
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                spacing: Dimensions.spacingLG
                visible: root.description !== "" || root.developers.length > 0
                opacity: root._reveal >= 2 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                Text {
                    text: qsTr("Hakkında")
                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                // Description card
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.description !== ""
                    implicitHeight: aboutCol.height + Dimensions.marginML * 2
                    radius: Dimensions.radiusStandard
                    color: Theme.glassBackground
                    border.color: Theme.glassBorder; border.width: 1

                    ColumnLayout {
                        id: aboutCol
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingMD

                        Text {
                            Layout.fillWidth: true
                            text: root.description
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap; lineHeight: 1.6
                            maximumLineCount: root.descriptionExpanded ? 9999 : 4
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: !root.descriptionExpanded
                            text: qsTr("Daha fazla göster...")
                            font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium
                            color: Theme.primary
                            opacity: expandMouse.containsMouse ? 1.0 : 0.7
                            MouseArea {
                                id: expandMouse; anchors.fill: parent; anchors.margins: -4
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: root.descriptionExpanded = true
                            }
                        }
                    }
                }

                // Details card (developer, publisher, etc.)
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.developers.length > 0 || root.publishers.length > 0
                    implicitHeight: detailsCol.height + Dimensions.marginML * 2
                    radius: Dimensions.radiusStandard
                    color: Theme.glassBackground
                    border.color: Theme.glassBorder; border.width: 1

                    ColumnLayout {
                        id: detailsCol
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: Dimensions.marginML
                        spacing: 0

                        DetailRow { label: qsTr("Geliştirici"); value: root.developers.join(", "); visible: root.developers.length > 0 }
                        DetailRow { label: qsTr("Yayıncı"); value: root.publishers.join(", "); visible: root.publishers.length > 0 }
                        DetailRow { label: qsTr("Çıkış Tarihi"); value: root.releaseDate; visible: root.releaseDate !== "" }
                        DetailRow { label: qsTr("Motor"); value: root.engine; visible: root.engine !== "" }
                        DetailRow { label: qsTr("Türler"); value: root.genres.join(", "); visible: root.genres.length > 0 }
                    }
                }
            }

            // =================================================================
            // SCREENSHOTS
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true; visible: root.screenshots.length > 0 }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingLG
                visible: root.screenshots.length > 0
                opacity: root._reveal >= 3 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                Text {
                    Layout.leftMargin: Dimensions.marginXL
                    text: qsTr("Ekran Görüntüleri")
                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220

                    ListView {
                        id: screenshotList
                        anchors.fill: parent
                        orientation: ListView.Horizontal
                        spacing: Dimensions.spacingLG; clip: true
                        leftMargin: Dimensions.marginXL; rightMargin: Dimensions.marginXL
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.screenshots

                        delegate: Rectangle {
                            required property string modelData
                            width: 380; height: 214
                            radius: Dimensions.radiusStandard
                            color: Theme.surfaceActive; clip: true

                            Image {
                                anchors.fill: parent
                                source: modelData
                                fillMode: Image.PreserveAspectCrop
                                sourceSize: Qt.size(760, 428)
                                asynchronous: true; cache: true
                            }
                            Rectangle {
                                anchors.fill: parent; radius: parent.radius
                                color: "transparent"
                                border.color: Theme.glassBorder; border.width: 1
                            }
                        }
                    }

                    // Left arrow
                    Rectangle {
                        anchors.left: parent.left; anchors.leftMargin: Dimensions.spacingMD
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36; height: 36; radius: 18
                        color: ssLeftMouse.containsMouse ? Theme.withAlpha(Theme.bgPrimary, 0.90) : Theme.withAlpha(Theme.bgPrimary, 0.65)
                        border.color: Theme.glassBorder; border.width: 1
                        visible: screenshotList.contentX > screenshotList.originX + 10
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Text { anchors.centerIn: parent; text: "\u2190"; font.pixelSize: Dimensions.fontMD; color: Theme.textPrimary }
                        MouseArea {
                            id: ssLeftMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: screenshotList.contentX = Math.max(screenshotList.originX, screenshotList.contentX - 400)
                        }
                    }

                    // Right arrow
                    Rectangle {
                        anchors.right: parent.right; anchors.rightMargin: Dimensions.spacingMD
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36; height: 36; radius: 18
                        color: ssRightMouse.containsMouse ? Theme.withAlpha(Theme.bgPrimary, 0.90) : Theme.withAlpha(Theme.bgPrimary, 0.65)
                        border.color: Theme.glassBorder; border.width: 1
                        visible: screenshotList.contentX < screenshotList.contentWidth - screenshotList.width - 10
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Text { anchors.centerIn: parent; text: "\u2192"; font.pixelSize: Dimensions.fontMD; color: Theme.textPrimary }
                        MouseArea {
                            id: ssRightMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: screenshotList.contentX = Math.min(screenshotList.contentWidth - screenshotList.width, screenshotList.contentX + 400)
                        }
                    }
                }
            }

            // =================================================================
            // CONTRIBUTORS
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                spacing: Dimensions.spacingLG
                opacity: root._reveal >= 4 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                Text {
                    text: qsTr("Katkıda Bulunanlar")
                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                // Contributors list (when available)
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.contributors.length > 0
                    implicitHeight: contributorsCol.height + Dimensions.marginML * 2
                    radius: Dimensions.radiusStandard
                    color: Theme.glassBackground
                    border.color: Theme.glassBorder; border.width: 1

                    ColumnLayout {
                        id: contributorsCol
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingLG

                        Repeater {
                            model: root.contributors
                            RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingLG

                                Rectangle {
                                    width: 40; height: 40; radius: 20
                                    color: Theme.withAlpha(Theme.primary, 0.12)
                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            var name = modelData.name || ""
                                            if (name.length >= 2) return name.substring(0, 2).toUpperCase()
                                            return name.toUpperCase()
                                        }
                                        font.pixelSize: Dimensions.fontSM; font.weight: Font.Bold
                                        color: Theme.primary
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingXXS
                                    Text {
                                        text: modelData.name || ""
                                        font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }
                                    Text {
                                        visible: (modelData.role || "") !== ""
                                        text: modelData.role || ""
                                        font.pixelSize: Dimensions.fontCaption
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }
                    }
                }

                // Placeholder when no contributors
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.contributors.length === 0
                    implicitHeight: 56; radius: Dimensions.radiusStandard
                    color: Theme.glassBackground; border.color: Theme.glassBorder; border.width: 1
                    Row {
                        anchors.centerIn: parent; spacing: Dimensions.spacingLG
                        Text { text: "\uD83D\uDC65"; font.pixelSize: Dimensions.fontTitle; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("Henüz katkıda bulunan bilgisi eklenmedi."); font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
            }

            // =================================================================
            // RUNTIME (Unity BepInEx) — conditional
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true; visible: root.isUnityGame && root.runtimeNeeded }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
                spacing: Dimensions.spacingLG
                visible: root.isUnityGame && root.runtimeNeeded
                opacity: root._reveal >= 5 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                Text {
                    text: qsTr("Çeviri Çalışma Ortamı")
                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold; color: Theme.textPrimary
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: runtimeCol.height + Dimensions.marginML * 2
                    radius: Dimensions.radiusStandard
                    color: Theme.glassBackground; border.color: Theme.glassBorder; border.width: 1

                    ColumnLayout {
                        id: runtimeCol
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingLG

                        // Status header
                        RowLayout {
                            spacing: Dimensions.spacingLG

                            Rectangle {
                                width: 40; height: 40; radius: 20
                                color: Theme.withAlpha(root.runtimeInstalled ? Theme.success : Theme.textPrimary, 0.12)
                                Text {
                                    anchors.centerIn: parent
                                    text: root.runtimeInstalled ? "\u2713" : "\u2193"
                                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.Bold
                                    color: root.runtimeInstalled ? Theme.success : Theme.textMuted
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                                Text {
                                    text: root.runtimeInstalled
                                        ? (root.runtimeUpToDate ? qsTr("BepInEx Kurulu ve Güncel") : qsTr("BepInEx Güncelleme Mevcut"))
                                        : qsTr("BepInEx Kurulu Değil")
                                    font.pixelSize: Dimensions.fontMD; font.weight: Font.DemiBold
                                    color: root.runtimeInstalled ? Theme.success : Theme.textSecondary
                                }
                                Text {
                                    visible: root.runtimeInstalled && root.bepinexVersion !== ""
                                    text: "BepInEx " + root.bepinexVersion
                                    font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
                                }
                                Text {
                                    visible: !root.runtimeInstalled
                                    text: qsTr("Çevirinin çalışması için BepInEx gereklidir")
                                    font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
                                }
                            }

                            // Backend badge
                            Rectangle {
                                visible: root.unityBackend !== "" && root.unityBackend !== "unknown"
                                width: backendLbl.width + 12; height: 24
                                radius: Dimensions.radiusFull
                                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                                Text { id: backendLbl; anchors.centerIn: parent; text: root.unityBackend; font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary }
                            }
                        }

                        // Anti-cheat warning
                        Rectangle {
                            Layout.fillWidth: true; visible: root.hasAntiCheat
                            implicitHeight: acRow.height + Dimensions.marginSM * 2
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.destructive, 0.08)
                            border.color: Theme.withAlpha(Theme.destructive, 0.20); border.width: 1

                            RowLayout {
                                id: acRow
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.top: parent.top; anchors.margins: Dimensions.marginSM
                                spacing: Dimensions.spacingMD
                                Text { text: "\u26A0"; font.pixelSize: Dimensions.fontTitle; color: Theme.destructive }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Bu oyunda %1 tespit edildi. BepInEx ile uyumsuz olabilir.").arg(root.antiCheatName)
                                    font.pixelSize: Dimensions.fontBody; color: Theme.destructive
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // Action buttons
                        RowLayout {
                            spacing: Dimensions.spacingLG

                            // Install/Update
                            Rectangle {
                                implicitWidth: rtBtnRow.width + 32; implicitHeight: 38
                                radius: Dimensions.radiusStandard
                                color: rtBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary
                                opacity: root.isInstallingRuntime ? 0.6 : 1.0
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                Accessible.role: Accessible.Button
                                Accessible.name: root.isInstallingRuntime ? qsTr("Kuruluyor...") : (!root.runtimeInstalled ? qsTr("BepInEx Kur") : qsTr("Güncelle"))

                                Row {
                                    id: rtBtnRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                    Text {
                                        text: root.isInstallingRuntime ? qsTr("Kuruluyor...") : (!root.runtimeInstalled ? qsTr("BepInEx Kur") : qsTr("Güncelle"))
                                        font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.textOnColor
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    id: rtBtnMouse; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor; enabled: !root.isInstallingRuntime
                                    onClicked: { root.isInstallingRuntime = true; GameService.installRuntime(root.gameId) }
                                }
                            }

                            // Uninstall
                            Rectangle {
                                visible: root.runtimeInstalled
                                implicitWidth: rtUnRow.width + 32; implicitHeight: 38
                                radius: Dimensions.radiusStandard
                                color: rtUnMouse.containsMouse ? Theme.withAlpha(Theme.destructive, 0.15) : Theme.withAlpha(Theme.textPrimary, 0.06)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.10); border.width: 1
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("BepInEx Kaldır")
                                Row {
                                    id: rtUnRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                    Text { text: qsTr("Kaldır"); font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium; color: Theme.textSecondary; anchors.verticalCenter: parent.verticalCenter }
                                }
                                MouseArea {
                                    id: rtUnMouse; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: GameService.uninstallRuntime(root.gameId)
                                }
                            }
                        }
                    }
                }
            }

            // =================================================================
            // BACKUP MANAGEMENT
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            ColumnLayout {
                id: backupSection
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
                spacing: Dimensions.spacingLG
                opacity: root._reveal >= 6 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                property var gameBackups: BackupManager.getBackupsForGame(root.gameId)
                property bool hasBackups: gameBackups.length > 0
                property var latestBackup: BackupManager.getLatestBackup(root.gameId)

                Connections {
                    target: BackupManager
                    function onBackupsChanged() {
                        backupSection.gameBackups = BackupManager.getBackupsForGame(root.gameId)
                        backupSection.latestBackup = BackupManager.getLatestBackup(root.gameId)
                    }
                    function onBackupRestored(gId) {
                        if (gId === root.gameId) {
                            backupSection.gameBackups = BackupManager.getBackupsForGame(root.gameId)
                            backupSection.latestBackup = BackupManager.getLatestBackup(root.gameId)
                        }
                    }
                }

                Text {
                    text: qsTr("Yedekleme Yönetimi")
                    font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold; color: Theme.textPrimary
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: backupContent.height + Dimensions.marginML * 2
                    radius: Dimensions.radiusStandard
                    color: Theme.glassBackground; border.color: Theme.glassBorder; border.width: 1

                    ColumnLayout {
                        id: backupContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: Dimensions.marginML
                        spacing: Dimensions.spacingLG

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Çeviri uygulamadan önce oyun dosyaları otomatik olarak yedeklenir.")
                            font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; wrapMode: Text.WordWrap
                        }

                        // Restore in progress
                        RowLayout {
                            visible: BackupManager.isRestoring
                            spacing: Dimensions.spacingMD
                            BusyIndicator { width: 20; height: 20; running: visible }
                            Text { text: BackupManager.restoreStatus; font.pixelSize: Dimensions.fontBody; color: Theme.primary }
                        }

                        // Has backups
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: backupSection.hasBackups && !BackupManager.isRestoring
                            spacing: Dimensions.spacingLG

                            // Latest backup info
                            RowLayout {
                                Layout.fillWidth: true; spacing: Dimensions.spacingLG

                                Rectangle {
                                    width: 40; height: 40; radius: 20
                                    color: Theme.withAlpha(Theme.success, 0.12)
                                    Text { anchors.centerIn: parent; text: "\u2713"; font.pixelSize: Dimensions.fontTitle; color: Theme.success }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                                    Text {
                                        text: qsTr("Son Yedek")
                                        font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold; color: Theme.textPrimary
                                    }
                                    Text {
                                        text: {
                                            var b = backupSection.latestBackup
                                            if (!b || !b.date) return ""
                                            var parts = []
                                            parts.push(b.date)
                                            if (b.sizeFormatted) parts.push(b.sizeFormatted)
                                            if (b.fileCount) parts.push(qsTr("%1 dosya").arg(b.fileCount))
                                            return parts.join(" \u2022 ")
                                        }
                                        font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
                                    }
                                }

                                // Count badge
                                Rectangle {
                                    width: countLbl.width + 12; height: 22
                                    radius: Dimensions.radiusFull
                                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                                    Text { id: countLbl; anchors.centerIn: parent; text: qsTr("%1 yedek").arg(backupSection.gameBackups.length); font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary }
                                }
                            }

                            // Buttons
                            RowLayout {
                                spacing: Dimensions.spacingLG

                                // Restore
                                Rectangle {
                                    implicitWidth: restoreRow.width + 32; implicitHeight: 38
                                    radius: Dimensions.radiusStandard
                                    color: restoreMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.20) : Theme.withAlpha(Theme.warning, 0.10)
                                    border.color: Theme.withAlpha(Theme.warning, 0.30); border.width: 1
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Orijinale Dön")

                                    Row {
                                        id: restoreRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                        Text { text: "\u21BB"; font.pixelSize: Dimensions.fontSM; color: Theme.warning; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: qsTr("Orijinale Dön"); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.warning; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                    MouseArea {
                                        id: restoreMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var b = backupSection.latestBackup
                                            if (b && b.id) BackupManager.restoreBackup(b.id)
                                        }
                                    }
                                }

                                // Delete all
                                Rectangle {
                                    implicitWidth: deleteRow.width + 32; implicitHeight: 38
                                    radius: Dimensions.radiusStandard
                                    color: deleteMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : Theme.withAlpha(Theme.textPrimary, 0.04)
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Yedekleri Sil")

                                    Row {
                                        id: deleteRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                        Text { text: qsTr("Yedekleri Sil"); font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                    MouseArea {
                                        id: deleteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: deleteBackupsConfirm.open()
                                    }
                                }
                            }
                        }

                        // No backups
                        Row {
                            visible: !backupSection.hasBackups && !BackupManager.isRestoring
                            spacing: Dimensions.spacingLG
                            Text { text: "\u2139"; font.pixelSize: Dimensions.fontTitle; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("Bu oyun için henüz yedek bulunmuyor."); font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: Dimensions.marginXXL; Layout.fillWidth: true }

        } // end contentCol
    } // end Flickable

    // =========================================================================
    // LOADING OVERLAY (Steam details)
    // =========================================================================

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: loadingRow.width + 40; height: 44
        radius: Dimensions.radiusFull
        color: Theme.withAlpha(Theme.surface, 0.92)
        border.color: Theme.glassBorder; border.width: 1
        visible: root.isLoadingSteamDetails && !root.hasSteamDetails
        z: 5

        RowLayout {
            id: loadingRow; anchors.centerIn: parent; spacing: Dimensions.spacingLG
            BusyIndicator { width: 20; height: 20; running: visible }
            Text { text: qsTr("Steam bilgileri yükleniyor..."); font.pixelSize: Dimensions.fontBody; color: Theme.textSecondary }
        }
    }

    // Error + Retry
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: errorCol.width + 40; height: errorCol.height + 24
        radius: Dimensions.radiusStandard
        color: Theme.withAlpha(Theme.surface, 0.92)
        border.color: Theme.glassBorder; border.width: 1
        visible: root.steamFetchFailed && !root.hasSteamDetails
        z: 5

        ColumnLayout {
            id: errorCol; anchors.centerIn: parent; spacing: Dimensions.spacingMD
            Text { text: qsTr("Steam bilgileri alınamadı"); font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; Layout.alignment: Qt.AlignHCenter }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: retryLbl.width + 24; implicitHeight: 30
                radius: Dimensions.radiusStandard
                color: retryMouse.containsMouse ? Theme.primaryHover : Theme.primary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Tekrar Dene")

                Text { id: retryLbl; anchors.centerIn: parent; text: qsTr("Tekrar Dene"); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.textOnColor }
                MouseArea { id: retryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.steamFetchFailed = false; root.isLoadingSteamDetails = true; GameService.fetchSteamDetails(root.steamAppId) } }
            }
        }
    }

    // =========================================================================
    // FOCUS INDICATOR
    // =========================================================================

    Accessible.role: Accessible.Pane
    Accessible.name: root.gameName

    // ===== CONFIRM DIALOGS =====
    ConfirmDialog {
        id: deleteBackupsConfirm
        parent: Overlay.overlay
        title: qsTr("Yedekleri Sil")
        message: qsTr("Bu oyuna ait tüm yedek dosyaları kalıcı olarak silinecek. Bu işlem geri alınamaz.")
        confirmText: qsTr("Sil")
        onConfirmed: {
            var all = backupSection.gameBackups
            for (var i = 0; i < all.length; i++)
                BackupManager.deleteBackup(all[i].id)
        }
    }

    // =========================================================================
    // INLINE COMPONENTS
    // =========================================================================

    component DetailRow: RowLayout {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        height: 28; spacing: 0
        Text { Layout.preferredWidth: 110; text: label; font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; elide: Text.ElideRight }
        Text { Layout.fillWidth: true; text: value; font.pixelSize: Dimensions.fontBody; font.weight: Font.Medium; color: Theme.textPrimary; wrapMode: Text.WordWrap }
    }
}
