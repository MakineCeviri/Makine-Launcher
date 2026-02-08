import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

/**
 * SplashScreen.qml - Animated splash screen with update checker
 */
Item {
    id: root

    signal animationFinished()
    signal updateAvailable(string version, string downloadUrl)

    property string statusText: qsTr("Yükleniyor...")
    property bool updateChecked: false
    property bool gamesChecked: false
    property int patchedGamesCount: 0

    function start() {
        animProgress = 0.0
        statusText = qsTr("Başlatılıyor...")
        mainAnimation.start()

            checkForUpdates()
        checkPatchedGames()
    }

    // ═══════════════════════════════════════════════════════════════════════
    // UPDATE CHECKER
    // GitHub Releases API
    // ═══════════════════════════════════════════════════════════════════════

    readonly property string githubOwner: Dimensions.githubOwner
    readonly property string githubRepo: Dimensions.githubRepo
    readonly property string githubReleasesUrl: Dimensions.githubReleasesUrl

    readonly property string currentVersion: {
        var ver = Dimensions.appVersion
        return ver.replace(/[a-zA-Z]/g, "")
    }

    function checkForUpdates() {
        statusText = qsTr("Güncellemeler kontrol ediliyor...")

        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        processUpdateResponse(response)
                    } catch (e) {
                        DebugHelper.warn("SplashScreen", "Update check parse error: " + e)
                        onUpdateCheckComplete(false, "", "")
                    }
                } else if (xhr.status === 404) {
                    // No releases yet - not an error
                    onUpdateCheckComplete(false, "", "")
                } else {
                    DebugHelper.warn("SplashScreen", "Update check failed: " + xhr.status)
                    onUpdateCheckComplete(false, "", "")
                }
            }
        }

        xhr.open("GET", githubReleasesUrl)
        xhr.setRequestHeader("Accept", "application/vnd.github.v3+json")
        xhr.setRequestHeader("User-Agent", "MakineAI-UpdateChecker")

        updateCheckTimeoutTimer.start()

        try {
            xhr.send()
        } catch (e) {
            DebugHelper.warn("SplashScreen", "Update check network error: " + e)
            onUpdateCheckComplete(false, "", "")
        }
    }

    Timer {
        id: updateCheckTimeoutTimer
        interval: 10000
        onTriggered: {
            DebugHelper.warn("SplashScreen", "Update check timed out")
            onUpdateCheckComplete(false, "", "")
        }
    }

    function processUpdateResponse(data) {
        updateCheckTimeoutTimer.stop()

        var tagName = data.tag_name || ""
        if (tagName.startsWith("v")) {
            tagName = tagName.substring(1)
        }
        var latestVersion = tagName.replace(/[a-zA-Z]/g, "")
        var downloadUrl = ""
        var assets = data.assets || []
        for (var i = 0; i < assets.length; i++) {
            var name = (assets[i].name || "").toLowerCase()
            if (name.indexOf("windows") !== -1 || name.endsWith(".exe") || name.endsWith(".zip")) {
                downloadUrl = assets[i].browser_download_url || ""
                break
            }
        }

        var hasUpdate = compareVersions(latestVersion, currentVersion) > 0

        DebugHelper.log("SplashScreen", "Update check: current=" + currentVersion + ", latest=" + latestVersion + ", hasUpdate=" + hasUpdate)

        if (hasUpdate && downloadUrl !== "") {
            onUpdateCheckComplete(true, tagName, downloadUrl)
        } else {
            onUpdateCheckComplete(false, "", "")
        }
    }

    function compareVersions(v1, v2) {
        if (!v1 || !v2) return 0

        var parts1 = v1.split(".").map(function(x) { return parseInt(x) || 0 })
        var parts2 = v2.split(".").map(function(x) { return parseInt(x) || 0 })

        for (var i = 0; i < 3; i++) {
            var p1 = i < parts1.length ? parts1[i] : 0
            var p2 = i < parts2.length ? parts2[i] : 0
            if (p1 > p2) return 1
            if (p1 < p2) return -1
        }
        return 0
    }

    function onUpdateCheckComplete(hasUpdate, version, downloadUrl) {
        updateCheckTimeoutTimer.stop()
        root.updateChecked = true
        root.statusText = qsTr("Oyunlar taranıyor...")

        if (hasUpdate) {
            DebugHelper.info("SplashScreen", "Update available: " + version)
            root.updateAvailable(version, downloadUrl)
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PATCHED GAMES CHECK
    // ═══════════════════════════════════════════════════════════════════════

    function checkPatchedGames() {
        patchedGamesCheckTimer.start()
    }

    Timer {
        id: patchedGamesCheckTimer
        interval: Dimensions.fadeTransitionDuration
        onTriggered: {
            root.patchedGamesCount = GameService.patchedGamesCount
            root.gamesChecked = true
            root.statusText = qsTr("Hazır!")

            DebugHelper.log("SplashScreen", "Patched games check complete: " + root.patchedGamesCount + " games")
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MAIN ANIMATION
    // ═══════════════════════════════════════════════════════════════════════

    property real animProgress: 0.0

    NumberAnimation {
        id: mainAnimation
        target: root
        property: "animProgress"
        from: 0.0
        to: 1.0
        duration: 2200
        onFinished: root.animationFinished()
    }

    // ═══════════════════════════════════════════════════════════════════════
    // EASING FUNCTIONS
    // ═══════════════════════════════════════════════════════════════════════

    function easeOutBack(t) {
        var c1 = 1.70158
        var c3 = c1 + 1
        return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2)
    }

    function easeIn(t) { return t * t }
    function easeInOut(t) { return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2 }
    function easeOut(t) { return 1 - Math.pow(1 - t, 2) }

    // ═══════════════════════════════════════════════════════════════════════
    // ANIMATION VALUES
    // ═══════════════════════════════════════════════════════════════════════

    readonly property real logoScale: {
        if (animProgress <= 0.0) return 0.7
        if (animProgress >= 0.4) return 1.0
        return 0.7 + 0.3 * easeOutBack(animProgress / 0.4)
    }

    readonly property real logoOpacity: {
        if (animProgress <= 0.0) return 0.0
        if (animProgress >= 0.3) return 1.0
        return easeIn(animProgress / 0.3)
    }

    readonly property real textOpacity: {
        if (animProgress <= 0.3) return 0.0
        if (animProgress >= 0.5) return 1.0
        return easeIn((animProgress - 0.3) / 0.2)
    }

    readonly property real progressWidth: {
        if (animProgress <= 0.4) return 0.0
        if (animProgress >= 0.85) return 1.0
        return easeInOut((animProgress - 0.4) / 0.45)
    }

    readonly property real fadeOut: {
        if (animProgress <= 0.9) return 1.0
        if (animProgress >= 1.0) return 0.0
        return 1.0 - easeOut((animProgress - 0.9) / 0.1)
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: Theme.splashBackground
        opacity: root.fadeOut

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.35 - height / 2
            width: parent.width * 3
            height: width
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 0.84, 0.0, 0.06) }
                GradientStop { position: 0.4; color: Qt.rgba(1.0, 0.84, 0.0, 0.02) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.4 - height / 2
            width: parent.width * 1.5
            height: width
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 0.41, 0.71, 0.03) }
                GradientStop { position: 0.5; color: Qt.rgba(1.0, 0.41, 0.71, 0.01) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MAIN CONTENT
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.centerIn: parent
        width: 400
        height: 300
        opacity: root.fadeOut

        // ───────────────────────────────────────────────────────────────────
        // LOGO SECTION
        // ───────────────────────────────────────────────────────────────────

        Item {
            id: logoSection
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -40
            width: 350
            height: 140
            opacity: root.logoOpacity
            scale: root.logoScale

            Image {
                id: logoImage
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                width: 80
                height: 80
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }

            Text {
                id: titleText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: logoImage.bottom
                anchors.topMargin: Dimensions.marginMS
                text: "MakineAI"
                font.pixelSize: Dimensions.displayMedium
                font.weight: Font.Bold
                font.letterSpacing: -0.5
                color: Theme.splashGold
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: titleText.bottom
                anchors.topMargin: Dimensions.marginSM
                width: badgeRow.width + 28
                height: 26
                radius: Dimensions.radiusStandard

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(1.0, 0.84, 0.0, 0.2) }
                    GradientStop { position: 1.0; color: Qt.rgba(1.0, 0.41, 0.71, 0.2) }
                }

                border.color: Qt.rgba(1.0, 0.84, 0.0, 0.3)
                border.width: 1

                Row {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 5

                    Text {
                        text: "✨"
                        font.pixelSize: Dimensions.fontMD
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: qsTr("Türkçe Yama")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.3
                        color: Theme.splashGold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // ───────────────────────────────────────────────────────────────────
        // PROGRESS BAR
        // ───────────────────────────────────────────────────────────────────

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: logoSection.bottom
            anchors.topMargin: Dimensions.marginXXL
            width: 200
            height: 50
            opacity: root.textOpacity

            Rectangle {
                id: progressTrack
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: 200
                height: 4
                radius: Dimensions.radiusStandard
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            Item {
                anchors.left: progressTrack.left
                anchors.top: progressTrack.top
                width: progressTrack.width * root.progressWidth
                height: 4
                clip: true
                visible: root.progressWidth > 0.01

                Rectangle {
                    visible: root.progressWidth > 0.05
                    anchors.centerIn: parent
                    width: parent.width + 8
                    height: 12
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1.0, 0.41, 0.71, 0.4)

                    // Shimmer glow pulse
                    opacity: 0.6 + 0.4 * Math.sin(shimmerPhase * Math.PI * 2)
                    property real shimmerPhase: 0
                    NumberAnimation on shimmerPhase {
                        from: 0; to: 1
                        duration: 1200
                        loops: Animation.Infinite
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: progressTrack.width
                    height: 4
                    radius: Dimensions.radiusStandard
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.splashGold }
                        GradientStop { position: 0.5; color: Theme.splashOrange }
                        GradientStop { position: 1.0; color: Theme.splashPink }
                    }
                }
            }

            Text {
                id: statusLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: progressTrack.bottom
                anchors.topMargin: 14
                text: root.statusText
                font.pixelSize: Dimensions.fontXS
                font.weight: Font.Medium
                font.letterSpacing: 1.5
                color: Qt.rgba(1, 1, 1, 0.35)

                // Crossfade on text change
                property string pendingText: root.statusText
                onPendingTextChanged: {
                    if (text !== pendingText) statusFadeAnim.restart()
                }

                SequentialAnimation {
                    id: statusFadeAnim
                    NumberAnimation { target: statusLabel; property: "opacity"; to: 0; duration: 120 }
                    ScriptAction { script: statusLabel.text = statusLabel.pendingText }
                    NumberAnimation { target: statusLabel; property: "opacity"; to: 1; duration: 180 }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // FOOTER
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        width: 200
        height: 50
        opacity: root.fadeOut * root.textOpacity

        Rectangle {
            id: versionBadge
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: versionText.width + 20
            height: versionText.height + 8
            radius: Dimensions.radiusStandard
            color: Qt.rgba(1, 1, 1, 0.04)
            scale: root.textOpacity  // Scales in with text
            Behavior on scale { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

            Text {
                id: versionText
                anchors.centerIn: parent
                text: Dimensions.appVersionFull
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.Medium
                color: Qt.rgba(1, 1, 1, 0.45)
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: versionBadge.bottom
            anchors.topMargin: Dimensions.spacingSM
            text: qsTr("Makine Çeviri Topluluğu")
            font.pixelSize: Dimensions.fontCaption
            font.letterSpacing: 0.3
            color: Qt.rgba(1, 1, 1, 0.3)
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // AUTO START
    // ═══════════════════════════════════════════════════════════════════════

    Component.onCompleted: start()
}
