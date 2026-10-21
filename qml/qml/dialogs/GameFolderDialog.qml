// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameFolderDialog.qml - A picked folder that needs a word before it is used
 *
 * GameFolderBar binds a folder that checks out without asking; this opens for
 * the rest: a folder we could not recognise, one that cannot hold the game,
 * and a patch that sits in another folder than its game. It leads with the
 * verdict, then draws the path itself — the part above the game greyed, the
 * game's own folder marked as where the patch goes, a deeper folder the user
 * opened struck through — and the evidence as short chips. Nothing is bound
 * until the button says so.
 */
BaseDialog {
    id: root

    property var report: ({})
    property string gameName: ""
    // install: the user is installing · move: a patch has to change folders · change
    property string intent: "change"

    signal useFolder(string root, string engine)
    signal pickAnother()
    // The patch sits in the wrong folder: take it off, bind the right one, reinstall.
    signal movePatch(string root, string engine)

    readonly property string _verdict: root.report.verdict || "refused"
    readonly property bool _moving: root.report.patchedElsewhere === true
    readonly property bool _canUse: root._moving || (root._verdict !== "refused" && root.report.same !== true)
    readonly property color _tone: root._moving || root._verdict === "ok" ? Theme.primary
                                 : root._verdict === "unsure" ? Theme.warning
                                 : Theme.error

    function _primary() {
        if (!root._canUse) return
        if (root._moving) root.movePatch(root.report.root, root.report.engine || "")
        else root.useFolder(root.report.root, root.report.engine || "")
        root.close()
    }

    // The path as segments: above the game, the game's folder, and the part
    // of the pick below it (or the part the launcher went down into).
    readonly property var _trail: {
        const bs = String.fromCharCode(92)
        const split = s => String(s || "").split(bs).join("/").split("/").filter(t => t !== "")
        const r = split(root.report.rootShown), p = split(root.report.picked)
        if (r.length === 0) return p.map(t => ({ text: t, kind: "base" }))
        let common = 0
        while (common < r.length && common < p.length
               && r[common].toLowerCase() === p[common].toLowerCase()) ++common
        const out = []
        for (let i = 0; i < r.length; ++i)
            out.push({ text: r[i], kind: i === r.length - 1 ? "root" : (i >= common ? "inner" : "base") })
        if (common === r.length)
            for (let j = r.length; j < p.length; ++j) out.push({ text: p[j], kind: "below" })
        return out
    }
    readonly property bool _climbed: root._trail.some(s => s.kind === "below")
    readonly property bool _descended: root._trail.some(s => s.kind === "inner")

    readonly property var _chips: {
        const r = root.report, out = []
        if (!r.root) return out
        const exe = String(r.exe || "")
        out.push(exe !== "" ? { kind: "ok", text: exe.substring(Math.max(exe.lastIndexOf("/"), exe.lastIndexOf(String.fromCharCode(92))) + 1) }
                            : { kind: "warn", text: qsTr("Oyunun .exe dosyası yok") })
        if ((r.filesTotal || 0) > 0)
            out.push(r.filesFound === r.filesTotal
                     ? { kind: "ok", text: qsTr("Değişecek %1 dosya yerinde").arg(r.filesTotal) }
                     : { kind: "warn", text: qsTr("%1 / %2 dosya eksik").arg(r.filesTotal - r.filesFound).arg(r.filesTotal) })
        if ((r.dirsTotal || 0) > 0)
            out.push({ kind: r.dirsFound * 2 >= r.dirsTotal ? "ok" : "warn",
                       text: qsTr("Yama klasörleri %1 / %2").arg(r.dirsFound).arg(r.dirsTotal) })
        if ((r.otherGame || "") !== "" && root._verdict !== "refused")
            out.push({ kind: "warn", text: qsTr("%1 oyununa benziyor").arg(r.otherGame) })
        if (root._verdict !== "refused" && r.writable === false)
            out.push({ kind: "info", text: qsTr("Yönetici izni istenecek") })
        return out
    }

    accentColor: root._tone
    width: Math.min(640, parent ? parent.width - 64 : 640)
    title: root._moving ? qsTr("Yama yanlış klasörde")
         : root._verdict === "ok" ? qsTr("Oyunun klasörü bulundu")
         : root._verdict === "unsure" ? qsTr("Bu klasörden emin olamadık")
         : qsTr("Bu klasöre kurulamaz")

    header: Item {
        implicitHeight: 64

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingXL
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingLG

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                color: Theme.withAlpha(root._tone, 0.14)

                Text {
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 16
                    text: root._moving ? "\uF12B" : root._verdict === "ok" ? "\uE73E"
                        : root._verdict === "unsure" ? "\uE7BA" : "\uE711"
                    color: root._tone
                }
            }

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontXL
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            DialogCloseButton {
                onClicked: { root.cancelled(); root.close() }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Dimensions.spacingXL

        // ── What it means, in a sentence ─────────────────────────────────────
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingXL
            Layout.rightMargin: Dimensions.paddingXL
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            lineHeight: 1.4
            font.pixelSize: Dimensions.fontMD
            color: Theme.textSecondary
            text: root._moving
                  ? qsTr("Oyun aşağıda işaretli klasörde, Türkçe yama ise başka bir klasöre kurulmuş; oyun yamayı görmüyor. Taşıdığında yama oradan kaldırılır ve oyunun klasörüne yeniden kurulur.")
                  : root._verdict === "ok"
                    ? qsTr("Yama aşağıda işaretli klasöre kurulacak.")
                  : root._verdict === "unsure"
                    ? qsTr("%1 için beklediğimiz dosyaları bu klasörde bulamadık. Oyunun klasörü olduğundan eminsen yine de kullanabilirsin; değilsen başka bir klasör seç.")
                          .arg(root.gameName || qsTr("Bu oyun"))
                  : (root.report.problem || qsTr("Bu klasör bir oyunun klasörü olamaz."))
        }

        // ── The path, drawn ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingXL
            Layout.rightMargin: Dimensions.paddingXL
            visible: root._trail.length > 0
            implicitHeight: trailCol.implicitHeight + Dimensions.paddingLG * 2
            radius: Dimensions.radiusLG
            color: Theme.textPrimary04
            border.width: 1
            border.color: Theme.textPrimary08

            ColumnLayout {
                id: trailCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Dimensions.paddingLG
                spacing: Dimensions.spacingMD

                // Moving: where the patch is now, above where it goes.
                Text {
                    visible: root._moving
                    textFormat: Text.PlainText
                    text: qsTr("YAMANIN ŞU ANKİ YERİ")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    color: Theme.warning
                }
                Text {
                    Layout.fillWidth: true
                    visible: root._moving
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    text: root.report.patchedAt || ""
                    font.pixelSize: Dimensions.fontMD
                    font.strikeout: true
                    color: Theme.textMuted
                }
                Text {
                    visible: root._moving
                    Layout.topMargin: Dimensions.spacingXS
                    textFormat: Text.PlainText
                    text: qsTr("TAŞINACAĞI YER")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    color: Theme.accentLight
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 0

                    Repeater {
                        model: root._trail

                        delegate: Row {
                            id: seg
                            required property var modelData
                            required property int index
                            spacing: 0
                            height: 28

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: seg.index > 0
                                textFormat: Text.PlainText
                                text: " " + String.fromCharCode(92) + " "
                                font.pixelSize: Dimensions.fontMD
                                color: Theme.textMuted
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                readonly property bool isRoot: seg.modelData.kind === "root"
                                width: segText.implicitWidth + (isRoot ? 16 : 0)
                                height: isRoot ? 26 : segText.implicitHeight
                                radius: Dimensions.radiusMD
                                color: isRoot ? Theme.withAlpha(root._tone, 0.16) : "transparent"
                                border.width: isRoot ? 1 : 0
                                border.color: Theme.withAlpha(root._tone, 0.45)

                                Text {
                                    id: segText
                                    anchors.centerIn: parent
                                    textFormat: Text.PlainText
                                    text: seg.modelData.text
                                    font.pixelSize: Dimensions.fontMD
                                    font.weight: seg.modelData.kind === "root" ? Font.DemiBold : Font.Normal
                                    font.strikeout: seg.modelData.kind === "below"
                                    color: seg.modelData.kind === "root" ? Theme.textPrimary
                                         : seg.modelData.kind === "inner" ? Theme.textSecondary
                                         : Theme.textMuted
                                    opacity: seg.modelData.kind === "below" ? 0.7 : 1
                                }
                            }
                        }
                    }
                }

                // The legend, only for what the path shows.
                RowLayout {
                    Layout.fillWidth: true
                    visible: (root.report.root || "") !== ""
                    spacing: Dimensions.spacingSM

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 3
                        color: Theme.withAlpha(root._tone, 0.16)
                        border.width: 1
                        border.color: Theme.withAlpha(root._tone, 0.45)
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textMuted
                        text: root._moving ? qsTr("Oyunun kendi klasörü: .exe dosyası burada.")
                              : (root._verdict === "refused" ? qsTr("Bulduğumuz oyun klasörü.")
                                                             : qsTr("Oyunun klasörü; yama buraya kurulur."))
                                + (root._climbed ? " " + qsTr("Seçtiğin klasör oyunun içindeydi (üstü çizili).")
                                   : root._descended ? " " + qsTr("Oyun, seçtiğin klasörün içinde bulundu.")
                                   : "")
                    }
                }
            }
        }

        // ── The evidence ─────────────────────────────────────────────────────
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingXL
            Layout.rightMargin: Dimensions.paddingXL
            visible: root._chips.length > 0
            spacing: Dimensions.spacingMD

            Repeater {
                model: root._chips

                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    readonly property color tint: chip.modelData.kind === "ok" ? Theme.success
                                                : chip.modelData.kind === "warn" ? Theme.warning
                                                : Theme.textSecondary
                    width: chipRow.implicitWidth + 20
                    height: 28
                    radius: 14
                    color: Theme.withAlpha(chip.tint, 0.10)
                    border.width: 1
                    border.color: Theme.withAlpha(chip.tint, 0.28)

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingSM

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.PlainText
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 11
                            text: chip.modelData.kind === "ok" ? "\uE73E"
                                : chip.modelData.kind === "warn" ? "\uE7BA" : "\uE946"
                            color: chip.tint
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.PlainText
                            text: chip.modelData.text
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    // A footer button: filled for the action, quiet for the way out.
    component FooterButton: Rectangle {
        id: fb
        property string text: ""
        property bool filled: false
        property color fill: Theme.primary
        signal clicked()

        Layout.preferredWidth: fbLabel.implicitWidth + Dimensions.paddingXL * 2
        Layout.preferredHeight: 40
        radius: Dimensions.radiusMD
        color: fb.filled ? (fbHover.hovered ? Qt.darker(fb.fill, 1.12) : fb.fill)
                         : (fbHover.hovered ? Theme.textPrimary12 : Theme.textPrimary06)
        border.width: fb.activeFocus ? 2 : 0
        border.color: Theme.accentLight
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: fb.text
        Keys.onReturnPressed: fb.clicked()
        Keys.onSpacePressed: fb.clicked()

        Text {
            id: fbLabel
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: fb.text
            font.pixelSize: Dimensions.fontMD
            font.weight: Font.DemiBold
            color: fb.filled ? Theme.textOnColor : Theme.textPrimary
        }
        HoverHandler { id: fbHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: fb.clicked() }
    }

    footer: Item {
        implicitHeight: 72

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingXL
            anchors.rightMargin: Dimensions.paddingXL
            spacing: Dimensions.spacingMD

            Item { Layout.fillWidth: true }

            FooterButton {
                text: qsTr("Başka klasör seç")
                onClicked: {
                    root.close()
                    root.pickAnother()
                }
            }
            FooterButton {
                id: primaryBtn
                visible: root._canUse
                filled: true
                fill: root._verdict === "unsure" && !root._moving ? Theme.warning : Theme.primary
                text: root._moving ? qsTr("Yamayı oyunun klasörüne taşı")
                    : root.intent === "install"
                      ? (root._verdict === "ok" ? qsTr("Bu klasöre kur") : qsTr("Yine de buraya kur"))
                      : (root._verdict === "ok" ? qsTr("Bu klasörü kullan") : qsTr("Yine de kullan"))
                onClicked: root._primary()
            }
        }
    }

    onOpened: if (primaryBtn.visible) primaryBtn.forceActiveFocus()
}
