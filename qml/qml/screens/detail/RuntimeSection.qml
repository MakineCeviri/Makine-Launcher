import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

SectionContainer {
    id: rtRoot
    contentSpacing: Dimensions.spacingLG

    // Required properties from parent
    required property string gameId
    required property bool isUnityGame
    required property bool runtimeNeeded
    required property bool runtimeInstalled
    required property bool runtimeUpToDate
    required property string bepinexVersion
    required property string xunityVersion
    required property string unityBackend
    required property string unityVersion
    required property bool hasAntiCheat
    required property string antiCheatName
    required property bool isInstallingRuntime

    visible: isUnityGame && runtimeNeeded

    Text {
        textFormat: Text.PlainText
        text: qsTr("Çeviri Çalışma Ortamı")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold; color: Theme.textPrimary
    }

    SettingsDivider { variant: "section" }

    // Status header
    RowLayout {
        spacing: Dimensions.spacingLG

        Rectangle {
            width: 40; height: 40; radius: 20
            color: rtRoot.runtimeInstalled ? Theme.success12 : Theme.textPrimary12
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: rtRoot.runtimeInstalled ? "\u2713" : "\u2193"
                font.pixelSize: Dimensions.fontTitle; font.weight: Font.Bold
                color: rtRoot.runtimeInstalled ? Theme.success : Theme.textMuted
            }
        }

        ColumnLayout {
            Layout.fillWidth: true; spacing: Dimensions.spacingXXS
            Text {
                textFormat: Text.PlainText
                text: rtRoot.runtimeInstalled
                    ? (rtRoot.runtimeUpToDate ? qsTr("BepInEx Kurulu ve Güncel") : qsTr("BepInEx Güncelleme Mevcut"))
                    : qsTr("BepInEx Kurulu Değil")
                font.pixelSize: Dimensions.fontMD; font.weight: Font.DemiBold
                color: rtRoot.runtimeInstalled ? Theme.success : Theme.textSecondary
            }
            Text {
                textFormat: Text.PlainText
                visible: rtRoot.runtimeInstalled && rtRoot.bepinexVersion !== ""
                text: "BepInEx " + rtRoot.bepinexVersion
                font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
            }
            Text {
                textFormat: Text.PlainText
                visible: !rtRoot.runtimeInstalled
                text: qsTr("Çevirinin çalışması için BepInEx gereklidir")
                font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
            }
        }

        // Backend badge
        Rectangle {
            visible: rtRoot.unityBackend !== "" && rtRoot.unityBackend !== "unknown"
            width: backendLbl.width + 12; height: 24
            radius: Dimensions.radiusFull
            color: Theme.textPrimary06
            Text {
                textFormat: Text.PlainText
                id: backendLbl
                anchors.centerIn: parent
                text: rtRoot.unityBackend
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.Medium
                color: Theme.textSecondary
            }
        }
    }

    // Anti-cheat warning (own card style preserved)
    Rectangle {
        Layout.fillWidth: true; visible: rtRoot.hasAntiCheat
        implicitHeight: acRow.height + Dimensions.marginSM * 2
        radius: Dimensions.radiusStandard
        color: Theme.destructive08
        border.color: Theme.destructive20; border.width: 1

        RowLayout {
            id: acRow
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: Dimensions.marginSM
            spacing: Dimensions.spacingMD
            Text {
                textFormat: Text.PlainText
                text: "\u26A0"
                font.pixelSize: Dimensions.fontTitle
                color: Theme.destructive
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: qsTr("Bu oyunda %1 tespit edildi. BepInEx ile uyumsuz olabilir.").arg(rtRoot.antiCheatName)
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
            opacity: rtRoot.isInstallingRuntime ? 0.6 : 1.0
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            Accessible.role: Accessible.Button
            Accessible.name: rtRoot.isInstallingRuntime ? qsTr("Kuruluyor...") : (!rtRoot.runtimeInstalled ? qsTr("BepInEx Kur") : qsTr("Güncelle"))

            Row {
                id: rtBtnRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                Text {
                    textFormat: Text.PlainText
                    text: rtRoot.isInstallingRuntime ? qsTr("Kuruluyor...") : (!rtRoot.runtimeInstalled ? qsTr("BepInEx Kur") : qsTr("Güncelle"))
                    font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.textOnColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                id: rtBtnMouse; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor; enabled: !rtRoot.isInstallingRuntime
                onClicked: { rtRoot.isInstallingRuntime = true; GameService.installRuntime(rtRoot.gameId) }
            }
        }

        // Uninstall
        Rectangle {
            visible: rtRoot.runtimeInstalled
            implicitWidth: rtUnRow.width + 32; implicitHeight: 38
            radius: Dimensions.radiusStandard
            color: rtUnMouse.containsMouse ? Theme.destructive15 : Theme.textPrimary06
            border.color: Theme.textPrimary10; border.width: 1
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("BepInEx Kaldır")
            Row {
                id: rtUnRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Kaldır")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                id: rtUnMouse; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: GameService.uninstallRuntime(rtRoot.gameId)
            }
        }
    }
}
