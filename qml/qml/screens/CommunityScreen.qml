// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * CommunityScreen.qml - "Topluluk": the website's community wall
 * (makineceviri.org/topluluk) inside the launcher, read-only
 * (CommunityService). The latest posts, filtered by type, and beside them
 * the most wanted patches, the unanswered questions and the month's most
 * active members. Taking part — posting, replying, liking, voting — needs a
 * member: the launcher signs in once by e-mail as the site does (an address,
 * then the 6-digit code it receives) or with Discord through the site's page
 * (CommunityService), then the composer, the hearts, the replies and the
 * polls work here. Posts rise into place as they arrive.
 */
Item {
    id: community

    property real contentMargin: 16

    function scrollToTop() { flick.contentY = 0 }

    readonly property string _site: "https://makineceviri.org/topluluk/"
    readonly property var _meta: CommunityService.meta
    readonly property bool _wide: width >= 980
    // What a new post can be (the team's polls and announcements stay on the site)
    readonly property var _postTypes: [
        { value: "sohbet", text: qsTr("Sohbet") },
        { value: "soru", text: qsTr("Soru") },
        { value: "oneri", text: qsTr("Öneri") },
        { value: "istek", text: qsTr("Yama isteği") },
        { value: "arkadas", text: qsTr("Oyun arkadaşı") }
    ]
    property string _newType: "sohbet"

    readonly property var _types: [
        { value: "", text: qsTr("Tümü") },
        { value: "soru", text: qsTr("Sorular") },
        { value: "oneri", text: qsTr("Öneriler") },
        { value: "istek", text: qsTr("Yama istekleri") },
        { value: "arkadas", text: qsTr("Oyun arkadaşı") },
        { value: "sohbet", text: qsTr("Sohbet") }
    ]

    function _typeTint(type) {
        switch (type) {
            case "soru": return Theme.info
            case "oneri": return Theme.success
            case "istek": return Theme.primaryText
            case "arkadas": return Theme.warning
            case "anket": return Theme.primaryText
            case "ekip": return Theme.turkishRed
            default: return Theme.textMuted
        }
    }

    // Fresh each time the page shows: the wall moves while the launcher runs
    onVisibleChanged: if (visible && !CommunityService.loading) CommunityService.refresh(CommunityService.type)
    Component.onCompleted: if (visible) CommunityService.refresh()

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top
        ScrollBar.vertical: StyledScrollBar {
            parent: flick.parent
            anchors.top: flick.top; anchors.right: flick.right; anchors.bottom: flick.bottom
        }

        ColumnLayout {
            id: page
            width: flick.width
            spacing: 22

            PageHeader {
                Layout.topMargin: 52
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                title: qsTr("Topluluk")
                subtitle: community._meta.members !== undefined
                          ? qsTr("Sorun, önerin, yama isteyin; ekip yama isteklerini buradan okuyor. %1 üye · %2 paylaşım.")
                                .arg(community._meta.members).arg(community._meta.posts)
                          : qsTr("Sorun, önerin, yama isteyin; ekip yama isteklerini buradan okuyor.")

                AppButton {
                    style: "gray"
                    icon: Icons.refreshCw
                    busy: CommunityService.loading
                    toolTip: qsTr("Yenile")
                    onClicked: CommunityService.refresh(CommunityService.type)
                }
                // Who is signed in, and the way out
                Row {
                    visible: CommunityService.signedIn
                    spacing: 10
                    Avatar {
                        anchors.verticalCenter: parent.verticalCenter
                        person: CommunityService.member
                        size: 30
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: CommunityService.member.name ?? ""
                        font.pixelSize: Dimensions.body
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    AppButton {
                        anchors.verticalCenter: parent.verticalCenter
                        style: "gray"
                        size: "small"
                        icon: Icons.logOut
                        toolTip: qsTr("Hesabı bu bilgisayardan çıkar")
                        onClicked: CommunityService.signOut()
                    }
                }
            }

            // ── Write ───────────────────────────────────────────────────
            Card {
                id: composer
                // Signed in by e-mail for the first time: a name comes first
                readonly property bool _needName: CommunityService.signedIn && CommunityService.member.needName === true
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                implicitHeight: composerCol.implicitHeight + 32
                radius: Dimensions.radiusSection

                ColumnLayout {
                    id: composerCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12

                    // Not signed in: what signing in means, the code the e-mail
                    // brought, or (Discord) the code to confirm on the site
                    RowLayout {
                        visible: !CommunityService.signedIn
                        Layout.fillWidth: true
                        spacing: 14
                        IconTile {
                            Layout.alignment: Qt.AlignTop
                            size: 34
                            glyph: CommunityService.connecting ? Icons.globe
                                 : CommunityService.codeEmail !== "" ? Icons.mail : Icons.messagesSquare
                            tint: Theme.primary
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: CommunityService.connecting
                                      ? qsTr("Tarayıcıda açılan sayfada bu kodu onayla")
                                      : CommunityService.codeEmail !== "" ? qsTr("E-postana gelen kodu yaz")
                                      : qsTr("Topluluğa launcher üzerinden yaz")
                                font.pixelSize: Dimensions.headline
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                                text: CommunityService.connecting
                                      ? qsTr("Sitede Discord'la gir ve \"Makine Launcher'ı bağla\"ya bas. Onaylayınca burası kendiliğinden açılır.")
                                      : CommunityService.codeEmail !== ""
                                      ? qsTr("%1 adresine 6 haneli bir kod gönderdik, 10 dakika geçerli. Gelmediyse gereksiz klasörüne de bak.").arg(CommunityService.codeEmail)
                                      : qsTr("Sitedeki gibi e-postanla gir: adresini yaz, gelen 6 haneli kodu buraya gir. Sonra soru, öneri ve yama isteklerini buradan paylaşır, beğenir, yanıtlarsın.")
                                font.pixelSize: Dimensions.callout
                                color: Theme.textSecondary
                            }
                        }
                        // The code, large, as the site shows it
                        Rectangle {
                            visible: CommunityService.connecting
                            implicitWidth: codeText.implicitWidth + 28
                            implicitHeight: 40
                            radius: Dimensions.radiusLG
                            color: Theme.withAlpha(Theme.primary, 0.14)
                            border.width: 1
                            border.color: Theme.withAlpha(Theme.primary, 0.4)
                            Text {
                                id: codeText
                                anchors.centerIn: parent
                                textFormat: Text.PlainText
                                text: CommunityService.connectCode
                                font.family: Dimensions.fontDisplay
                                font.pixelSize: Dimensions.title3
                                font.weight: Font.Bold
                                font.letterSpacing: 2
                                color: Theme.primaryText
                            }
                        }
                        AppButton {
                            icon: Icons.x
                            visible: CommunityService.connecting
                            style: "gray"
                            text: qsTr("Vazgeç")
                            onClicked: CommunityService.cancelConnect()
                        }
                    }

                    // The address, then the code it receives (or Discord)
                    RowLayout {
                        id: signIn
                        readonly property bool _code: CommunityService.codeEmail !== ""
                        visible: !CommunityService.signedIn && !CommunityService.connecting
                        Layout.fillWidth: true
                        Layout.leftMargin: 48
                        spacing: 10

                        function submit() {
                            if (signIn._code) {
                                if (codeField.text.length === 6) CommunityService.verifyCode(codeField.text)
                            } else if (emailField.text.trim().indexOf("@") > 0) {
                                CommunityService.sendCode(emailField.text)
                            }
                        }

                        SearchField {
                            id: emailField
                            visible: !signIn._code
                            Layout.preferredWidth: 280
                            glyph: Icons.mail
                            placeholder: qsTr("E-posta adresin")
                            maximumLength: 254
                            onAccepted: signIn.submit()
                        }
                        SearchField {
                            id: codeField
                            visible: signIn._code
                            Layout.preferredWidth: 170
                            glyph: Icons.lock
                            placeholder: qsTr("6 haneli kod")
                            maximumLength: 6
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                            onAccepted: signIn.submit()
                            // The sixth digit signs in, as code fields do
                            onTextChanged: if (text.length === 6) signIn.submit()
                        }
                        AppButton {
                            style: "filled"
                            icon: signIn._code ? Icons.check : Icons.send
                            text: signIn._code ? qsTr("Giriş yap") : qsTr("Kod gönder")
                            busy: CommunityService.signingIn
                            enabled: signIn._code ? codeField.text.length === 6 : emailField.text.trim().indexOf("@") > 0
                            onClicked: signIn.submit()
                        }
                        AppButton {
                            icon: Icons.mail
                            visible: signIn._code
                            style: "plain"
                            text: qsTr("Adresi değiştir")
                            onClicked: {
                                CommunityService.resetCode()
                                emailField.focusAtEnd()
                            }
                        }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            visible: !signIn._code
                            style: "gray"
                            icon: Icons.discord
                            text: qsTr("Discord ile bağlan")
                            onClicked: CommunityService.connectAccount()
                        }

                        Connections {
                            target: CommunityService
                            function onEmailSignInChanged() {
                                // The code is on its way: its field, ready to type
                                if (signIn._code && !codeField.editing) {
                                    codeField.clear()
                                    codeField.focusField()
                                }
                            }
                        }
                    }

                    // Signed in, no name yet: how the wall will show this member
                    RowLayout {
                        visible: composer._needName
                        Layout.fillWidth: true
                        spacing: 14
                        IconTile {
                            Layout.alignment: Qt.AlignTop
                            size: 34
                            glyph: Icons.circleUser
                            tint: Theme.primary
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: qsTr("Toplulukta görünecek adını seç")
                                font.pixelSize: Dimensions.headline
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                                text: qsTr("Paylaşımların bu adla görünür; e-posta adresin hiçbir yerde görünmez. 2–32 karakter: harf, rakam, boşluk ve . _ -")
                                font.pixelSize: Dimensions.callout
                                color: Theme.textSecondary
                            }
                        }
                    }
                    RowLayout {
                        visible: composer._needName
                        Layout.fillWidth: true
                        Layout.leftMargin: 48
                        spacing: 10
                        SearchField {
                            id: nameField
                            Layout.preferredWidth: 260
                            glyph: Icons.user
                            placeholder: qsTr("Adın")
                            maximumLength: 32
                            onAccepted: if (text.trim().length >= 2) CommunityService.setName(text)
                        }
                        AppButton {
                            style: "filled"
                            icon: Icons.check
                            text: qsTr("Kaydet")
                            busy: CommunityService.signingIn
                            enabled: nameField.text.trim().length >= 2
                            onClicked: CommunityService.setName(nameField.text)
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Connected: the composer
                    RowLayout {
                        visible: CommunityService.signedIn && !composer._needName
                        Layout.fillWidth: true
                        spacing: 12
                        Avatar {
                            Layout.alignment: Qt.AlignTop
                            person: CommunityService.member
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Math.max(76, editor.implicitHeight + 20)
                            radius: Dimensions.radiusLG
                            color: editor.activeFocus ? Theme.fillSecondary : Theme.fillTertiary
                            border.width: editor.activeFocus ? 1 : 0
                            border.color: Theme.withAlpha(Theme.primary, 0.55)
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            TextArea {
                                id: editor
                                anchors.fill: parent
                                anchors.margins: 4
                                wrapMode: TextArea.Wrap
                                placeholderText: community._newType === "soru" ? qsTr("Ne sormak istersin?")
                                               : community._newType === "istek" ? qsTr("Hangi oyunun Türkçe yamasını istiyorsun, neden?")
                                               : community._newType === "arkadas" ? qsTr("Hangi oyunda, nasıl bir oyun arkadaşı arıyorsun?")
                                               : qsTr("Ne paylaşmak istersin?")
                                placeholderTextColor: Theme.textTertiary
                                color: Theme.textPrimary
                                selectionColor: Theme.primary40
                                font.pixelSize: Dimensions.body
                                background: null
                                // The site's limit
                                onTextChanged: if (length > 500) remove(500, length)
                            }
                        }
                    }
                    RowLayout {
                        visible: CommunityService.signedIn && !composer._needName
                        Layout.fillWidth: true
                        Layout.leftMargin: 48
                        spacing: 10
                        SegmentedControl {
                            small: true
                            compact: true
                            model: community._postTypes
                            currentValue: community._newType
                            onActivated: (value) => community._newType = value
                        }
                        SearchField {
                            id: topic
                            Layout.preferredWidth: 200
                            placeholder: community._newType === "istek" ? qsTr("Oyun (gerekli)") : qsTr("Oyun (isteğe bağlı)")
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            textFormat: Text.PlainText
                            text: editor.length + " / 500"
                            font.pixelSize: Dimensions.footnote
                            font.features: { "tnum": 1 }
                            color: editor.length > 460 ? Theme.warning : Theme.textMuted
                        }
                        AppButton {
                            style: "filled"
                            icon: Icons.send
                            text: qsTr("Paylaş")
                            busy: CommunityService.posting
                            enabled: editor.text.trim().length >= 2
                                     && (community._newType !== "istek" || topic.text.trim() !== "")
                            onClicked: CommunityService.post(editor.text, community._newType, topic.text)
                        }
                    }

                    // What came of the last post, like or vote
                    Text {
                        id: noticeLine
                        Layout.fillWidth: true
                        Layout.leftMargin: CommunityService.signedIn ? 48 : 0
                        visible: CommunityService.notice !== ""
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        text: CommunityService.notice
                        font.pixelSize: Dimensions.footnote
                        font.weight: Font.Medium
                        color: Theme.primaryText
                        opacity: visible ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
                        Timer {
                            running: noticeLine.visible
                            interval: 7000
                            onTriggered: CommunityService.clearNotice()
                        }
                    }
                }

                Connections {
                    target: CommunityService
                    function onPosted() {
                        editor.text = ""
                        topic.clear()
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                columns: community._wide ? 2 : 1
                columnSpacing: 18
                rowSpacing: 18

                // ── The posts ────────────────────────────────────────────
                Card {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: feed.implicitHeight + 18
                    radius: Dimensions.radiusSection
                    glow: true

                    ColumnLayout {
                        id: feed
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 16
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 20
                            Layout.rightMargin: 16
                            Layout.bottomMargin: 14
                            spacing: 12
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: qsTr("Son paylaşımlar")
                                font.pixelSize: Dimensions.title3
                                font.weight: Font.Bold
                                color: Theme.textPrimary
                            }
                            SegmentedControl {
                                small: true
                                compact: true
                                model: community._types
                                currentValue: CommunityService.type
                                onActivated: (value) => CommunityService.refresh(value)
                            }
                        }

                        Repeater {
                            model: CommunityService.posts

                            ColumnLayout {
                                id: entry
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 0
                                FadeLine { lit: true }
                                Post {
                                    Layout.fillWidth: true
                                    post: entry.modelData
                                    order: entry.index
                                }
                            }
                        }

                        // Loading, failed, nothing of this type
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 28
                            Layout.bottomMargin: 28
                            visible: CommunityService.posts.length === 0
                            spacing: 10

                            Spinner {
                                Layout.alignment: Qt.AlignHCenter
                                visible: CommunityService.loading
                                size: 22
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                visible: !CommunityService.loading
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
                                text: CommunityService.error !== "" ? CommunityService.error
                                                                    : qsTr("Bu türde henüz paylaşım yok. İlkini sen yap.")
                                font.pixelSize: Dimensions.callout
                                color: Theme.textMuted
                            }
                        }

                        FadeLine { lit: true; visible: CommunityService.posts.length > 0 }
                        AppButton {
                            icon: Icons.globe
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: 8
                            style: "plain"
                            text: qsTr("Bütün paylaşımlar sitede")
                            trailingIcon: Icons.arrowUpRight
                            onClicked: Qt.openUrlExternally(community._site)
                        }
                    }
                }

                // ── Beside the posts ─────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: !community._wide
                    Layout.preferredWidth: community._wide ? 300 : -1
                    Layout.alignment: Qt.AlignTop
                    spacing: 18

                    Rail {
                        title: qsTr("En çok istenen yamalar")
                        note: qsTr("Ekip bu listeyi okuyor. Sitede oy verebilir ya da yeni bir oyun isteyebilirsin.")
                        visible: (community._meta.wanted ?? []).length > 0

                        Repeater {
                            model: community._meta.wanted ?? []

                            RowLayout {
                                id: wish
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 64
                                    Layout.preferredHeight: 30
                                    radius: 4
                                    color: Theme.surfaceLight
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        source: wish.modelData.art ?? ""
                                        fillMode: Image.PreserveAspectCrop
                                        sourceSize: Qt.size(128, 60)
                                        asynchronous: true
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: wish.modelData.name ?? ""
                                        elide: Text.ElideRight
                                        font.pixelSize: Dimensions.callout
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }
                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 4
                                            radius: 2
                                            color: Theme.fillTertiary
                                            Rectangle {
                                                width: parent.width * (wish.modelData.progress ?? 0)
                                                height: parent.height
                                                radius: 2
                                                color: Theme.primary
                                            }
                                        }
                                        Text {
                                            textFormat: Text.PlainText
                                            text: qsTr("%1/%2 oy").arg(wish.modelData.votes ?? 0).arg(wish.modelData.target ?? 0)
                                            font.pixelSize: Dimensions.footnote
                                            font.features: { "tnum": 1 }
                                            color: Theme.textMuted
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rail {
                        title: qsTr("Cevap bekleyen sorular")
                        note: qsTr("Bildiğin bir şey varsa sitede cevaplayabilirsin.")
                        visible: (community._meta.open ?? []).length > 0

                        Repeater {
                            model: community._meta.open ?? []

                            ColumnLayout {
                                id: question
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: question.modelData.body ?? ""
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.callout
                                    color: Theme.textPrimary
                                }
                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: [question.modelData.name, question.modelData.topicName, question.modelData.when]
                                          .filter(s => !!s).join("  ·  ")
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.footnote
                                    color: Theme.textMuted
                                }
                            }
                        }
                    }

                    Rail {
                        title: qsTr("Bu ayın en aktifleri")
                        note: qsTr("Paylaşım +10, yanıt +5, aldığın beğeni +2, anket oyu +1 puan.")
                        visible: (community._meta.leaders ?? []).length > 0

                        Repeater {
                            model: community._meta.leaders ?? []

                            RowLayout {
                                id: leader
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    Layout.preferredWidth: 14
                                    textFormat: Text.PlainText
                                    text: leader.index + 1
                                    font.pixelSize: Dimensions.callout
                                    font.weight: Font.Bold
                                    font.features: { "tnum": 1 }
                                    color: leader.index === 0 ? Theme.warning : Theme.textMuted
                                }
                                Avatar { person: leader.modelData; size: 28 }
                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: leader.modelData.name ?? ""
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.callout
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: qsTr("+%1").arg(leader.modelData.gained ?? 0)
                                    font.pixelSize: Dimensions.footnote
                                    font.weight: Font.DemiBold
                                    font.features: { "tnum": 1 }
                                    color: Theme.success
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    TopDissolve {
        flick: flick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    // ── Parts ────────────────────────────────────────────────────────────

    // A member's picture, or their initials on the accent
    component Avatar: Rectangle {
        id: avatar
        property var person: ({})
        property real size: 36
        width: size
        height: size
        radius: size / 2
        color: Theme.withAlpha(Theme.primary, 0.22)

        Text {
            anchors.centerIn: parent
            visible: avatarImg.status !== Image.Ready
            textFormat: Text.PlainText
            text: avatar.person.initials ?? "?"
            font.pixelSize: Math.round(avatar.size * 0.38)
            font.weight: Font.DemiBold
            color: Theme.primaryText
        }
        Image {
            id: avatarImg
            anchors.fill: parent
            visible: status === Image.Ready
            source: avatar.person.avatarUrl ?? ""
            sourceSize: Qt.size(avatar.size * 2, avatar.size * 2)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            layer.enabled: visible
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: avatarMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }
        }
        Item {
            id: avatarMask
            anchors.fill: parent
            visible: false
            layer.enabled: true
            Rectangle { anchors.fill: parent; radius: width / 2; color: "white" }
        }
    }

    // One post: who, when, what kind, about which game; its text; its likes
    // and replies. A click opens it on the site.
    component Post: Item {
        id: postItem
        property var post: ({})
        // Its place in the list: the later, the later it rises in
        property int order: 0
        property bool _replying: false
        implicitHeight: postRow.implicitHeight + 28

        opacity: 0
        transform: Translate { id: rise; y: 16 }
        Component.onCompleted: arrive.start()
        SequentialAnimation {
            id: arrive
            PauseAnimation { duration: Dimensions.animSlow > 0 ? Math.min(postItem.order, 8) * 45 : 0 }
            ParallelAnimation {
                NumberAnimation { target: postItem; property: "opacity"; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
                NumberAnimation { target: rise; property: "y"; to: 0; duration: Dimensions.animSlow + 120; easing.type: Easing.OutExpo }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            radius: Dimensions.radiusMD
            color: postHover.hovered ? Theme.fillQuaternary : "transparent"
        }

        RowLayout {
            id: postRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 14
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 12

            Avatar {
                Layout.alignment: Qt.AlignTop
                person: postItem.post.author ?? ({})
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        textFormat: Text.PlainText
                        text: postItem.post.author ? postItem.post.author.name : ""
                        font.pixelSize: Dimensions.body
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    StatusBadge {
                        visible: postItem.post.team === true
                        text: qsTr("Ekip")
                        tint: Theme.primaryText
                        showDot: false
                    }
                    StatusBadge {
                        text: postItem.post.typeLabel ?? ""
                        tint: community._typeTint(postItem.post.type ?? "")
                        showDot: false
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: [postItem.post.topicName, postItem.post.when].filter(s => !!s).join("  ·  ")
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.footnote
                        color: Theme.textMuted
                    }
                }

                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: postItem.post.body ?? ""
                    wrapMode: Text.WordWrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                    lineHeight: 1.3
                    font.pixelSize: Dimensions.body
                    color: Theme.textSecondary
                }

                // A poll: each option's share; a click votes, signed in
                Repeater {
                    model: postItem.post.pollRows ?? []
                    Item {
                        id: option
                        required property var modelData
                        required property int index
                        readonly property bool _canVote: CommunityService.signedIn && !(postItem.post.pollClosed ?? false)
                        Layout.fillWidth: true
                        Layout.topMargin: 2
                        implicitHeight: 30
                        Rectangle {
                            anchors.fill: parent
                            radius: Dimensions.radiusMD
                            color: optionHover.hovered && option._canVote ? Theme.fillTertiary : Theme.fillQuaternary
                            border.width: option.modelData.mine ? 1 : 0
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                        }
                        Rectangle {
                            width: parent.width * (option.modelData.share ?? 0)
                            height: parent.height
                            radius: Dimensions.radiusMD
                            color: Theme.withAlpha(Theme.primary, option.modelData.mine ? 0.42 : 0.26)
                            Behavior on width { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                        }
                        HoverHandler { id: optionHover; enabled: option._canVote; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: option._canVote
                            onTapped: CommunityService.vote(postItem.post.id, option.index)
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.PlainText
                            text: option.modelData.text ?? ""
                            font.pixelSize: Dimensions.callout
                            color: Theme.textPrimary
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.PlainText
                            text: "%" + Math.round((option.modelData.share ?? 0) * 100)
                            font.pixelSize: Dimensions.footnote
                            font.features: { "tnum": 1 }
                            color: Theme.textMuted
                        }
                    }
                }

                // Hearts and replies: they act once the launcher is connected
                Row {
                    Layout.topMargin: 2
                    spacing: 6
                    PostAction {
                        glyph: Icons.heart
                        filled: postItem.post.liked === true
                        tint: postItem.post.liked === true ? Theme.turkishRed : Theme.textMuted
                        count: postItem.post.likes ?? 0
                        enabled: CommunityService.signedIn
                        tip: CommunityService.signedIn ? qsTr("Beğen") : qsTr("Beğenmek için hesabını bağla")
                        onClicked: CommunityService.like(postItem.post.id)
                    }
                    PostAction {
                        glyph: Icons.messageCircle
                        filled: postItem._replying
                        tint: postItem._replying ? Theme.primaryText : Theme.textMuted
                        count: postItem.post.replyCount ?? 0
                        enabled: CommunityService.signedIn
                        tip: CommunityService.signedIn ? qsTr("Yanıtla") : qsTr("Yanıtlamak için hesabını bağla")
                        onClicked: postItem._replying = !postItem._replying
                    }
                }

                // The replies, one level, under a line
                Repeater {
                    model: postItem.post.replies ?? []
                    RowLayout {
                        id: answer
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 10
                        Rectangle {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 2
                            radius: 1
                            color: Theme.separatorStrong
                        }
                        Avatar { Layout.alignment: Qt.AlignTop; person: answer.modelData.author ?? ({}); size: 24 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: [answer.modelData.author ? answer.modelData.author.name : "", answer.modelData.when]
                                      .filter(s => !!s).join("  ·  ")
                                elide: Text.ElideRight
                                font.pixelSize: Dimensions.footnote
                                font.weight: Font.DemiBold
                                color: Theme.textSecondary
                            }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: answer.modelData.body ?? ""
                                wrapMode: Text.WordWrap
                                font.pixelSize: Dimensions.callout
                                color: Theme.textSecondary
                            }
                        }
                    }
                }

                // Writing a reply
                RowLayout {
                    visible: postItem._replying
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 8
                    SearchField {
                        id: replyField
                        Layout.fillWidth: true
                        placeholder: qsTr("Yanıtın…")
                        onAccepted: sendReply.clicked()
                    }
                    AppButton {
                        id: sendReply
                        style: "filled"
                        size: "small"
                        icon: Icons.send
                        busy: CommunityService.posting
                        enabled: replyField.text.trim().length >= 2
                        onClicked: {
                            CommunityService.reply(postItem.post.id, replyField.text.slice(0, 500))
                            replyField.clear()
                            postItem._replying = false
                        }
                    }
                }
            }
        }

        HoverHandler { id: postHover }
    }

    // A small icon-and-count button under a post (heart, replies)
    component PostAction: Item {
        id: act
        property string glyph: ""
        property bool filled: false
        property color tint: Theme.textMuted
        property int count: 0
        property string tip: ""
        signal clicked()

        implicitWidth: actRow.implicitWidth + 16
        implicitHeight: 26

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: actHover.hovered && act.enabled ? Theme.fillTertiary : "transparent"
        }
        Row {
            id: actRow
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            Icon {
                id: actIcon
                anchors.verticalCenter: parent.verticalCenter
                glyph: act.glyph
                font.family: act.filled ? Icons.symbolsFamily : Icons.family
                size: 14
                color: act.tint
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: act.count
                font.pixelSize: Dimensions.footnote
                font.features: { "tnum": 1 }
                color: act.tint
            }
        }
        // A heart that pops when it turns on
        onFilledChanged: if (filled) pop.restart()
        SequentialAnimation {
            id: pop
            NumberAnimation { target: actIcon; property: "scale"; to: 1.35; duration: Dimensions.animFast; easing.type: Easing.OutCubic }
            NumberAnimation { target: actIcon; property: "scale"; to: 1.0; duration: Dimensions.animNormal; easing.type: Easing.OutBack }
        }

        HoverHandler { id: actHover; enabled: act.enabled; cursorShape: Qt.PointingHandCursor }
        TapHandler { enabled: act.enabled; onTapped: act.clicked() }
        StyledToolTip { visible: actHover.hovered && act.tip !== ""; text: act.tip }
    }

    // A card beside the posts: its title, its rows, a line under them
    component Rail: Card {
        id: rail
        property string title: ""
        property string note: ""
        default property alias rows: railRows.data

        Layout.fillWidth: true
        implicitHeight: railCol.implicitHeight + 34
        radius: Dimensions.radiusSection

        ColumnLayout {
            id: railCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 12

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: rail.title
                font.pixelSize: Dimensions.headline
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            ColumnLayout {
                id: railRows
                Layout.fillWidth: true
                spacing: 12
            }
            Text {
                Layout.fillWidth: true
                visible: rail.note !== ""
                textFormat: Text.PlainText
                text: rail.note
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                color: Theme.textTertiary
            }
        }
    }
}
