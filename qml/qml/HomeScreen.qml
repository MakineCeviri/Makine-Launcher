import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
import "dialogs"
import "components"

/**
 * HomeScreen.qml - Main home view with game status, announcements, and projects
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    // Live-tunable values for Design Inspector
    property real contentMargin: 16
    property real diCardMargin: 8
    property real diCardSpacing: 8
    property real diTopRowHeight: 210
    property real diTopRowGap: 16
    property real diGamesSectionGap: 8
    property real diCardGap: 32
    property real diSeparatorTopMargin: 4
    property real diSeparatorBottomMargin: 8

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal scanRequested()

    // ===== UPDATE CHECKER =====
    property bool updateAvailable: false
    property string latestVersion: ""
    property string downloadUrl: ""
    property string notificationMessage: ""
    property string notificationType: "info"  // info, warning, error, update

    readonly property string githubOwner: Dimensions.githubOwner
    readonly property string githubRepo: Dimensions.githubRepo
    readonly property string currentVersion: Dimensions.appVersion.replace(/[a-zA-Z]/g, "")

    function checkForUpdates() {
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        var tagName = response.tag_name || ""
                        var remoteVersion = tagName.replace(/^v/, "").replace(/[a-zA-Z-]/g, "")

                        if (compareVersions(remoteVersion, currentVersion) > 0) {
                            updateAvailable = true
                            latestVersion = tagName
                            downloadUrl = response.html_url || ""
                            notificationMessage = "Yeni sürüm mevcut: " + tagName
                            notificationType = "update"
                        }
                    } catch (e) {
                        DebugHelper.warn("HomeScreen", "Update check parse error: " + e)
                    }
                }
            }
        }

        var url = Dimensions.githubReleasesUrl
        xhr.open("GET", url)
        xhr.setRequestHeader("Accept", "application/vnd.github.v3+json")
        xhr.setRequestHeader("User-Agent", "MakineAI-UpdateChecker")
        try { xhr.send() } catch (e) { DebugHelper.warn("HomeScreen", "Update check error: " + e) }
    }

    function compareVersions(v1, v2) {
        var parts1 = v1.split(".").map(function(x) { return parseInt(x) || 0 })
        var parts2 = v2.split(".").map(function(x) { return parseInt(x) || 0 })
        for (var i = 0; i < Math.max(parts1.length, parts2.length); i++) {
            var p1 = parts1[i] || 0
            var p2 = parts2[i] || 0
            if (p1 > p2) return 1
            if (p1 < p2) return -1
        }
        return 0
    }

    function showNotification(message, type) {
        notificationMessage = message
        notificationType = type || "info"
    }

    function hideNotification() {
        notificationMessage = ""
    }

    Component.onCompleted: {
        GameService.scanAllLibraries()
        checkForUpdates()
    }

    function showHomePage() {
        if (!aiActive) {
            pageStack.currentIndex = 0
            // Replay entry animations when returning to home
            replayEntryAnimations()
        }
    }

    // Replay entry animations when page becomes visible again
    function replayEntryAnimations() {
        // Reset and replay top row animation
        topRowLayout.opacity = 0
        topRowEntryAnim.restart()

        // Reset and replay games section animation
        gamesSectionLayout.opacity = 0
        gamesSectionEntryAnim.restart()

        // Trigger game cards to replay their animations
        animationTrigger++
    }

    function showProjectsPage() {
        if (!aiActive) {
            pageStack.currentIndex = 1
        }
    }

    function setAIActive(active) {
        aiActive = active
        if (active) {
            pageStack.currentIndex = 2
        } else {
            pageStack.currentIndex = 0
        }
    }

    property bool aiActive: false

    // Animation trigger - increment to replay all entry animations
    property int animationTrigger: 0

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        // ===== PAGE STACK - Native Qt QStackedWidget =====
        StackLayout {
            id: pageStack
            anchors.fill: parent
            currentIndex: 0

            // ===== HOME PAGE (Index 0) =====
            Item {
                id: homePage

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: root.contentMargin
                    spacing: Dimensions.marginMD

                        // ===== NOTIFICATION BANNER =====
                        Rectangle {
                            id: notificationBanner
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            Layout.preferredHeight: notificationMessage ? 44 : 0
                            radius: Dimensions.radiusStandard
                            visible: notificationMessage !== ""
                            clip: true

                            color: {
                                switch(notificationType) {
                                    case "update": return Qt.rgba(0.2, 0.6, 1, 0.15)
                                    case "warning": return Qt.rgba(1, 0.7, 0, 0.15)
                                    case "error": return Qt.rgba(1, 0.3, 0.3, 0.15)
                                    default: return Qt.rgba(0.5, 0.5, 0.5, 0.1)
                                }
                            }

                            border.color: {
                                switch(notificationType) {
                                    case "update": return Qt.rgba(0.2, 0.6, 1, 0.4)
                                    case "warning": return Qt.rgba(1, 0.7, 0, 0.4)
                                    case "error": return Qt.rgba(1, 0.3, 0.3, 0.4)
                                    default: return Qt.rgba(1, 1, 1, 0.1)
                                }
                            }
                            border.width: 1

                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 12

                                Text {
                                    text: {
                                        switch(notificationType) {
                                            case "update": return "⬆"
                                            case "warning": return "⚠"
                                            case "error": return "✕"
                                            default: return "ℹ"
                                        }
                                    }
                                    font.pixelSize: 16
                                    color: {
                                        switch(notificationType) {
                                            case "update": return Theme.notificationUpdate
                                            case "warning": return Theme.notificationWarning
                                            case "error": return Theme.notificationError
                                            default: return Theme.textSecondary
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: notificationMessage
                                    font.pixelSize: 13
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: notificationType === "update" && downloadUrl
                                    width: updateBtnText.width + 16
                                    height: 28
                                    radius: Dimensions.radiusStandard
                                    color: updateBtnMouse.containsMouse ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(0.2, 0.6, 1, 0.2)

                                    Text {
                                        id: updateBtnText
                                        anchors.centerIn: parent
                                        text: "İndir"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: Theme.notificationUpdate
                                    }

                                    MouseArea {
                                        id: updateBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally(downloadUrl)
                                    }
                                }

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 12
                                    color: closeBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                    }

                                    MouseArea {
                                        id: closeBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: hideNotification()
                                    }
                                }
                            }
                        }

                        // ===== TOP ROW =====
                        RowLayout {
                            id: topRowLayout
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.diTopRowHeight
                            Layout.maximumHeight: root.diTopRowHeight
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.diTopRowGap

                            opacity: 0
                            Component.onCompleted: topRowEntryAnim.start()

                            NumberAnimation {
                                id: topRowEntryAnim
                                target: topRowLayout
                                property: "opacity"
                                from: 0; to: 1
                                duration: 400
                                easing.type: Easing.OutCubic
                            }

                            // ============================================================
                            // GAME STATUS CARD
                            // ============================================================
                            Rectangle {
                                id: gameStatusCard
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.diTopRowHeight
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    id: gameStatusCardLayout
                                    anchors.fill: parent
                                    anchors.margins: root.diCardMargin
                                    spacing: root.diCardSpacing

                                    Row {
                                        Layout.fillWidth: true
                                        height: 44
                                        spacing: 14

                                        // Turkish flag with geometric proportions
                                        Rectangle {
                                            width: 44
                                            height: 44
                                            radius: Dimensions.radiusStandard
                                            color: Theme.turkishRed
                                            clip: true

                                            Rectangle {
                                                x: 7; y: 11
                                                width: 22; height: 22
                                                radius: 11
                                                color: "white"
                                            }

                                            Rectangle {
                                                x: 12; y: 13
                                                width: 18; height: 18
                                                radius: 9
                                                color: Theme.turkishRed
                                            }

                                            Canvas {
                                                x: 25; y: 16
                                                width: 12; height: 12
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    var cx = 6, cy = 6
                                                    var R = 5.5
                                                    var r = R * 0.382
                                                    ctx.beginPath()
                                                    for (var i = 0; i < 5; i++) {
                                                        var oa = i * 72 * Math.PI / 180
                                                        var ia = (i * 72 + 36) * Math.PI / 180
                                                        if (i === 0) ctx.moveTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                                                        else ctx.lineTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                                                        ctx.lineTo(cx - r * Math.cos(ia), cy - r * Math.sin(ia))
                                                    }
                                                    ctx.closePath()
                                                    ctx.fillStyle = "white"
                                                    ctx.fill()
                                                }
                                            }
                                        }

                                        Column {
                                            anchors.verticalCenter: parent.children[0].verticalCenter
                                            spacing: 3

                                            Label {
                                                text: "Türkçe Yama"
                                                font.pixelSize: 16
                                                font.weight: Font.DemiBold
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                text: "Oyun çeviri durumu"
                                                font.pixelSize: 11
                                                color: Theme.textMuted
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 60
                                        radius: Dimensions.radiusStandard
                                        color: Qt.rgba(1, 1, 1, 0.02)
                                        border.color: Qt.rgba(1, 1, 1, 0.06)
                                        border.width: 1

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 10

                                            Row {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                spacing: 8

                                                Repeater {
                                                    model: 3

                                                    Rectangle {
                                                        required property int index
                                                        width: 8
                                                        height: 8
                                                        radius: 4
                                                        color: Theme.primary

                                                        SequentialAnimation on opacity {
                                                            loops: Animation.Infinite
                                                            running: root.animationsEnabled
                                                            PauseAnimation { duration: index * 250 }
                                                            NumberAnimation { from: 0.25; to: 1.0; duration: 400; easing.type: Easing.InOutSine }
                                                            NumberAnimation { from: 1.0; to: 0.25; duration: 400; easing.type: Easing.InOutSine }
                                                            PauseAnimation { duration: (2 - index) * 250 }
                                                        }

                                                        SequentialAnimation on scale {
                                                            loops: Animation.Infinite
                                                            running: root.animationsEnabled
                                                            PauseAnimation { duration: index * 250 }
                                                            NumberAnimation { from: 1.0; to: 1.3; duration: 400; easing.type: Easing.InOutSine }
                                                            NumberAnimation { from: 1.3; to: 1.0; duration: 400; easing.type: Easing.InOutSine }
                                                            PauseAnimation { duration: (2 - index) * 250 }
                                                        }
                                                    }
                                                }
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: "Oyun Bekleniyor"
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: "Desteklenen bir oyun çalıştırın veya manuel seçin"
                                                font.pixelSize: 11
                                                color: Theme.textMuted
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: Dimensions.radiusStandard
                                        color: manualBtnMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.1)
                                            : Qt.rgba(1, 1, 1, 0.05)
                                        border.color: Qt.rgba(1, 1, 1, 0.08)
                                        border.width: 1

                                        Behavior on color {
                                            ColorAnimation { duration: 150 }
                                        }

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 8

                                            Image {
                                                width: 14; height: 14
                                                source: "qrc:/qt/qml/MakineAI/resources/icons/window-search.svg"
                                                sourceSize: Qt.size(14, 14)
                                                anchors.verticalCenter: parent.verticalCenter
                                                opacity: manualBtnMouse.containsMouse ? 1.0 : 0.6
                                                Behavior on opacity { NumberAnimation { duration: 150 } }
                                            }

                                            Label {
                                                text: "Manuel Oyun Seç"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: manualBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        MouseArea {
                                            id: manualBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: gameDetectorDialog.open()
                                        }
                                    }
                                }
                            }

                            // ============================================================
                            // ANNOUNCEMENT CARD
                            // ============================================================
                            Rectangle {
                                id: announcementCard
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.diTopRowHeight
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: root.diCardMargin
                                    spacing: root.diCardSpacing

                                    Column {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 44
                                        spacing: 3

                                        Label {
                                            text: "Duyurular"
                                            font.pixelSize: 16
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }
                                        Label {
                                            text: "18 Ocak 2026"
                                            font.pixelSize: 11
                                            color: Theme.textMuted
                                        }
                                    }

                                    Rectangle {
                                        id: announcementContentBox
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 60
                                        radius: Dimensions.radiusStandard
                                        color: announcementContentMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.06)
                                            : Qt.rgba(1, 1, 1, 0.02)
                                        border.color: announcementContentMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.12)
                                            : Qt.rgba(1, 1, 1, 0.06)
                                        border.width: 1

                                        property bool expanded: false

                                        Behavior on color { ColorAnimation { duration: 150 } }
                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        Flickable {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            contentHeight: announcementContentCol.height
                                            clip: true
                                            interactive: announcementContentBox.expanded
                                            boundsBehavior: Flickable.StopAtBounds

                                            Column {
                                                id: announcementContentCol
                                                width: parent.width
                                                spacing: 8

                                                Item { width: 1; height: 4 }

                                                Label {
                                                    width: parent.width
                                                    text: "Desteklenmeyen Oyunlar Hakkında"
                                                    font.pixelSize: 13
                                                    font.weight: Font.DemiBold
                                                    color: Theme.textPrimary
                                                    horizontalAlignment: Text.AlignHCenter
                                                }

                                                Label {
                                                    width: parent.width
                                                    text: announcementContentBox.expanded
                                                        ? "Resmi olarak desteklenmeyen oyunlarda çeviri hataları veya performans sorunları yaşanabilir.\n\nMakineAI, her oyun için en iyi çeviri deneyimini sunmayı hedefler. Ancak bazı oyunlar, kullandıkları özel metin sistemleri veya şifreleme yöntemleri nedeniyle henüz tam olarak desteklenememektedir.\n\nDesteklenmeyen bir oyunda çeviri başlattığınızda:\n• Bazı metinler eksik veya hatalı görünebilir\n• Oyun performansında düşüş yaşanabilir\n• Nadir durumlarda oyun kararsız hale gelebilir\n\nBu tür sorunlarla karşılaşırsanız, orijinal dil dosyalarını geri yüklemek için yedekleme özelliğini kullanabilirsiniz. Destek ekibimize bildirdiğiniz oyunlar öncelikli olarak değerlendirilecektir."
                                                        : "Resmi olarak desteklenmeyen oyunlarda çeviri hataları veya performans sorunları yaşanabilir."
                                                    font.pixelSize: 11
                                                    color: Theme.textSecondary
                                                    wrapMode: Text.WordWrap
                                                    horizontalAlignment: announcementContentBox.expanded ? Text.AlignLeft : Text.AlignHCenter
                                                }

                                                // "Devamını oku" / "Kapat" indicator
                                                Label {
                                                    width: parent.width
                                                    text: announcementContentBox.expanded ? "Kapat" : "Devamını oku →"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Medium
                                                    color: Theme.primary
                                                    horizontalAlignment: Text.AlignHCenter
                                                    opacity: announcementContentMouse.containsMouse || announcementContentBox.expanded ? 1.0 : 0.0
                                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: announcementContentMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: announcementContentBox.expanded = !announcementContentBox.expanded
                                        }
                                    }

                                    Rectangle {
                                        id: securityBtn
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: Dimensions.radiusStandard
                                        color: securityMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.1)
                                            : Qt.rgba(1, 1, 1, 0.05)
                                        border.color: securityMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.15)
                                            : Qt.rgba(1, 1, 1, 0.08)
                                        border.width: 1

                                        Behavior on color { ColorAnimation { duration: 150 } }
                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Item {
                                                width: 13; height: 13
                                                anchors.verticalCenter: parent.verticalCenter

                                                Image {
                                                    anchors.fill: parent
                                                    source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
                                                    sourceSize: Qt.size(13, 13)
                                                    opacity: securityMouse.containsMouse ? 0 : 0.6
                                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                                }
                                                Image {
                                                    anchors.fill: parent
                                                    source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check-active.svg"
                                                    sourceSize: Qt.size(13, 13)
                                                    opacity: securityMouse.containsMouse ? 0.9 : 0
                                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                                }
                                            }

                                            Label {
                                                text: "Güvenliğiniz için yalnızca"
                                                font.pixelSize: 10
                                                color: securityMouse.containsMouse ? Theme.textSecondary : Theme.textMuted
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                            }

                                            Label {
                                                text: "makineai.com"
                                                font.pixelSize: 10
                                                font.weight: Font.Medium
                                                font.underline: true
                                                color: securityMouse.containsMouse ? Theme.primary : Theme.textSecondary
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                            }

                                            Label {
                                                text: "üzerinden indirin"
                                                font.pixelSize: 10
                                                color: securityMouse.containsMouse ? Theme.textSecondary : Theme.textMuted
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                            }
                                        }

                                        MouseArea {
                                            id: securityMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: Qt.openUrlExternally("https://makineai.com")
                                        }
                                    }
                                }
                            }
                        }

                        // ===== GAMES SECTION =====
                        ColumnLayout {
                            id: gamesSectionLayout
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.diGamesSectionGap

                            opacity: 0
                            Component.onCompleted: gamesSectionEntryAnim.start()

                            SequentialAnimation {
                                id: gamesSectionEntryAnim
                                PauseAnimation { duration: 200 }
                                NumberAnimation {
                                    target: gamesSectionLayout
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: 400
                                    easing.type: Easing.OutCubic
                                }
                            }

                            RowLayout {
                                spacing: 12

                                Label {
                                    text: GameService.isScanning ? "Oyunlar Taraniyor..." : "Desteklenen Oyunlar"
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }

                                Item { Layout.fillWidth: true }

                                Row {
                                    visible: GameService.isScanning
                                    spacing: 8

                                    BusyIndicator {
                                        width: 20
                                        height: 20
                                        running: GameService.isScanning
                                    }

                                    Label {
                                        text: GameService.scanStatus || "Taraniyor..."
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Rectangle {
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: gamesCountLabel.width + 14
                                    radius: Dimensions.badgeRadius
                                    color: Theme.withAlpha(Theme.primary, 0.12)

                                    Label {
                                        id: gamesCountLabel
                                        anchors.centerIn: parent
                                        text: GameService.gameCount + " oyun"
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: Theme.primary
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: root.diSeparatorTopMargin
                                Layout.bottomMargin: root.diSeparatorBottomMargin
                                color: Qt.rgba(1, 1, 1, 0.15)
                            }

                            Flow {
                                id: skeletonFlow
                                Layout.fillWidth: true
                                spacing: Dimensions.cardGap
                                visible: GameService.isScanning && GameService.gameCount === 0

                                Repeater {
                                    model: 7

                                    GameCardSkeleton {
                                        animationsEnabled: root.animationsEnabled
                                        animationDelay: index * 100
                                    }
                                }
                            }

                            Row {
                                id: gamesRow
                                Layout.alignment: Qt.AlignHCenter
                                spacing: root.diCardGap
                                visible: !skeletonFlow.visible

                                readonly property int availableWidth: gamesSectionLayout.width
                                readonly property int cardTotal: Dimensions.cardWidth + root.diCardGap
                                readonly property int maxCards: Math.max(1, Math.floor((availableWidth - Dimensions.cardWidth) / cardTotal))

                                Repeater {
                                    id: gamesRepeater
                                    model: GameService.games.slice(0, gamesRow.maxCards)

                                    GameCard {
                                        id: gameCardDelegate
                                        required property var modelData
                                        required property int index
                                        gameName: modelData.name || ""
                                        imageUrl: modelData.headerImageUrl || ""
                                        verified: modelData.isVerified || false
                                        translated: modelData.hasTranslation || false

                                        // Staggered fade-in (no transform - fixes click issues)
                                        opacity: 0
                                        Component.onCompleted: entryAnimation.start()

                                        // Re-trigger animation when animationTrigger changes
                                        Connections {
                                            target: root
                                            function onAnimationTriggerChanged() {
                                                gameCardDelegate.opacity = 0
                                                entryAnimation.restart()
                                            }
                                        }

                                        SequentialAnimation {
                                            id: entryAnimation
                                            PauseAnimation { duration: index * 40 }
                                            NumberAnimation {
                                                target: gameCardDelegate
                                                property: "opacity"
                                                from: 0; to: 1
                                                duration: 300
                                                easing.type: Easing.OutCubic
                                            }
                                        }

                                        onClicked: {
                                            root.gameSelected(
                                                modelData.id || "",
                                                modelData.name || "",
                                                modelData.installPath || "",
                                                modelData.engine || ""
                                            )
                                        }
                                    }
                                }

                                ViewAllCard {
                                    id: viewAllCardItem
                                    remainingCount: Math.max(0, GameService.gameCount - gamesRepeater.count)

                                    onClicked: {
                                        allGamesDialog.games = GameService.games
                                        allGamesDialog.open()
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1
                                visible: GameService.gameCount === 0 && !GameService.isScanning

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 12

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "\uD83C\uDFAE"
                                        font.pixelSize: 32
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "Henüz oyun bulunamadı"
                                        font.pixelSize: 14
                                        color: Theme.textSecondary
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "Steam, Epic veya GOG kütüphane klasörlerinizi kontrol edin"
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== PROJECTS PAGE (Index 1) =====
            Item {
                id: projectsPage

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
                            color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                        }
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 32

                        Item { Layout.preferredHeight: Dimensions.marginXXL }

                        RowLayout {
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: 16

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: Dimensions.radiusStandard
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Theme.withAlpha(Theme.gold, 0.2) }
                                    GradientStop { position: 1.0; color: Theme.withAlpha(Theme.olive, 0.2) }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDE80"
                                    font.pixelSize: 24
                                }
                            }

                            ColumnLayout {
                                spacing: 4

                                Label {
                                    text: "Projelerimiz"
                                    font.pixelSize: 24
                                    font.weight: Font.Bold
                                    color: Theme.textPrimary
                                }

                                Label {
                                    text: "Makine Çeviri topluluğunun aktif projeleri"
                                    font.pixelSize: 14
                                    color: Theme.textSecondary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        CedraInteractiveCard {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            animationsEnabled: root.animationsEnabled
                        }

                        ProjectCategory {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            title: "Oyun Projeleri"
                            subtitle: "CEDRA Interactive bünyesinde geliştirilen oyunlar"
                            categoryColor: Theme.destructive
                        }

                        ProjectCategory {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            title: "Çeviri Projeleri"
                            subtitle: "Topluluk tarafından yürütülen çeviri projeleri"
                            categoryColor: Theme.statusCyan
                        }

                        Item { Layout.preferredHeight: Dimensions.marginXXL }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 120
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.35) }
                    }
                    // Don't block mouse events
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        propagateComposedEvents: true
                    }
                }
            }

            // ===== AI ACTIVE PAGE (Index 2) =====
            Item {
                id: aiActivePage

                Rectangle {
                    anchors.fill: parent
                    color: Theme.bgPrimary

                    Rectangle {
                        anchors.centerIn: parent
                        width: 400
                        height: waitingCardContent.height + 64
                        radius: Dimensions.radiusStandard
                        color: Theme.titlebarBg
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        ColumnLayout {
                            id: waitingCardContent
                            anchors.centerIn: parent
                            spacing: 20
                            width: parent.width - 64

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 64
                                radius: 32
                                color: Qt.rgba(1, 1, 1, 0.05)

                                Label {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDD0D"
                                    font.pixelSize: 28
                                }
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Oyun Aktif Değil"
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                text: "Desteklenen bir oyunu başlattığınızda\notomatik olarak tespit edilecektir."
                                font.pixelSize: 13
                                color: Theme.textMuted
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                lineHeight: 1.5
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                radius: Dimensions.radiusStandard
                                color: Qt.rgba(1, 1, 1, 0.03)

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Label {
                                        text: "\uD83C\uDFAE"
                                        font.pixelSize: 16
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: GameService.gameCount + " desteklenen oyun"
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== GLASS CARD COMPONENT =====
    component GlassCard: Rectangle {
        radius: Dimensions.radiusStandard
        color: Qt.rgba(1, 1, 1, 0.03)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    // ===== GAME CARD COMPONENT - Modern minimal hover =====
    component GameCard: Item {
        id: gameCardRoot
        property string gameName: ""
        property string imageUrl: ""
        property bool verified: false
        property bool translated: false

        signal clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: cardMouse.containsMouse

        transform: [
            Translate { y: gameCardRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: gameCardRoot.width / 2; origin.y: gameCardRoot.height / 2
                xScale: gameCardRoot.isHovered ? 1.02 : 1.0; yScale: gameCardRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            }
        ]

        Rectangle {
            id: cardContent
            anchors.fill: parent
            radius: Dimensions.cardBorderRadius
            clip: true
            color: Theme.surface

            property real borderPhase: 0
            NumberAnimation on borderPhase {
                from: 0; to: 1
                duration: 8000
                loops: Animation.Infinite
                running: gameCardRoot.isHovered && root.animationsEnabled
            }

            Canvas {
                anchors.fill: parent
                z: 10
                property real phase: cardContent.borderPhase
                onPhaseChanged: if (hov) requestPaint()
                property bool hov: gameCardRoot.isHovered
                onHovChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var angle = phase * Math.PI * 2
                    var cx = width / 2, cy = height / 2
                    var len = Math.max(width, height) * 0.7
                    var x1 = cx + Math.cos(angle) * len
                    var y1 = cy + Math.sin(angle) * len
                    var x2 = cx - Math.cos(angle) * len
                    var y2 = cy - Math.sin(angle) * len

                    var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                    var colors = Theme.brandGradient
                    for (var i = 0; i < colors.length; i++)
                        grad.addColorStop(i / (colors.length - 1), colors[i])

                    var r = Dimensions.cardBorderRadius
                    var bw = 1.5
                    var px = bw / 2, py = bw / 2
                    var w = width - bw, h = height - bw

                    ctx.beginPath()
                    ctx.moveTo(px + r, py)
                    ctx.lineTo(px + w - r, py)
                    ctx.arcTo(px + w, py, px + w, py + r, r)
                    ctx.lineTo(px + w, py + h - r)
                    ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                    ctx.lineTo(px + r, py + h)
                    ctx.arcTo(px, py + h, px, py + h - r, r)
                    ctx.lineTo(px, py + r)
                    ctx.arcTo(px, py, px + r, py, r)
                    ctx.closePath()

                    ctx.strokeStyle = grad
                    ctx.lineWidth = bw
                    ctx.globalAlpha = hov ? 0.8 : 0.0
                    ctx.stroke()
                }
            }

            Item {
                id: imageMask
                anchors.fill: parent
                visible: false
                layer.enabled: true
                Rectangle { anchors.fill: parent; radius: Dimensions.cardBorderRadius; color: "white" }
            }

            Image {
                id: gameImage
                anchors.fill: parent
                source: imageUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                visible: false
            }

            MultiEffect {
                anchors.fill: gameImage
                source: gameImage
                maskEnabled: true
                maskSource: imageMask
                visible: gameImage.status === Image.Ready
                brightness: gameCardRoot.isHovered ? 0.06 : 0
                Behavior on brightness { NumberAnimation { duration: 250 } }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                visible: gameImage.status !== Image.Ready
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }

            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: parent.height * 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.9) }
                }
            }

            Rectangle {
                visible: gameCardRoot.translated || gameCardRoot.verified
                anchors.top: parent.top; anchors.right: parent.right
                anchors.topMargin: 6; anchors.rightMargin: 6
                width: badgeRow.width + 8; height: 20
                radius: Dimensions.badgeRadius
                color: Qt.rgba(0, 0, 0, 0.7)

                Row {
                    id: badgeRow
                    anchors.centerIn: parent; spacing: 4
                    Rectangle {
                        visible: gameCardRoot.translated
                        width: 20; height: 14; radius: Dimensions.badgeRadius
                        color: Theme.turkishRed; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "TR"; font.pixelSize: 8; font.weight: Font.Bold; color: "white" }
                    }
                    Rectangle {
                        visible: gameCardRoot.verified
                        width: 14; height: 14; radius: 7
                        color: Theme.primary; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "✓"; font.pixelSize: 9; font.weight: Font.Bold; color: "white" }
                    }
                }
            }

            Label {
                id: gameNameLabel
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.margins: 10
                text: gameName; font.pixelSize: 10; font.weight: Font.DemiBold
                color: "white"; elide: Text.ElideRight

                ToolTip {
                    visible: gameCardRoot.isHovered && gameNameLabel.truncated
                    delay: 300; text: gameName; font.pixelSize: 12
                    background: Rectangle { color: Qt.rgba(0.08, 0.08, 0.08, 0.96); radius: Dimensions.radiusStandard; border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1 }
                }
            }
        }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: gameCardRoot.clicked()
        }
    }

    // ===== VIEW ALL CARD - Modern, GameCard ile uyumlu =====
    component ViewAllCard: Item {
        id: viewAllRoot
        property int remainingCount: 0

        signal clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: viewAllMouse.containsMouse

        transform: [
            Translate { y: viewAllRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: viewAllRoot.width / 2; origin.y: viewAllRoot.height / 2
                xScale: viewAllRoot.isHovered ? 1.02 : 1.0; yScale: viewAllRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            }
        ]

        AnimatedGradientGlow {
            anchors.centerIn: viewAllContent
            width: viewAllContent.width + 40
            height: viewAllContent.height + 40
            active: viewAllRoot.isHovered
            animationsEnabled: root.animationsEnabled
            z: -2
        }

        Rectangle {
            id: viewAllContent
            anchors.fill: parent
            radius: Dimensions.cardBorderRadius
            color: Theme.bgPrimary

            property real borderPhase: 0
            NumberAnimation on borderPhase {
                from: 0; to: 1
                duration: 8000
                loops: Animation.Infinite
                running: viewAllRoot.visible && root.animationsEnabled
            }

            Canvas {
                anchors.fill: parent
                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (viewAllRoot.isHovered) requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var angle = phase * Math.PI * 2
                    var cx = width / 2, cy = height / 2
                    var len = Math.max(width, height) * 0.7
                    var x1 = cx + Math.cos(angle) * len
                    var y1 = cy + Math.sin(angle) * len
                    var x2 = cx - Math.cos(angle) * len
                    var y2 = cy - Math.sin(angle) * len

                    var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                    var colors = Theme.brandGradient
                    for (var i = 0; i < colors.length; i++)
                        grad.addColorStop(i / (colors.length - 1), colors[i])

                    var r = Dimensions.cardBorderRadius
                    var bw = 1.5
                    var px = bw / 2, py = bw / 2
                    var w = width - bw, h = height - bw

                    ctx.beginPath()
                    ctx.moveTo(px + r, py)
                    ctx.lineTo(px + w - r, py)
                    ctx.arcTo(px + w, py, px + w, py + r, r)
                    ctx.lineTo(px + w, py + h - r)
                    ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                    ctx.lineTo(px + r, py + h)
                    ctx.arcTo(px, py + h, px, py + h - r, r)
                    ctx.lineTo(px, py + r)
                    ctx.arcTo(px, py, px + r, py, r)
                    ctx.closePath()

                    ctx.strokeStyle = grad
                    ctx.lineWidth = bw
                    ctx.globalAlpha = viewAllRoot.isHovered ? 0.8 : 0.4
                    ctx.stroke()
                }

                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
            }

            Canvas {
                id: viewAllCanvas
                anchors.fill: parent

                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (hov) requestPaint()
                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
                property int count: viewAllRoot.remainingCount
                onCountChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width / 2;
                    var angle = phase * Math.PI * 2;
                    var len = width * 0.6;
                    var x1 = cx + Math.cos(angle) * len;
                    var x2 = cx - Math.cos(angle) * len;
                    var colors = Theme.brandGradient;

                    var numGrad = ctx.createLinearGradient(x1, 0, x2, 0);
                    for (var i = 0; i < colors.length; i++)
                        numGrad.addColorStop(i / (colors.length - 1), colors[i]);

                    // Sideways "+N" number - centered
                    ctx.save();
                    ctx.translate(cx + 5, height / 2 - 4);
                    ctx.rotate(-Math.PI / 2);
                    ctx.font = "bold 46px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    ctx.globalAlpha = hov ? 1.0 : 0.6;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.5)";
                    ctx.fillText("+" + count, 0, 0);
                    ctx.restore();

                    // Gradient separator line - exact center
                    var lineY = height - 34;
                    var lineW = 36;
                    var lineX = Math.round(cx - lineW / 2);
                    ctx.globalAlpha = hov ? 0.5 : 0.15;
                    ctx.fillStyle = numGrad;
                    ctx.fillRect(lineX, lineY, lineW, 1.5);

                    // "Tümünü Gör" - exact center
                    ctx.font = "500 11px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "top";
                    ctx.globalAlpha = hov ? 0.9 : 0.4;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.4)";
                    var label = hov ? "Tümünü Gör →" : "Tümünü Gör";
                    ctx.fillText(label, Math.round(cx), lineY + 6);
                }
            }
        }

        MouseArea {
            id: viewAllMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewAllRoot.clicked()
        }
    }

    // ===== PROJECT CATEGORY COMPONENT =====
    component ProjectCategory: Rectangle {
        property string title: ""
        property string subtitle: ""
        property color categoryColor: Theme.primary

        radius: Dimensions.radiusStandard
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        implicitHeight: catContent.height

        ColumnLayout {
            id: catContent
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: Dimensions.radiusStandard
                    color: Theme.withAlpha(categoryColor, 0.15)

                    Label {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAE"
                        font.pixelSize: 22
                    }
                }

                ColumnLayout {
                    spacing: 2

                    Label {
                        text: title
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: "white"
                    }

                    Label {
                        text: subtitle
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            Flow {
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 16

                ProjectCard {
                    title: "Cyberless: Online"
                    description: "Çok oyunculu cyberpunk aksiyon oyunu"
                    status: "Tamamlandı"
                    statusColor: Theme.statusOnline
                }

                ProjectCard {
                    title: "Endurance"
                    description: "Hayatta kalma, kaynak yönetimi, korku ve gerilim oyunu"
                    status: "Geliştiriliyor"
                    statusColor: Theme.statusPurple
                }
            }
        }
    }

    // ===== ALL GAMES DIALOG =====
    AllGamesDialog {
        id: allGamesDialog
        parent: Overlay.overlay

        onGameSelected: function(gameId) {
            var gameData = GameService.getGameById(gameId)
            if (gameData) {
                root.gameSelected(gameId, gameData.name, gameData.installPath, gameData.engine || "Unknown")
            }
            close()
        }
    }

    // ===== GAME DETECTOR DIALOG =====
    GameDetectorDialog {
        id: gameDetectorDialog
        parent: Overlay.overlay

        onGameSelected: function(game) {
            if (game) {
                root.gameSelected(game.id, game.name, game.installPath || "", game.engine || "Unknown")
            }
        }

        onDialogClosed: {}
    }
}
