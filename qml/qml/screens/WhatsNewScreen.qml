// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * WhatsNewScreen.qml - "Yenilikler": the launcher's own news
 *
 * Whether a newer launcher is out and how to get it (the Store's page in the
 * Store build, the download page for the ZIP), then one timeline that rises
 * like a branch graph: the plan at the top on a dashed line (later, next,
 * what we are on), the version in use glowing below it, older versions
 * further down; each change branches off the trunk in its kind's colour.
 * The nodes rise into place one after another when the page opens. Opens by
 * itself on the first start after an update. Notes and plan come from
 * UpdateService: built into the app (qml/resources/whatsnew.json) and
 * refreshed from the CDN's copy.
 */
Item {
    id: news

    property real contentMargin: 16

    function scrollToTop() { newsFlick.contentY = 0 }

    readonly property int _state: UpdateService.state
    readonly property bool _update: _state !== UpdateService.Idle && _state !== UpdateService.Checking
    readonly property bool _failed: _state === UpdateService.Idle && UpdateService.error !== ""

    onVisibleChanged: if (visible) { UpdateService.markNotesSeen(); revealTick++ }
    Component.onCompleted: if (visible) UpdateService.markNotesSeen()

    function _kindLabel(type) {
        return type === "new" ? qsTr("Yeni") : type === "fixed" ? qsTr("Düzeldi") : qsTr("İyileşti")
    }
    function _kindTint(type) {
        return type === "new" ? Theme.primary : type === "fixed" ? Theme.success : Theme.info
    }

    // ── Small parts ──────────────────────────────────────────────────────────
    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 12
        textFormat: Text.PlainText
        font.family: Dimensions.fontDisplay
        font.pixelSize: Dimensions.title2
        font.weight: Font.Bold
        font.letterSpacing: Dimensions.trackingHeadline
        color: Theme.textPrimary
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        lineHeight: 1.3
        font.pixelSize: Dimensions.body
        color: Theme.textSecondary
    }

    // ── Timeline ─────────────────────────────────────────────────────────────
    // x of the trunk in a node, and where a node's content starts
    readonly property real _trunkX: 15
    readonly property real _gutter: 44
    // Bumped each time the page shows: the nodes rise in again
    property int revealTick: 0

    // One stop on the trunk: the line through its gutter, its dot, its
    // content to the right. `future` draws the line dashed and the dot
    // hollow; `current` lights the dot. The first node's line starts at its
    // dot, the last one's ends there.
    component Node: Item {
        id: node
        property bool future: false
        property bool current: false
        property bool first: false
        property bool last: false
        property color tint: Theme.primary
        property int order: 0
        // A stage of the plan wears its sign on the trunk instead of a ring
        property string glyph: ""
        // Its branches shown (Fold); the head opens and shuts it
        property bool open: true
        default property alias content: body.data

        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + 30

        readonly property real _dotY: 13

        // The trunk
        Shape {
            id: trunk
            x: news._trunkX - 1
            y: node.first ? node._dotY : 0
            width: 2
            height: node.last ? node._dotY - y : node.height - y
            visible: height > 0
            ShapePath {
                strokeColor: node.future ? Theme.withAlpha(Theme.textMuted, 0.45) : Theme.withAlpha(node.tint, 0.45)
                strokeWidth: 2
                strokeStyle: node.future ? ShapePath.DashLine : ShapePath.SolidLine
                dashPattern: [2, 3]
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                startX: 1; startY: 0
                PathLine { x: 1; y: trunk.height }
            }
        }

        // The dot, and the version in use lit
        RectangularShadow {
            visible: node.current
            x: news._trunkX - 9; y: node._dotY - 9
            width: 18; height: 18
            radius: 9
            blur: 14
            spread: 2
            color: Theme.withAlpha(node.tint, 0.7)
        }
        Rectangle {
            visible: node.glyph !== ""
            x: news._trunkX - width / 2
            y: node._dotY - height / 2
            width: 26; height: 26
            radius: 13
            color: Qt.tint(Theme.bgPrimary, Theme.withAlpha(node.tint, 0.16))
            border.width: 1.5
            border.color: Theme.withAlpha(node.tint, 0.8)
            Icon {
                anchors.centerIn: parent
                glyph: node.glyph
                size: 14
                color: node.tint
            }
        }
        Rectangle {
            visible: node.glyph === ""
            x: news._trunkX - width / 2
            y: node._dotY - height / 2
            width: node.current ? 16 : 12
            height: width
            radius: width / 2
            color: node.future ? Theme.bgPrimary : node.tint
            border.width: node.future ? 2 : node.current ? 3 : 0
            border.color: node.future ? Theme.withAlpha(node.tint, 0.8) : Qt.rgba(1, 1, 1, 0.85)
        }

        ColumnLayout {
            id: body
            x: news._gutter
            width: node.width - news._gutter
            spacing: 8
        }

        // Rising into place, top first, one after another
        opacity: 0
        transform: Translate { id: lift; y: 28 }
        function _arrive() {
            if (Dimensions.animSlow === 0) { node.opacity = 1; lift.y = 0; return }
            node.opacity = 0; lift.y = 28
            arrive.restart()
        }
        Component.onCompleted: _arrive()
        Connections { target: news; function onRevealTickChanged() { node._arrive() } }
        SequentialAnimation {
            id: arrive
            PauseAnimation { duration: Math.min(node.order, 10) * 70 }
            ParallelAnimation {
                NumberAnimation { target: node; property: "opacity"; to: 1; duration: 380; easing.type: Easing.OutCubic }
                NumberAnimation { target: lift; property: "y"; to: 0; duration: 620; easing.type: Easing.OutExpo }
            }
        }
    }

    // A change (or a plan item) branching off the trunk: a curve from the
    // trunk into a small dot in its kind's colour, then its words
    component Branch: RowLayout {
        id: branch
        property color tint: Theme.primary
        property string label: ""
        property string text: ""
        property string detail: ""
        property bool dashed: false

        Layout.fillWidth: true
        spacing: 10

        Item {
            Layout.preferredWidth: 10
            Layout.alignment: Qt.AlignTop
            implicitHeight: 18
            // From the trunk (left, out of this row) down and right into the dot
            Shape {
                x: -(news._gutter - news._trunkX) - 10 + 10
                y: -12
                width: news._gutter - news._trunkX
                height: 21
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Theme.withAlpha(branch.tint, 0.55)
                    strokeWidth: 1.5
                    strokeStyle: branch.dashed ? ShapePath.DashLine : ShapePath.SolidLine
                    dashPattern: [2, 3]
                    fillColor: "transparent"
                    startX: 0; startY: 0
                    PathQuad { x: 12; y: 21; controlX: 0; controlY: 21 }
                    PathLine { x: news._gutter - news._trunkX + 2; y: 21 }
                }
            }
            Rectangle {
                x: 2; y: 5
                width: 8; height: 8; radius: 4
                color: branch.tint
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    visible: branch.label !== ""
                    Layout.alignment: Qt.AlignTop
                    textFormat: Text.PlainText
                    text: branch.label
                    font.pixelSize: Dimensions.footnote
                    font.weight: Font.DemiBold
                    color: branch.tint
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                    text: branch.text
                    font.pixelSize: Dimensions.callout
                    font.weight: branch.detail !== "" ? Font.DemiBold : Font.Normal
                    color: branch.detail !== "" ? Theme.textPrimary : Theme.textSecondary
                }
            }
            Text {
                Layout.fillWidth: true
                visible: branch.detail !== ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                lineHeight: 1.25
                text: branch.detail
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
            }
        }
    }

    // A stop's heading: a title, a line under it, badges and a date
    component NodeHead: ColumnLayout {
        id: head
        property string title: ""
        property string subtitle: ""
        property string date: ""
        property bool large: false
        // Folds its node: the whole head is the switch, a count and a
        // chevron on its right say what is inside and which way it goes
        property bool foldable: false
        property bool open: true
        property string countText: ""
        signal toggled()
        default property alias badges: badgeRow.data
        Layout.fillWidth: true
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            RowLayout {
                id: badgeRow
                spacing: 10
                Text {
                    textFormat: Text.PlainText
                    text: head.title
                    font.family: Dimensions.fontDisplay
                    font.pixelSize: head.large ? Dimensions.title2 : Dimensions.title3
                    font.weight: Font.Bold
                    font.letterSpacing: Dimensions.trackingHeadline
                    color: Theme.textPrimary
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                visible: head.foldable && head.countText !== ""
                implicitWidth: countLabel.implicitWidth + 18
                implicitHeight: 24
                radius: 12
                color: headHover.hovered ? Theme.fillSecondary : Theme.fillTertiary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: head.countText
                    font.pixelSize: Dimensions.caption1
                    font.family: Dimensions.fontText
                    font.weight: Font.DemiBold
                    color: Theme.textSecondary
                }
            }
            Rectangle {
                visible: head.foldable
                implicitWidth: 28
                implicitHeight: 28
                radius: 14
                color: head.open ? Theme.withAlpha(Theme.primary, headHover.hovered ? 0.22 : 0.14)
                     : headHover.hovered ? Theme.fillSecondary : Theme.fillTertiary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Icon {
                    anchors.centerIn: parent
                    glyph: Icons.chevronDown
                    size: 15
                    color: head.open ? Theme.primaryText : Theme.textSecondary
                    rotation: head.open ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutBack } }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: head.subtitle !== "" || head.date !== ""
            spacing: 8
            Text {
                Layout.fillWidth: true
                visible: head.subtitle !== ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                text: head.subtitle
                font.pixelSize: Dimensions.callout
                font.weight: Font.Medium
                color: Theme.textSecondary
            }
            Text {
                visible: head.date !== ""
                textFormat: Text.PlainText
                text: head.date
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }
        }

        HoverHandler {
            id: headHover
            enabled: head.foldable
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            enabled: head.foldable
            onTapped: head.toggled()
        }
    }

    // What a node holds, folding away: the room for it grows and shrinks, the
    // branches fade in after it. It reaches into the gutter and a little
    // above, so the curves from the trunk are inside what it clips.
    component Fold: Item {
        id: fold
        property bool open: true
        default property alias content: foldCol.data
        readonly property real _reach: 14
        Layout.fillWidth: true
        Layout.leftMargin: -news._gutter
        Layout.topMargin: -fold._reach
        implicitHeight: fold.open ? foldCol.implicitHeight + fold._reach : 0
        Behavior on implicitHeight { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        clip: true

        ColumnLayout {
            id: foldCol
            x: news._gutter
            y: fold._reach + (fold.open ? 0 : -10)
            width: fold.width - news._gutter
            spacing: 8
            opacity: fold.open ? 1 : 0
            Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutExpo } }
            Behavior on opacity { NumberAnimation { duration: fold.open ? Dimensions.animSlow : Dimensions.animFast } }
        }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: newsFlick
        anchors.fill: parent
        contentHeight: page.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: newsFlick.parent
            anchors.top: newsFlick.top; anchors.right: newsFlick.right; anchors.bottom: newsFlick.bottom
        }

        ColumnLayout {
            id: page
            width: Math.min(newsFlick.width - Dimensions.pageMargin * 2, Dimensions.contentMaxWidth)
            x: (newsFlick.width - width) / 2
            spacing: 16

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Yenilikler")
                subtitle: qsTr("Launcher'ın her sürümünde neyin değiştiği ve sırada ne olduğu. Yeni bir sürüm çıktığında burada ve kenar çubuğunun altında haber veririz.")
            }

            // ── This copy: up to date, or how to get the new one ──
            Card {
                Layout.fillWidth: true
                implicitHeight: statusRow.implicitHeight + 36
                radius: Dimensions.radiusLG
                glow: news._update

                RowLayout {
                    id: statusRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 14

                    IconTile {
                        Layout.alignment: Qt.AlignTop
                        size: 40
                        glyph: news._update ? Icons.circleArrowDown : news._failed ? Icons.circleAlert : Icons.circleCheck
                        tint: news._update ? Theme.primary : news._failed ? Theme.warning : Theme.success
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            text: {
                                switch (news._state) {
                                case UpdateService.Checking:    return qsTr("Yeni sürüm var mı diye bakılıyor…")
                                case UpdateService.Available:   return qsTr("Yeni sürüm çıktı: %1").arg(UpdateService.version)
                                case UpdateService.Downloading: return qsTr("İndiriliyor… %%1").arg(Math.round(UpdateService.progress * 100))
                                case UpdateService.Verifying:   return qsTr("İndirilen dosya doğrulanıyor…")
                                case UpdateService.Ready:       return qsTr("Güncelleme hazır")
                                case UpdateService.Installing:  return qsTr("Güncelleme kuruluyor…")
                                default: return news._failed ? qsTr("Yeni sürüme bakılamadı") : qsTr("Güncelsin")
                                }
                            }
                            font.pixelSize: Dimensions.headline
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            lineHeight: 1.25
                            text: {
                                switch (news._state) {
                                case UpdateService.Available: return UpdateService.actionHint
                                case UpdateService.Ready:     return qsTr("Launcher kapanıp yeni sürümle açılır; kurulu yamalarına dokunulmaz.")
                                case UpdateService.Idle:
                                    if (news._failed)
                                        return qsTr("%1. İnternet bağlantını kontrol edip yeniden dene; kurulu yamaların bundan etkilenmez.").arg(UpdateService.error)
                                    return UpdateService.storeBuild
                                        ? qsTr("Sürüm %1. Yeni sürümler Microsoft Store'dan gelir; çıktığında burada ve kenar çubuğunun altında görürsün.").arg(Dimensions.appVersion)
                                        : qsTr("Sürüm %1. Launcher açılışta ve gün içinde yeni sürüm olup olmadığına kendisi bakar.").arg(Dimensions.appVersion)
                                default: return qsTr("Sürüm %1").arg(Dimensions.appVersion)
                                }
                            }
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                        }
                    }
                    Spinner {
                        visible: news._state === UpdateService.Checking || news._state === UpdateService.Installing
                    }
                    AppButton {
                        visible: news._state === UpdateService.Idle
                        style: "gray"
                        icon: Icons.refreshCw
                        text: news._failed ? qsTr("Tekrar dene") : qsTr("Denetle")
                        onClicked: UpdateService.check()
                    }
                    AppButton {
                        icon: Icons.download
                        visible: UpdateService.actionable
                        style: news._state === UpdateService.Available || news._state === UpdateService.Ready ? "filled" : "gray"
                        text: UpdateService.actionText
                        trailingIcon: UpdateService.opensPage ? Icons.arrowUpRight : ""
                        onClicked: {
                            switch (news._state) {
                            case UpdateService.Available: UpdateService.download(); break
                            case UpdateService.Ready: UpdateService.install(); break
                            case UpdateService.Downloading:
                            case UpdateService.Verifying: UpdateService.cancel(); break
                            }
                        }
                    }
                }
            }

            // ── The timeline: the plan above, the versions below ──
            SectionTitle { text: qsTr("Yol haritası") }
            Body {
                text: qsTr("Yukarısı gelecek: kesik çizgide sıraya aldıklarımız ve üzerinde çalıştıklarımız. Parlayan nokta kullandığın sürüm; aşağı indikçe eski sürümler ve her birinde neyin değiştiği. Plan Discord'daki isteklerle ve hata raporlarıyla değişebilir; tarih vermiyoruz, hazır olanı yayınlıyoruz.")
            }

            ColumnLayout {
                id: timeline
                Layout.fillWidth: true
                Layout.topMargin: 10
                spacing: 0

                readonly property var _stages: [
                    { status: "later", title: qsTr("Daha sonra"), tint: Theme.textSecondary, glyph: Icons.calendarClock },
                    { status: "next", title: qsTr("Sırada"), tint: Theme.info, glyph: Icons.listOrdered },
                    { status: "doing", title: qsTr("Üzerinde çalışıyoruz"), tint: Theme.primary, glyph: Icons.wrench }
                ].filter(st => UpdateService.roadmap.some(i => i.status === st.status))

                // The future, furthest first
                Repeater {
                    model: timeline._stages
                    delegate: Node {
                        id: stageNode
                        required property var modelData
                        required property int index
                        future: true
                        first: index === 0
                        tint: modelData.tint
                        glyph: modelData.glyph
                        order: index
                        // What is under way shows; the rest opens on a click
                        open: modelData.status === "doing"
                        readonly property var _items: UpdateService.roadmap.filter(i => i.status === stageNode.modelData.status)

                        NodeHead {
                            title: stageNode.modelData.title
                            foldable: true
                            open: stageNode.open
                            countText: qsTr("%1 başlık").arg(stageNode._items.length)
                            onToggled: stageNode.open = !stageNode.open
                        }
                        Fold {
                            open: stageNode.open
                            Repeater {
                                model: stageNode._items
                                delegate: Branch {
                                    required property var modelData
                                    dashed: true
                                    tint: stageNode.modelData.tint
                                    text: modelData.title
                                    detail: modelData.text || ""
                                }
                            }
                        }
                    }
                }

                // The versions, newest first
                Repeater {
                    model: UpdateService.releases
                    delegate: Node {
                        id: rel
                        required property var modelData
                        required property int index
                        open: index < 2 || modelData.newer === true || modelData.running === true
                        readonly property var _changes: rel.modelData.changes || []
                        first: timeline._stages.length === 0 && index === 0
                        last: index === UpdateService.releases.length - 1
                        current: modelData.running === true
                        tint: modelData.newer === true ? Theme.primary
                            : modelData.running === true ? Theme.success : Theme.primaryText
                        order: timeline._stages.length + index

                        NodeHead {
                            title: qsTr("Sürüm %1").arg(rel.modelData.version)
                            subtitle: rel.modelData.title || ""
                            date: rel.modelData.dateText || ""
                            large: rel.current || rel.modelData.newer === true
                            foldable: rel._changes.length > 0 || (rel.modelData.summary || "") !== ""
                            open: rel.open
                            countText: rel._changes.length > 0 ? qsTr("%1 değişiklik").arg(rel._changes.length) : ""
                            onToggled: rel.open = !rel.open
                            StatusBadge {
                                visible: rel.current
                                text: qsTr("Kullandığın sürüm")
                                tint: Theme.success
                            }
                            StatusBadge {
                                visible: rel.modelData.newer === true
                                text: qsTr("Yeni sürüm")
                                tint: Theme.primary
                            }
                        }
                        Fold {
                            open: rel.open
                            Body {
                                visible: (rel.modelData.summary || "") !== ""
                                text: rel.modelData.summary || ""
                                font.pixelSize: Dimensions.callout
                            }
                            Repeater {
                                model: rel._changes
                                delegate: Branch {
                                    required property var modelData
                                    tint: news._kindTint(modelData.type)
                                    label: news._kindLabel(modelData.type)
                                    text: modelData.text
                                }
                            }
                        }
                    }
                }
            }

            Body {
                Layout.topMargin: 4
                text: qsTr("Bir isteğin ya da bir sorun mu var? Topluluğumuz ve çevirmenlerimiz Discord'da.")
            }
            AppButton {
                Layout.bottomMargin: 8
                style: "gray"
                icon: Icons.discord
                text: qsTr("Discord'da yaz")
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }
        }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: newsFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
