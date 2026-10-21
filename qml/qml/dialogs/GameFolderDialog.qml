// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameFolderDialog.qml - What a picked folder turned out to be, before it is used
 *
 * The user may point at the game's folder, the folder the .exe sits in, the
 * exe itself or the library that holds the game; GameService.inspectGameFolder
 * finds the root the patch needs and checks it against the patch. This shows
 * the outcome as the user would want it explained: where they pointed, where
 * the patch will go, and the evidence for it — the game's exe, the files the
 * patch rewrites, the folders it writes into. Nothing is bound until "Bu
 * klasörü kullan".
 */
BaseDialog {
    id: root

    property var report: ({})
    property string gameName: ""

    signal useFolder(string root, string engine)
    signal pickAnother()
    // The patch sits in the wrong folder: take it off, then bind the right one.
    signal uninstallAndUse(string root, string engine)

    function _use() {
        root.useFolder(root.report.root, root.report.engine || "")
        root.close()
    }

    readonly property string _verdict: root.report.verdict || "refused"
    readonly property bool _climbed: !!root.report.climbed

    accentColor: _verdict === "ok" ? Theme.success
               : _verdict === "unsure" || root.report.patchedElsewhere ? Theme.warning
               : Theme.error

    width: 560
    title: root.report.audit ? (_verdict === "refused" ? qsTr("Yama oyunun klasörüne gitmemiş")
                                                       : qsTr("Oyunun asıl klasörü bulundu"))
         : _verdict === "ok" ? qsTr("Oyunun klasörü bulundu")
         : _verdict === "unsure" ? qsTr("Bu klasörden emin olamadık")
         : qsTr("Bu klasör kullanılamaz")

    // One line of evidence: state glyph + sentence.
    component Check: RowLayout {
        id: check
        property string kind: "ok"   // ok | warn | info | bad
        property string text: ""
        Layout.fillWidth: true
        spacing: Dimensions.spacingMD

        Label {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            textFormat: Text.PlainText
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 13
            text: check.kind === "ok" ? "\uE73E" : check.kind === "bad" ? "\uE711"
                : check.kind === "warn" ? "\uE7BA" : "\uE946"
            color: check.kind === "ok" ? Theme.success : check.kind === "bad" ? Theme.error
                 : check.kind === "warn" ? Theme.warning : Theme.textMuted
        }
        Label {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: check.text
            wrapMode: Text.WordWrap
            lineHeight: 1.35
            font.pixelSize: Dimensions.fontSM
            color: Theme.textPrimary
        }
    }

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.withAlpha(root.accentColor, 0.10)
                border.color: Theme.withAlpha(root.accentColor, 0.20)
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 15
                    text: root._verdict === "ok" ? "\uE73E" : root._verdict === "unsure" ? "\uE7BA" : "\uE711"
                    color: root.accentColor
                }
            }

            Label {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            DialogCloseButton {
                onClicked: { root.cancelled(); root.close() }
            }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.textPrimary06
        }
    }

    contentItem: ColumnLayout {
        spacing: Dimensions.spacingLG

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        // ── Where they pointed → where the patch goes ───────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            implicitHeight: paths.implicitHeight + Dimensions.paddingMD * 2
            radius: Dimensions.radiusMD
            color: Theme.textPrimary04
            border.width: 1
            border.color: Theme.textPrimary08
            visible: (root.report.root || "") !== "" || (root.report.picked || "") !== ""

            ColumnLayout {
                id: paths
                anchors.fill: parent
                anchors.margins: Dimensions.paddingMD
                spacing: Dimensions.spacingXS

                Label {
                    visible: root._climbed || (root.report.root || "") === ""
                    textFormat: Text.PlainText
                    text: root.report.audit ? qsTr("ŞU ANKİ KLASÖR") : qsTr("SEÇTİĞİN")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    color: Theme.textMuted
                }
                Label {
                    Layout.fillWidth: true
                    visible: root._climbed || (root.report.root || "") === ""
                    textFormat: Text.PlainText
                    text: root.report.picked || ""
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textSecondary
                }
                Label {
                    visible: root._climbed
                    Layout.topMargin: 2
                    textFormat: Text.PlainText
                    text: "↓  " + qsTr("oyunun kök klasörü burası")
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.accentLight
                }
                Label {
                    visible: (root.report.root || "") !== ""
                    Layout.topMargin: root._climbed ? 2 : 0
                    textFormat: Text.PlainText
                    text: root.report.patchedElsewhere ? qsTr("YAMANIN GİTMESİ GEREKEN YER")
                                                       : qsTr("YAMA BURAYA KURULACAK")
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    color: root._verdict === "ok" || root.report.patchedElsewhere ? Theme.success
                         : root._verdict === "unsure" ? Theme.warning
                         : Theme.textMuted
                }
                Label {
                    Layout.fillWidth: true
                    visible: (root.report.root || "") !== ""
                    textFormat: Text.PlainText
                    text: root.report.rootShown || ""
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
            }
        }

        // ── The evidence ─────────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG
            Layout.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Check {
                visible: (root.report.problem || "") !== ""
                kind: root.report.patchedElsewhere ? "warn" : "bad"
                text: root.report.problem || ""
            }
            Check {
                visible: (root.report.root || "") !== ""
                kind: (root.report.exe || "") !== "" ? "ok" : "warn"
                text: (root.report.exe || "") !== ""
                      ? qsTr("Oyunun kendi dosyası burada: %1").arg(root.report.exe)
                      : qsTr("%1 için beklenen .exe dosyası bu klasörde yok.").arg(root.gameName || qsTr("Bu oyun"))
            }
            Check {
                visible: (root.report.root || "") !== "" && (root.report.filesTotal || 0) > 0
                kind: root.report.filesFound === root.report.filesTotal ? "ok" : "warn"
                text: root.report.filesFound === root.report.filesTotal
                      ? qsTr("Yamanın değiştireceği %1 oyun dosyasının hepsi yerinde.").arg(root.report.filesTotal)
                      : qsTr("Yamanın değiştireceği %1 oyun dosyasından %2 tanesi bu klasörde yok; kurulum yarıda kalabilir.")
                            .arg(root.report.filesTotal).arg(root.report.filesTotal - root.report.filesFound)
            }
            Check {
                visible: (root.report.root || "") !== "" && (root.report.dirsTotal || 0) > 0
                kind: root.report.dirsFound * 2 >= root.report.dirsTotal ? "ok" : "warn"
                text: qsTr("Yamanın dosya yazacağı %1 klasörün %2 tanesi burada.")
                          .arg(root.report.dirsTotal).arg(root.report.dirsFound)
            }
            Check {
                visible: (root.report.otherGame || "") !== "" && root._verdict !== "refused"
                kind: "warn"
                text: qsTr("Bu klasör daha çok %1 oyununa benziyor. Doğru oyunun klasörünü seçtiğinden emin ol.")
                          .arg(root.report.otherGame || "")
            }
            Check {
                visible: root._verdict !== "refused" && root.report.writable === false
                kind: "info"
                text: qsTr("Bu klasöre yazmak için kurulum sırasında bir kez yönetici izni istenecek.")
            }
            Check {
                visible: root._verdict !== "refused" && root.report.same === true
                kind: "info"
                text: qsTr("Bu zaten oyunun şu anki klasörü.")
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 60

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.textPrimary06
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG
            anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Item { Layout.fillWidth: true }

            Rectangle {
                id: another
                Layout.preferredWidth: anotherLbl.implicitWidth + Dimensions.paddingLG * 2
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: anotherHover.hovered ? Theme.textPrimary12 : Theme.textPrimary06
                Accessible.role: Accessible.Button
                Accessible.name: anotherLbl.text

                Label {
                    id: anotherLbl
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: qsTr("Başka klasör seç")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                HoverHandler { id: anotherHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        root.close()
                        root.pickAnother()
                    }
                }
            }

            Rectangle {
                id: fix
                visible: root.report.patchedElsewhere === true
                Layout.preferredWidth: fixLbl.implicitWidth + Dimensions.paddingLG * 2
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: fixHover.hovered ? Qt.darker(Theme.primary, 1.1) : Theme.primary
                Accessible.role: Accessible.Button
                Accessible.name: fixLbl.text

                Label {
                    id: fixLbl
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: qsTr("Yamayı kaldır ve klasörü düzelt")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }
                HoverHandler { id: fixHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        root.uninstallAndUse(root.report.root, root.report.engine || "")
                        root.close()
                    }
                }
            }

            Rectangle {
                id: use
                visible: root._verdict !== "refused" && root.report.same !== true
                Layout.preferredWidth: useLbl.implicitWidth + Dimensions.paddingLG * 2
                Layout.preferredHeight: 36
                radius: Dimensions.radiusStandard
                color: useHover.hovered ? Qt.darker(root.accentColor, 1.1) : root.accentColor
                Accessible.role: Accessible.Button
                Accessible.name: useLbl.text
                activeFocusOnTab: true
                Keys.onReturnPressed: root._use()

                Label {
                    id: useLbl
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    text: root._verdict === "ok" ? qsTr("Bu klasörü kullan") : qsTr("Yine de bu klasörü kullan")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }
                HoverHandler { id: useHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root._use() }
            }
        }
    }
}
