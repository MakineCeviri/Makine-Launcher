import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
import "dialogs"
import "components"
import "utils/VersionUtils.js" as VersionUtils

/**
 * HomeScreen.qml - Main home view with game status, announcements, and projects
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    // Layout tuning values
    property real contentMargin: 16
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 210
    property real layoutTopRowGap: 16
    property real layoutGamesSectionGap: 8
    property real layoutCardGap: 32
    property real layoutSepTopMargin: 4
    property real layoutSepBottomMargin: 8

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()

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

                        if (VersionUtils.compareVersions(remoteVersion, currentVersion) > 0) {
                            updateAvailable = true
                            latestVersion = tagName
                            downloadUrl = response.html_url || ""
                            notificationMessage = qsTr("Yeni sürüm mevcut: %1").arg(tagName)
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

    function showNotification(message, type) {
        notificationMessage = message
        notificationType = type || "info"
    }

    function hideNotification() {
        notificationDismissAnim.start()
    }

    SequentialAnimation {
        id: notificationDismissAnim
        NumberAnimation {
            target: notificationBanner
            property: "opacity"
            to: 0
            duration: Dimensions.transitionDuration
            easing.type: Easing.OutCubic
        }
        ScriptAction {
            script: {
                notificationMessage = ""
                notificationBanner.opacity = 1
            }
        }
    }

    Component.onCompleted: {
        GameService.scanAllLibraries()
        checkForUpdates()
    }

    function showHomePage() {
        pageStack.currentIndex = 0
        replayEntryAnimations()
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
        pageStack.currentIndex = 1
        projectsPage.replayProjectAnimations()
    }

    function showTranslationPage() {
        pageStack.currentIndex = 2
    }

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
                                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.15)
                                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.15)
                                    case "error": return Theme.withAlpha(Theme.notificationError, 0.15)
                                    default: return Theme.withAlpha(Theme.textMuted, 0.1)
                                }
                            }

                            border.color: {
                                switch(notificationType) {
                                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.4)
                                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.4)
                                    case "error": return Theme.withAlpha(Theme.notificationError, 0.4)
                                    default: return Theme.withAlpha(Theme.textPrimary, 0.1)
                                }
                            }
                            border.width: 1

                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Dimensions.marginMD
                                anchors.rightMargin: Dimensions.marginMS
                                spacing: Dimensions.spacingLG

                                Text {
                                    text: {
                                        switch(notificationType) {
                                            case "update": return "⬆"
                                            case "warning": return "⚠"
                                            case "error": return "✕"
                                            default: return "ℹ"
                                        }
                                    }
                                    font.pixelSize: Dimensions.fontLG
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
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: notificationType === "update" && downloadUrl
                                    width: updateBtnText.width + 16
                                    height: 28
                                    radius: Dimensions.radiusStandard
                                    color: updateBtnMouse.containsMouse ? Theme.withAlpha(Theme.notificationUpdate, 0.3) : Theme.withAlpha(Theme.notificationUpdate, 0.2)
                                    scale: updateBtnMouse.pressed ? 0.94 : 1.0
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Download update")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: Qt.openUrlExternally(downloadUrl)
                                    Keys.onSpacePressed: Qt.openUrlExternally(downloadUrl)

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                                    Text {
                                        id: updateBtnText
                                        anchors.centerIn: parent
                                        text: qsTr("İndir")
                                        font.pixelSize: Dimensions.fontSM
                                        font.weight: Font.Medium
                                        color: Theme.notificationUpdate
                                    }

                                    // Focus indicator
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -1
                                        radius: parent.radius + 1
                                        color: "transparent"
                                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                                        border.width: 2
                                        visible: parent.activeFocus
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
                                    color: closeBtnMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Close notification")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: hideNotification()
                                    Keys.onSpacePressed: hideNotification()

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        font.pixelSize: Dimensions.fontSM
                                        color: Theme.textMuted
                                    }

                                    // Focus indicator
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -1
                                        radius: parent.radius + 1
                                        color: "transparent"
                                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                                        border.width: 2
                                        visible: parent.activeFocus
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
                            Layout.preferredHeight: root.layoutTopRowHeight
                            Layout.maximumHeight: root.layoutTopRowHeight
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.layoutTopRowGap

                            opacity: 0
                            Component.onCompleted: topRowEntryAnim.start()

                            NumberAnimation {
                                id: topRowEntryAnim
                                target: topRowLayout
                                property: "opacity"
                                from: 0; to: 1
                                duration: Dimensions.animSlow
                                easing.type: Easing.OutCubic
                            }

                            // ============================================================
                            // GAME STATUS CARD
                            // ============================================================
                            Rectangle {
                                id: gameStatusCard
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.layoutTopRowHeight
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    id: gameStatusCardLayout
                                    anchors.fill: parent
                                    anchors.margins: root.layoutCardMargin
                                    spacing: root.layoutCardSpacing

                                    Row {
                                        Layout.fillWidth: true
                                        height: 44
                                        spacing: 14

                                        // Turkish flag with geometric proportions
                                        Rectangle {
                                            id: turkishFlagIcon
                                            width: 44
                                            height: 44
                                            radius: Dimensions.radiusStandard
                                            color: Theme.turkishRed
                                            clip: true

                                            Rectangle {
                                                x: 7; y: 11
                                                width: 22; height: 22
                                                radius: 11
                                                color: Theme.textOnColor
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
                                                antialiasing: true
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
                                            anchors.verticalCenter: turkishFlagIcon.verticalCenter
                                            spacing: 3

                                            Label {
                                                text: qsTr("Türkçe Yama")
                                                font.pixelSize: Dimensions.fontLG
                                                font.weight: Font.DemiBold
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                text: qsTr("Oyun çeviri durumu")
                                                font.pixelSize: Dimensions.fontXS
                                                color: Theme.textMuted
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 60
                                        radius: Dimensions.radiusStandard
                                        color: Theme.withAlpha(Theme.textPrimary, 0.02)
                                        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
                                        border.width: 1

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 14

                                            // Typing dots
                                            Row {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                spacing: 5

                                                Repeater {
                                                    model: 3

                                                    Rectangle {
                                                        required property int index
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        color: Theme.textMuted
                                                        opacity: 0.3
                                                        scale: 1.0

                                                        SequentialAnimation on opacity {
                                                            loops: Animation.Infinite
                                                            running: root.animationsEnabled
                                                            PauseAnimation { duration: index * 220 }
                                                            NumberAnimation { to: 0.75; duration: 500; easing.type: Easing.InOutSine }
                                                            NumberAnimation { to: 0.3; duration: 500; easing.type: Easing.InOutSine }
                                                            PauseAnimation { duration: (2 - index) * 220 + 600 }
                                                        }

                                                        SequentialAnimation on scale {
                                                            loops: Animation.Infinite
                                                            running: root.animationsEnabled
                                                            PauseAnimation { duration: index * 220 }
                                                            NumberAnimation { to: 1.35; duration: 500; easing.type: Easing.InOutSine }
                                                            NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutSine }
                                                            PauseAnimation { duration: (2 - index) * 220 + 600 }
                                                        }
                                                    }
                                                }
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: qsTr("Oyun Bekleniyor")
                                                font.pixelSize: Dimensions.fontMD
                                                font.weight: Font.DemiBold
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: qsTr("Desteklenen bir oyun çalıştırın veya manuel seçin")
                                                font.pixelSize: Dimensions.fontXS
                                                color: Theme.textMuted
                                            }
                                        }
                                    }

                                    Rectangle {
                                        id: manualBtn
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: Dimensions.radiusStandard
                                        color: manualBtnMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.1)
                                            : Theme.withAlpha(Theme.textPrimary, 0.05)
                                        border.color: manualBtnMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.15)
                                            : Theme.withAlpha(Theme.textPrimary, 0.08)
                                        border.width: 1
                                        Accessible.role: Accessible.Button
                                        Accessible.name: qsTr("Select game manually")
                                        activeFocusOnTab: true
                                        Keys.onReturnPressed: root.manualFolderRequested()
                                        Keys.onSpacePressed: root.manualFolderRequested()

                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Item {
                                                width: 13; height: 13
                                                anchors.verticalCenter: parent.verticalCenter

                                                Image {
                                                    anchors.fill: parent
                                                    source: "qrc:/qt/qml/MakineAI/resources/icons/window-search.svg"
                                                    sourceSize: Qt.size(13, 13)
                                                    opacity: manualBtnMouse.containsMouse ? 0 : 0.6
                                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                                }
                                                Image {
                                                    anchors.fill: parent
                                                    source: "qrc:/qt/qml/MakineAI/resources/icons/window-search-active.svg"
                                                    sourceSize: Qt.size(13, 13)
                                                    opacity: manualBtnMouse.containsMouse ? 0.9 : 0
                                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                                }
                                            }

                                            Label {
                                                text: qsTr("Manuel Oyun Ekle")
                                                font.pixelSize: Dimensions.fontSM
                                                font.weight: Font.Medium
                                                color: manualBtnMouse.containsMouse ? Theme.textSecondary : Theme.textMuted
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                            }
                                        }

                                        // Focus indicator
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: -1
                                            radius: parent.radius + 1
                                            color: "transparent"
                                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                                            border.width: 2
                                            visible: manualBtn.activeFocus
                                        }

                                        MouseArea {
                                            id: manualBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.manualFolderRequested()
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
                                Layout.preferredHeight: root.layoutTopRowHeight
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: root.layoutCardMargin
                                    spacing: root.layoutCardSpacing

                                    // Header
                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Label {
                                            text: qsTr("Duyurular")
                                            font.pixelSize: Dimensions.fontLG
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }
                                        Label {
                                            text: new Date().toLocaleDateString("tr-TR", { day: 'numeric', month: 'long', year: 'numeric' })
                                            font.pixelSize: Dimensions.fontXS
                                            color: Theme.textMuted
                                        }
                                    }

                                    // Announcement content
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 60
                                        radius: Dimensions.radiusStandard
                                        color: Theme.withAlpha(Theme.textPrimary, 0.02)
                                        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
                                        border.width: 1

                                        Flickable {
                                            anchors.fill: parent
                                            anchors.margins: Dimensions.marginBase
                                            contentHeight: announcementContentCol.height
                                            clip: true
                                            boundsBehavior: Flickable.StopAtBounds

                                            Column {
                                                id: announcementContentCol
                                                width: parent.width
                                                spacing: Dimensions.spacingSM

                                                // Title with accent bar
                                                Row {
                                                    width: parent.width
                                                    spacing: Dimensions.spacingSM

                                                    Rectangle {
                                                        width: 2
                                                        height: announcementTitle.height
                                                        radius: 1
                                                        color: Theme.withAlpha(Theme.textPrimary, 0.25)
                                                        anchors.verticalCenter: parent.verticalCenter
                                                    }

                                                    Label {
                                                        id: announcementTitle
                                                        text: qsTr("Kütüphaneye Dahil Olmayan Oyunlar")
                                                        font.pixelSize: Dimensions.fontBody
                                                        font.weight: Font.DemiBold
                                                        color: Theme.textPrimary
                                                    }
                                                }

                                                // Body text
                                                Label {
                                                    width: parent.width
                                                    text: qsTr("Çeviri kütüphanemizde henüz yer almayan oyunlarda beklenmedik sorunlarla karşılaşılabilir. Olası aksaklıklara karşı, yama öncesi otomatik oluşturulan yedek sayesinde dosyalarınız güvende tutulur.\n\nKütüphane dışı oyunlarda bazı metinler eksik görünebilir veya performans farklılıkları yaşanabilir. Eklenmesini istediğiniz oyunları bize bildirmeniz, değerlendirme sürecimize dahil edilecektir.")
                                                    font.pixelSize: Dimensions.fontXS
                                                    color: Theme.textSecondary
                                                    wrapMode: Text.WordWrap
                                                    lineHeight: 1.4
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        id: securityBtn
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: Dimensions.radiusStandard
                                        color: securityMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.1)
                                            : Theme.withAlpha(Theme.textPrimary, 0.05)
                                        border.color: securityMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.15)
                                            : Theme.withAlpha(Theme.textPrimary, 0.08)
                                        border.width: 1
                                        Accessible.role: Accessible.Link
                                        Accessible.name: qsTr("Visit makineai.com")
                                        activeFocusOnTab: true
                                        Keys.onReturnPressed: Qt.openUrlExternally("https://makineai.com")
                                        Keys.onSpacePressed: Qt.openUrlExternally("https://makineai.com")

                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

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
                                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                                }
                                                Image {
                                                    anchors.fill: parent
                                                    source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check-active.svg"
                                                    sourceSize: Qt.size(13, 13)
                                                    opacity: securityMouse.containsMouse ? 0.9 : 0
                                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                                }
                                            }

                                            Label {
                                                text: qsTr("Güvenliğiniz için yalnızca")
                                                font.pixelSize: Dimensions.fontCaption
                                                color: securityMouse.containsMouse ? Theme.textSecondary : Theme.textMuted
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                            }

                                            Label {
                                                text: "makineai.com"
                                                font.pixelSize: Dimensions.fontCaption
                                                font.weight: Font.Medium
                                                font.underline: true
                                                color: securityMouse.containsMouse ? Theme.primary : Theme.textSecondary
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                            }

                                            Label {
                                                text: qsTr("üzerinden indirin")
                                                font.pixelSize: Dimensions.fontCaption
                                                color: securityMouse.containsMouse ? Theme.textSecondary : Theme.textMuted
                                                anchors.verticalCenter: parent.verticalCenter
                                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                            }
                                        }

                                        // Focus indicator
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: -1
                                            radius: securityBtn.radius + 1
                                            color: "transparent"
                                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                                            border.width: 2
                                            visible: securityBtn.activeFocus
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

                        // ===== BATCH OPERATIONS PANEL =====
                        BatchOperationsPanel {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            animationsEnabled: root.animationsEnabled
                        }

                        // ===== GAMES SECTION =====
                        ColumnLayout {
                            id: gamesSectionLayout
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.layoutGamesSectionGap

                            opacity: 0
                            Component.onCompleted: gamesSectionEntryAnim.start()

                            SequentialAnimation {
                                id: gamesSectionEntryAnim
                                PauseAnimation { duration: Dimensions.transitionDuration }
                                NumberAnimation {
                                    target: gamesSectionLayout
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: Dimensions.animSlow
                                    easing.type: Easing.OutCubic
                                }
                            }

                            RowLayout {
                                spacing: Dimensions.spacingLG

                                Label {
                                    text: qsTr("Türkçe Yama Kütüphanesi")
                                    font.pixelSize: Dimensions.fontXL
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }

                                BusyIndicator {
                                    visible: GameService.isScanning
                                    running: GameService.isScanning
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                }

                                Item { Layout.fillWidth: true }

                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: root.layoutSepTopMargin
                                Layout.bottomMargin: root.layoutSepBottomMargin
                                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                            }

                            Flow {
                                id: skeletonFlow
                                Layout.fillWidth: true
                                spacing: Dimensions.cardGap
                                visible: GameService.supportedGameCount === 0 && GameService.isScanning

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
                                spacing: root.layoutCardGap
                                visible: !skeletonFlow.visible

                                readonly property int availableWidth: gamesSectionLayout.width
                                readonly property int cardTotal: Dimensions.cardWidth + root.layoutCardGap
                                readonly property int maxCards: Math.max(1, Math.floor((availableWidth - Dimensions.cardWidth) / cardTotal))

                                Repeater {
                                    id: gamesRepeater
                                    model: GameService.supportedGames.slice(0, gamesRow.maxCards)

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
                                                duration: Dimensions.fadeTransitionDuration
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
                                    remainingCount: Math.max(0, GameService.supportedGameCount - gamesRepeater.count)

                                    onClicked: {
                                        allGamesDialogLoader.active = true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1
                                visible: GameService.supportedGameCount === 0 && !GameService.isScanning

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: Dimensions.spacingLG

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "\uD83C\uDFAE"
                                        font.pixelSize: Dimensions.fontBanner
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: qsTr("Çeviri paketi bulunamadı")
                                        font.pixelSize: Dimensions.fontMD
                                        color: Theme.textSecondary
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: qsTr("translation_data klasörünü kontrol edin")
                                        font.pixelSize: Dimensions.fontSM
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

                readonly property var projectData: [
                    { title: "Cyberless: Online", desc: qsTr("Çok oyunculu cyberpunk aksiyon oyunu"), category: "oyun", status: qsTr("Tamamlandı"), statusColor: Theme.statusOnline, accent: Theme.statusOnline, emoji: "\uD83C\uDFAE", progress: 1.0 },
                    { title: "MakineAI Launcher", desc: qsTr("Türkçe oyun çevirisi başlatıcısı ve yönetim aracı"), category: "ceviri", status: qsTr("Alfa"), statusColor: Theme.primary, accent: Theme.primary, emoji: "\uD83D\uDE80", progress: 0.7 },
                    { title: qsTr("Topluluk Çeviri Paketi"), desc: qsTr("Açık kaynak topluluk çeviri paketleri"), category: "ceviri", status: qsTr("Sürekli"), statusColor: Theme.statusCyan, accent: Theme.statusCyan, emoji: "\uD83C\uDF0D", progress: -1 },
                    { title: "MakineAI", desc: qsTr("Oyun çevirisi için yapay zeka dil modeli"), category: "ceviri", status: qsTr("Geliştiriliyor"), statusColor: Theme.statusPurple, accent: Theme.statusPurple, emoji: "\uD83E\uDD16", progress: 0.3 },
                    { title: qsTr("Çeviri API"), desc: qsTr("Geliştiriciler için RESTful çeviri API hizmeti"), category: "ceviri", status: qsTr("Planlanıyor"), statusColor: Theme.warning, accent: Theme.warning, emoji: "\u26A1", progress: 0.0 }
                ]

                function replayProjectAnimations() {
                    for (var i = 0; i < projectCardsRepeater.count; i++) {
                        var item = projectCardsRepeater.itemAt(i)
                        if (item) item.replayAnimation()
                    }
                }

                // Main layout - EXACT same pattern as homepage
                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: root.contentMargin
                    spacing: Dimensions.marginMD

                    // ===== COMMUNITY SECTION =====
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.contentMargin
                        Layout.rightMargin: root.contentMargin
                        spacing: root.layoutGamesSectionGap

                        Label {
                            text: qsTr("Topluluk")
                            font.pixelSize: Dimensions.fontXL
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: root.layoutSepTopMargin
                            Layout.bottomMargin: root.layoutSepBottomMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

                            component LinkCard: Rectangle {
                                property string label: ""
                                property string subtitle: ""
                                property string url: ""
                                property color cardColor: Theme.primary
                                property string iconType: "" // "discord", "globe", "heart"

                                Layout.fillWidth: true
                                Layout.preferredHeight: 60
                                radius: Dimensions.radiusStandard
                                color: lcMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                                             : Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: lcMouse.containsMouse ? Theme.withAlpha(cardColor, 0.25)
                                                                    : Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Dimensions.marginMS
                                    anchors.rightMargin: Dimensions.marginMS
                                    spacing: Dimensions.spacingLG

                                    // Icon circle with Canvas-drawn vector icon
                                    Rectangle {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 16
                                        color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.15 : 0.08)
                                        border.color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.25 : 0.12)
                                        border.width: 1
                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                        Canvas {
                                            anchors.centerIn: parent
                                            width: 16; height: 16
                                            onPaint: {
                                                var ctx = getContext("2d")
                                                ctx.clearRect(0, 0, width, height)
                                                ctx.strokeStyle = Qt.rgba(cardColor.r, cardColor.g, cardColor.b, 0.9)
                                                ctx.fillStyle = ctx.strokeStyle
                                                ctx.lineWidth = 1.5
                                                ctx.lineCap = "round"
                                                ctx.lineJoin = "round"

                                                if (iconType === "discord") {
                                                    // Chat bubble icon
                                                    ctx.beginPath()
                                                    ctx.moveTo(2, 4)
                                                    ctx.lineTo(2, 11)
                                                    ctx.lineTo(5, 11)
                                                    ctx.lineTo(5, 14)
                                                    ctx.lineTo(8, 11)
                                                    ctx.lineTo(14, 11)
                                                    ctx.lineTo(14, 4)
                                                    ctx.closePath()
                                                    ctx.stroke()
                                                    // Dots inside
                                                    ctx.beginPath()
                                                    ctx.arc(6, 7.5, 1, 0, Math.PI * 2)
                                                    ctx.fill()
                                                    ctx.beginPath()
                                                    ctx.arc(10, 7.5, 1, 0, Math.PI * 2)
                                                    ctx.fill()
                                                } else if (iconType === "globe") {
                                                    // Globe icon
                                                    ctx.beginPath()
                                                    ctx.arc(8, 8, 6.5, 0, Math.PI * 2)
                                                    ctx.stroke()
                                                    // Horizontal line
                                                    ctx.beginPath()
                                                    ctx.moveTo(1.5, 8)
                                                    ctx.lineTo(14.5, 8)
                                                    ctx.stroke()
                                                    // Vertical ellipse (meridian)
                                                    ctx.beginPath()
                                                    ctx.ellipse(4.5, 1.5, 7, 13, 0, 0, Math.PI * 2)
                                                    ctx.stroke()
                                                } else if (iconType === "heart") {
                                                    // Heart icon
                                                    ctx.beginPath()
                                                    ctx.moveTo(8, 14)
                                                    ctx.bezierCurveTo(1, 9, 1, 3.5, 4.5, 2.5)
                                                    ctx.bezierCurveTo(6.5, 2, 8, 4, 8, 4)
                                                    ctx.bezierCurveTo(8, 4, 9.5, 2, 11.5, 2.5)
                                                    ctx.bezierCurveTo(15, 3.5, 15, 9, 8, 14)
                                                    ctx.closePath()
                                                    ctx.fill()
                                                    ctx.globalAlpha = 0.3
                                                    ctx.stroke()
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                                        Label {
                                            text: label; font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                            color: lcMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        }
                                        Label { text: subtitle; font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted }
                                    }

                                    Label {
                                        text: "\u2197"; font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                                        opacity: lcMouse.containsMouse ? 1.0 : 0.3
                                        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                    }
                                }

                                MouseArea {
                                    id: lcMouse; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally(url)
                                }
                                Accessible.role: Accessible.Link; Accessible.name: label
                                activeFocusOnTab: true
                                Keys.onReturnPressed: Qt.openUrlExternally(url)
                                Keys.onSpacePressed: Qt.openUrlExternally(url)
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: -1
                                    radius: parent.radius + 1; color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6); border.width: 2
                                    visible: parent.activeFocus
                                }
                            }

                            LinkCard { label: "Discord"; subtitle: qsTr("Topluluğa katıl"); url: Dimensions.discordUrl; cardColor: Theme.discordColor; iconType: "discord" }
                            LinkCard { label: qsTr("Web Sitesi"); subtitle: "makineai.com"; url: Dimensions.websiteUrl; cardColor: Theme.primary; iconType: "globe" }
                            LinkCard { label: qsTr("Destekçi Ol"); subtitle: qsTr("Projeleri destekle"); url: Dimensions.donatePageUrl; cardColor: Theme.brandCoral; iconType: "heart" }
                        }
                    }

                    // ===== PROJECTS SECTION =====
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.contentMargin
                        Layout.rightMargin: root.contentMargin
                        spacing: root.layoutGamesSectionGap

                        RowLayout {
                            spacing: Dimensions.spacingLG

                            Label {
                                text: qsTr("Aktif Projeler")
                                font.pixelSize: Dimensions.fontXL
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Rectangle {
                                Layout.preferredHeight: 22
                                Layout.preferredWidth: projectCountLabel.width + 14
                                radius: Dimensions.badgeRadius
                                color: Theme.withAlpha(Theme.primary, 0.12)
                                Label {
                                    id: projectCountLabel; anchors.centerIn: parent
                                    text: qsTr("%1 proje").arg(projectsPage.projectData.length)
                                    font.pixelSize: Dimensions.fontXS; font.weight: Font.Medium; color: Theme.primary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: root.layoutSepTopMargin
                            Layout.bottomMargin: root.layoutSepBottomMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Dimensions.spacingXL
                            rowSpacing: Dimensions.spacingLG

                            Repeater {
                                id: projectCardsRepeater
                                model: projectsPage.projectData

                                ProjectShowcaseCard {
                                    title: modelData.title
                                    description: modelData.desc
                                    status: modelData.status
                                    statusColor: modelData.statusColor
                                    accentColor: modelData.accent
                                    emoji: modelData.emoji
                                    progress: modelData.progress
                                    entryIndex: index
                                    animationsEnabled: root.animationsEnabled
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // Bottom fade gradient overlay
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 60
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Theme.bgPrimary }
                    }
                }
            }

            // ===== TRANSLATION PAGE (Index 2) - Türkçe Yamalar =====
            Item {
                id: translationPage

                // Filter state: "all", "installed", "available"
                property string activeFilter: "all"

                // Track installing games
                property var installingGames: ({})
                property var installProgressMap: ({})

                function filteredModel() {
                    var source = GameService.gamesWithTranslation
                    var result = []
                    for (var i = 0; i < source.length; i++) {
                        var game = source[i]
                        if (translationPage.activeFilter === "installed" && !game.packageInstalled) continue
                        if (translationPage.activeFilter === "available" && game.packageInstalled) continue
                        result.push(game)
                    }
                    return result
                }

                function countByFilter(filterKey) {
                    var source = GameService.gamesWithTranslation
                    if (filterKey === "all") return source.length
                    var count = 0
                    for (var i = 0; i < source.length; i++) {
                        if (filterKey === "installed" && source[i].packageInstalled) count++
                        if (filterKey === "available" && !source[i].packageInstalled) count++
                    }
                    return count
                }

                // Uninstall confirm dialog
                UninstallConfirmDialog {
                    id: uninstallDialog
                    parent: Overlay.overlay
                    onConfirmed: {
                        GameService.uninstallTranslation(uninstallDialog.gameId)
                    }
                }

                Connections {
                    target: GameService
                    function onTranslationInstallStarted(gameId) {
                        var m = translationPage.installingGames
                        m[gameId] = true
                        translationPage.installingGames = m
                        patchRepeater.model = translationPage.filteredModel()
                    }
                    function onTranslationInstallProgress(gameId, progress, status) {
                        var m = translationPage.installProgressMap
                        m[gameId] = progress
                        translationPage.installProgressMap = m
                        patchRepeater.model = translationPage.filteredModel()
                    }
                    function onTranslationInstallCompleted(gameId, success, message) {
                        var m = translationPage.installingGames
                        delete m[gameId]
                        translationPage.installingGames = m
                        var p = translationPage.installProgressMap
                        delete p[gameId]
                        translationPage.installProgressMap = p
                        patchRepeater.model = translationPage.filteredModel()
                    }
                    function onTranslationUninstalled(gameId, success, message) {
                        patchRepeater.model = translationPage.filteredModel()
                    }
                    function onGamesChanged() {
                        patchRepeater.model = translationPage.filteredModel()
                    }
                }

                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: parent.width
                        spacing: Dimensions.spacingLG

                        Item { Layout.preferredHeight: root.contentMargin }

                        // ===== HEADER =====
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: Dimensions.spacingMD

                            Label {
                                text: qsTr("Türkçe Yamalar")
                                font.pixelSize: Dimensions.fontXL
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Rectangle {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: patchCountLabel.width + 12
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.primary, 0.10)
                                Label {
                                    id: patchCountLabel
                                    anchors.centerIn: parent
                                    text: qsTr("%1 kurulu oyunda yama mevcut").arg(GameService.gamesWithTranslation.length)
                                    font.pixelSize: Dimensions.fontXS
                                    font.weight: Font.Medium
                                    color: Theme.primary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // ===== FILTER BUTTONS =====
                        Row {
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: Dimensions.spacingSM

                            Repeater {
                                model: [
                                    { key: "all", label: qsTr("Tümü") },
                                    { key: "installed", label: qsTr("Kurulu") },
                                    { key: "available", label: qsTr("Mevcut") }
                                ]

                                Rectangle {
                                    required property var modelData
                                    required property int index

                                    property int filterCount: translationPage.countByFilter(modelData.key)

                                    width: filterRow.width + Dimensions.paddingLG * 2
                                    height: 28
                                    radius: Dimensions.radiusStandard
                                    color: translationPage.activeFilter === modelData.key
                                        ? Theme.withAlpha(Theme.primary, 0.12)
                                        : filterMouse.containsMouse
                                            ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                            : "transparent"
                                    border.color: translationPage.activeFilter === modelData.key
                                        ? Theme.withAlpha(Theme.primary, 0.30)
                                        : Theme.withAlpha(Theme.textPrimary, 0.08)
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: modelData.label
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: {
                                        translationPage.activeFilter = modelData.key
                                        patchRepeater.model = translationPage.filteredModel()
                                    }
                                    Keys.onSpacePressed: {
                                        translationPage.activeFilter = modelData.key
                                        patchRepeater.model = translationPage.filteredModel()
                                    }

                                    Row {
                                        id: filterRow
                                        anchors.centerIn: parent
                                        spacing: Dimensions.spacingXS

                                        Label {
                                            text: modelData.label
                                            font.pixelSize: Dimensions.fontXS
                                            font.weight: translationPage.activeFilter === modelData.key ? Font.DemiBold : Font.Medium
                                            color: translationPage.activeFilter === modelData.key
                                                ? Theme.primary
                                                : Theme.textSecondary
                                            anchors.verticalCenter: parent.verticalCenter
                                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        }

                                        // Count badge
                                        Rectangle {
                                            visible: filterCount > 0
                                            width: countLabel.width + 8
                                            height: 16
                                            radius: 8
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: translationPage.activeFilter === modelData.key
                                                ? Theme.withAlpha(Theme.primary, 0.15)
                                                : Theme.withAlpha(Theme.textPrimary, 0.06)

                                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                            Label {
                                                id: countLabel
                                                anchors.centerIn: parent
                                                text: filterCount
                                                font.pixelSize: Dimensions.fontMicro
                                                font.weight: Font.Bold
                                                color: translationPage.activeFilter === modelData.key
                                                    ? Theme.primary
                                                    : Theme.textMuted
                                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: filterMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            translationPage.activeFilter = modelData.key
                                            patchRepeater.model = translationPage.filteredModel()
                                        }
                                    }
                                }
                            }
                        }

                        // Separator
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        // ===== PATCH GRID =====
                        Flow {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: Dimensions.spacingMD
                            visible: patchRepeater.count > 0

                            Repeater {
                                id: patchRepeater
                                model: translationPage.filteredModel()

                                PatchCard {
                                    required property var modelData
                                    required property int index

                                    gameId: modelData.id || ""
                                    gameName: modelData.name || ""
                                    imageUrl: modelData.logoImageUrl || modelData.headerImageUrl || ""
                                    packageInstalled: modelData.packageInstalled || false
                                    isInstalling: translationPage.installingGames[modelData.id] || false
                                    installProgress: translationPage.installProgressMap[modelData.id] || 0.0

                                    onInstallClicked: {
                                        GameService.installTranslation(modelData.id)
                                    }
                                    onUninstallClicked: {
                                        uninstallDialog.gameId = modelData.id
                                        uninstallDialog.gameName = modelData.name || ""
                                        uninstallDialog.open()
                                    }
                                    onCardClicked: {
                                        root.gameSelected(modelData.id, modelData.name || "", modelData.installPath || "", modelData.engine || "")
                                    }
                                }
                            }
                        }

                        // Empty state
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 160
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.textPrimary, 0.02)
                            border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
                            border.width: 1
                            visible: patchRepeater.count === 0 && !GameService.isScanning

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Dimensions.spacingMD

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: ":/"
                                    font.pixelSize: Dimensions.fontHero
                                    color: Theme.textMuted
                                }
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: translationPage.activeFilter === "all"
                                        ? qsTr("Henüz yama bulunamadı")
                                        : translationPage.activeFilter === "installed"
                                            ? qsTr("Kurulu yama yok")
                                            : qsTr("Tüm yamalar kurulu")
                                    font.pixelSize: Dimensions.fontBody
                                    font.weight: Font.Medium
                                    color: Theme.textSecondary
                                }
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: translationPage.activeFilter === "all"
                                        ? qsTr("Bilgisayarınızda çevirisi olan oyun bulunamadı")
                                        : translationPage.activeFilter === "installed"
                                            ? qsTr("Mevcut yamalardan birini kurmak için Tümü filtresine geçin")
                                            : qsTr("Tüm yamalar başarıyla kurulmuş durumda")
                                    font.pixelSize: Dimensions.fontXS
                                    color: Theme.textMuted
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.maximumWidth: 340
                                    wrapMode: Text.WordWrap
                                }

                                // Scan CTA button (only on "all" filter with no games)
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.topMargin: Dimensions.spacingXS
                                    visible: translationPage.activeFilter === "all" && GameService.gameCount === 0
                                    width: scanCtaRow.width + Dimensions.paddingLG * 2
                                    height: 32
                                    radius: Dimensions.radiusStandard
                                    color: scanCtaMouse.containsMouse ? Theme.primaryHover : Theme.primary

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Scan game libraries")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: GameService.scanAllLibraries()
                                    Keys.onSpacePressed: GameService.scanAllLibraries()

                                    Row {
                                        id: scanCtaRow
                                        anchors.centerIn: parent
                                        spacing: Dimensions.spacingSM

                                        Label {
                                            text: "\uD83D\uDD0D"
                                            font.pixelSize: Dimensions.fontXS
                                            color: Theme.textOnColor
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Label {
                                            text: qsTr("Kütüphaneleri Tara")
                                            font.pixelSize: Dimensions.fontXS
                                            font.weight: Font.DemiBold
                                            color: Theme.textOnColor
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: scanCtaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: GameService.scanAllLibraries()
                                    }
                                }
                            }
                        }

                        // Scanning indicator
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.textPrimary, 0.03)
                            visible: GameService.isScanning

                            Row {
                                anchors.centerIn: parent
                                spacing: Dimensions.spacingMD
                                BusyIndicator { width: 14; height: 14; running: true }
                                Label {
                                    text: GameService.scanStatus || qsTr("Kütüphaneler taranıyor...")
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.textMuted
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }

                        // ===== GAME LIBRARY SECTION =====
                        Item { Layout.preferredHeight: Dimensions.spacingLG }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: Dimensions.spacingMD

                            Label {
                                text: qsTr("Oyun Kütüphanesi")
                                font.pixelSize: Dimensions.fontXL
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            BusyIndicator {
                                visible: GameService.isScanning
                                running: GameService.isScanning
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }

                            Rectangle {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: libCountLabel.width + 12
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.textMuted, 0.10)
                                Label {
                                    id: libCountLabel
                                    anchors.centerIn: parent
                                    text: qsTr("%1 kurulu").arg(GameService.gameCount)
                                    font.pixelSize: Dimensions.fontXS
                                    font.weight: Font.Medium
                                    color: Theme.textSecondary
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Rescan button
                            Rectangle {
                                visible: !GameService.isScanning
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                radius: Dimensions.badgeRadius
                                color: libRescanMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Rescan libraries")
                                activeFocusOnTab: true
                                Keys.onReturnPressed: GameService.scanAllLibraries()
                                Keys.onSpacePressed: GameService.scanAllLibraries()

                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                                Label {
                                    anchors.centerIn: parent
                                    text: "\u21BB"
                                    font.pixelSize: Dimensions.fontBody
                                    color: libRescanMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                }

                                MouseArea {
                                    id: libRescanMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: GameService.scanAllLibraries()
                                }

                                ToolTip {
                                    visible: libRescanMouse.containsMouse
                                    text: qsTr("Kütüphaneleri yeniden tara")
                                    delay: 400
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        // Library game grid
                        Flow {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: Dimensions.cardGap
                            visible: GameService.gameCount > 0

                            Repeater {
                                id: libraryRepeater
                                model: GameService.games

                                Item {
                                    id: libCard
                                    width: Dimensions.cardWidth
                                    height: Dimensions.cardHeight

                                    required property var modelData
                                    required property int index

                                    property bool hov: libCardMouse.containsMouse

                                    // Hover lift: Y translate + scale (matches GameCard)
                                    transform: [
                                        Translate { y: libCard.hov ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
                                        Scale {
                                            origin.x: libCard.width / 2; origin.y: libCard.height / 2
                                            xScale: libCard.hov ? 1.02 : 1.0; yScale: libCard.hov ? 1.02 : 1.0
                                            Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                                            Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                                        }
                                    ]

                                    Rectangle {
                                        id: libCardClip
                                        anchors.fill: parent
                                        radius: Dimensions.cardBorderRadius
                                        color: Theme.surface
                                        clip: true

                                        // Animated gradient border phase
                                        property real borderPhase: 0
                                        NumberAnimation on borderPhase {
                                            from: 0; to: 1
                                            duration: 8000
                                            loops: Animation.Infinite
                                            running: libCard.hov
                                        }

                                        // Animated rainbow gradient border (matches GameCard)
                                        Canvas {
                                            anchors.fill: parent
                                            z: Dimensions.zContent
                                            property real phase: libCardClip.borderPhase
                                            onPhaseChanged: if (hov) requestPaint()
                                            property bool hov: libCard.hov
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
                                                    grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                                                var bw = 1.5
                                                var r = Dimensions.cardBorderRadius - bw / 2
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

                                        // Mask for rounded corners
                                        Item {
                                            id: libImgMask
                                            anchors.fill: parent
                                            visible: false
                                            layer.enabled: true

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: Dimensions.cardBorderRadius
                                                color: Theme.textOnColor
                                            }
                                        }

                                        Image {
                                            id: libImg
                                            anchors.fill: parent
                                            source: libCard.modelData.headerImageUrl || ""
                                            fillMode: Image.PreserveAspectCrop
                                            sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                                            asynchronous: true
                                            cache: true
                                            visible: false
                                        }

                                        // Masked image with brightness on hover
                                        MultiEffect {
                                            anchors.fill: libImg
                                            source: libImg
                                            maskEnabled: true
                                            maskSource: libImgMask
                                            visible: libImg.status === Image.Ready
                                            brightness: libCard.hov ? 0.06 : 0
                                            Behavior on brightness { NumberAnimation { duration: Dimensions.animNormal } }
                                        }

                                        // Placeholder
                                        Rectangle {
                                            anchors.fill: parent
                                            color: Theme.withAlpha(Theme.textPrimary, 0.04)
                                            visible: libImg.status !== Image.Ready

                                            Text {
                                                anchors.centerIn: parent
                                                text: libCard.modelData.name ? libCard.modelData.name.substring(0, 2).toUpperCase() : ""
                                                font.pixelSize: Dimensions.fontXL
                                                font.weight: Font.Bold
                                                color: Theme.withAlpha(Theme.textMuted, 0.5)
                                            }
                                        }

                                        // Bottom gradient
                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: parent.height * 0.45
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: "transparent" }
                                                GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.9) }
                                            }
                                        }

                                        // TR badge if has translation
                                        Rectangle {
                                            visible: libCard.modelData.hasTranslation === true
                                            anchors.top: parent.top
                                            anchors.right: parent.right
                                            anchors.topMargin: Dimensions.spacingSM
                                            anchors.rightMargin: Dimensions.spacingSM
                                            width: 22; height: 14
                                            radius: Dimensions.badgeRadius
                                            color: Theme.turkishRed

                                            Text {
                                                anchors.centerIn: parent
                                                text: "TR"
                                                font.pixelSize: Dimensions.fontMicro
                                                font.weight: Font.Bold
                                                color: "#ffffff"
                                            }
                                        }

                                        // Source badge (Steam/Epic/GOG)
                                        Rectangle {
                                            anchors.top: parent.top
                                            anchors.left: parent.left
                                            anchors.topMargin: Dimensions.spacingSM
                                            anchors.leftMargin: Dimensions.spacingSM
                                            width: srcLabel.width + 8; height: 14
                                            radius: Dimensions.badgeRadius
                                            color: Theme.withAlpha(Theme.bgPrimary, 0.7)
                                            visible: libCard.modelData.source && libCard.modelData.source !== ""

                                            Text {
                                                id: srcLabel
                                                anchors.centerIn: parent
                                                text: {
                                                    var s = libCard.modelData.source || ""
                                                    if (s === "steam") return "Steam"
                                                    if (s === "epic") return "Epic"
                                                    if (s === "gog") return "GOG"
                                                    return s.charAt(0).toUpperCase() + s.slice(1)
                                                }
                                                font.pixelSize: Dimensions.fontMicro
                                                color: Theme.textMuted
                                            }
                                        }

                                        // Game name
                                        Text {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            anchors.margins: Dimensions.marginBase
                                            text: libCard.modelData.name || ""
                                            font.pixelSize: Dimensions.fontCaption
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            maximumLineCount: 2
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    // Focus indicator
                                    Rectangle {
                                        anchors.fill: libCardClip
                                        anchors.margins: -2
                                        radius: libCardClip.radius + 2
                                        color: "transparent"
                                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                                        border.width: 2
                                        visible: libCard.activeFocus
                                    }

                                    MouseArea {
                                        id: libCardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.gameSelected(
                                                libCard.modelData.id || "",
                                                libCard.modelData.name || "",
                                                libCard.modelData.installPath || "",
                                                libCard.modelData.engine || ""
                                            )
                                        }
                                    }
                                }
                            }
                        }

                        // Library empty state
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 100
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            radius: Dimensions.radiusStandard
                            color: Theme.withAlpha(Theme.textPrimary, 0.02)
                            border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
                            border.width: 1
                            visible: GameService.gameCount === 0 && !GameService.isScanning

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Dimensions.spacingSM

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Kurulu oyun bulunamadı")
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textSecondary
                                }
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Steam, Epic veya GOG kütüphanenizi tarayın")
                                    font.pixelSize: Dimensions.fontXS
                                    color: Theme.textMuted
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 60 }
                    }
                }

                // Bottom fade gradient
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 60; z: 1
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Theme.bgPrimary }
                    }
                }
            }
        }
    }

    // ===== GAME CARD COMPONENT - Modern minimal hover =====
    component GameCard: Item {
        id: gameCardRoot
        property string gameName: ""
        property string imageUrl: ""
        property bool verified: false
        property bool translated: false

        signal clicked()

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: gameName
        Accessible.description: {
            var desc = gameName
            if (verified) desc += " — " + qsTr("Verified")
            if (translated) desc += " — " + qsTr("Turkish translation available")
            return desc
        }
        Accessible.onPressAction: clicked()
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: cardMouse.containsMouse

        transform: [
            Translate { y: gameCardRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: gameCardRoot.width / 2; origin.y: gameCardRoot.height / 2
                xScale: gameCardRoot.isHovered ? 1.02 : 1.0; yScale: gameCardRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
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
                z: Dimensions.zContent
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
                        grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

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
                sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
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
                Behavior on brightness { NumberAnimation { duration: Dimensions.animNormal } }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingMD
                visible: gameImage.status !== Image.Ready
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: Dimensions.fontXL
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }

            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: parent.height * 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.9) }
                }
            }

            Rectangle {
                visible: gameCardRoot.translated || gameCardRoot.verified
                anchors.top: parent.top; anchors.right: parent.right
                anchors.topMargin: Dimensions.spacingSM; anchors.rightMargin: Dimensions.spacingSM
                width: badgeRow.width + 8; height: 20
                radius: Dimensions.badgeRadius
                color: Theme.withAlpha(Theme.bgPrimary, 0.7)

                Row {
                    id: badgeRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingXS
                    Rectangle {
                        visible: gameCardRoot.translated
                        width: 20; height: 14; radius: Dimensions.badgeRadius
                        color: Theme.turkishRed; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "TR"; font.pixelSize: Dimensions.fontMicro; font.weight: Font.Bold; color: Theme.textOnColor }
                    }
                    Rectangle {
                        visible: gameCardRoot.verified
                        width: 14; height: 14; radius: 7
                        color: Theme.primary; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "✓"; font.pixelSize: Dimensions.fontMini; font.weight: Font.Bold; color: Theme.textOnColor }
                    }
                }
            }


            Label {
                id: gameNameLabel
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.margins: Dimensions.marginBase
                text: gameName; font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold
                color: Theme.textPrimary; elide: Text.ElideRight

                ToolTip {
                    visible: gameCardRoot.isHovered && gameNameLabel.truncated
                    delay: 300; text: gameName; font.pixelSize: Dimensions.fontSM
                    background: Rectangle { color: Theme.withAlpha(Theme.surface, 0.96); radius: Dimensions.radiusStandard; border.color: Theme.withAlpha(Theme.textPrimary, 0.12); border.width: 1 }
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

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("View All (+%1)").arg(remainingCount)
        Accessible.onPressAction: clicked()
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: viewAllMouse.containsMouse

        transform: [
            Translate { y: viewAllRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: viewAllRoot.width / 2; origin.y: viewAllRoot.height / 2
                xScale: viewAllRoot.isHovered ? 1.02 : 1.0; yScale: viewAllRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
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
                        grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

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
                        numGrad.addColorStop(i / Math.max(1, colors.length - 1), colors[i]);

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
                    var label = hov ? qsTr("Tümünü Gör") + " →" : qsTr("Tümünü Gör");
                    ctx.fillText(label, Math.round(cx), lineY + 6);
                }
            }
        }

        // Focus indicator
        Rectangle {
            anchors.fill: viewAllContent
            anchors.margins: -2
            radius: viewAllContent.radius + 2
            color: "transparent"
            border.color: Theme.withAlpha(Theme.primary, 0.6)
            border.width: 2
            visible: viewAllRoot.activeFocus
            z: Dimensions.zBase
        }

        MouseArea {
            id: viewAllMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewAllRoot.clicked()
        }
    }

    // ===== ALL GAMES LOADING OVERLAY =====
    Rectangle {
        id: allGamesLoadingOverlay
        parent: Overlay.overlay
        anchors.fill: parent
        color: Theme.withAlpha("#000000", 0.4)
        visible: allGamesDialogLoader.active && allGamesDialogLoader.status !== Loader.Ready
        z: 999

        // Block clicks while loading
        MouseArea { anchors.fill: parent }

        Column {
            anchors.centerIn: parent
            spacing: 16

            // Spinning arc indicator
            Item {
                width: 40; height: 40
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    id: spinnerRing
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                    border.width: 3
                }

                Canvas {
                    id: spinnerArc
                    anchors.fill: parent

                    property real angle: 0
                    NumberAnimation on angle {
                        from: 0; to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: allGamesLoadingOverlay.visible
                    }
                    onAngleChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = Theme.primary
                        ctx.lineWidth = 3
                        ctx.lineCap = "round"
                        var startRad = angle * Math.PI / 180
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 2, startRad, startRad + Math.PI * 0.75)
                        ctx.stroke()
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Yükleniyor...")
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

    // ===== ALL GAMES DIALOG (lazy-loaded) =====
    Loader {
        id: allGamesDialogLoader
        active: false
        sourceComponent: Component {
            AllGamesDialog {
                parent: Overlay.overlay

                Component.onCompleted: {
                    games = GameService.supportedGames
                    open()
                }

                onGameSelected: function(gameId) {
                    var gameData = GameService.getGameById(gameId)
                    if (gameData) {
                        root.gameSelected(gameId, gameData.name, gameData.installPath, gameData.engine || "Unknown")
                    }
                    close()
                }

                onClosed: allGamesDialogLoader.active = false
            }
        }
    }

}
