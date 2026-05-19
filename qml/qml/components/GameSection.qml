import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameSection.qml - Reusable section: title + badge + horizontal game strip + empty state.
 * Wraps SectionContainer with a standard header pattern.
 */
SectionContainer {
    id: section

    property string title: ""
    property var model: []
    property color badgeColor: Theme.primary
    property string emptyText: ""
    property bool loading: false
    property int notificationCount: 0
    property string notificationText: ""
    property bool refreshable: false

    signal gameClicked(string gameId, string gameName, string installPath, string engine)
    signal refreshClicked()

    Layout.fillWidth: true
    Layout.fillHeight: true

    // Header row — fixed height so the title never shifts when the
    // refresh button (24px) is swapped for the BusyIndicator on refresh
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 24
        spacing: Dimensions.spacingSM

        Rectangle {
            Layout.preferredWidth: 6; Layout.preferredHeight: 6
            radius: 3; color: Theme.accentBase
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            textFormat: Text.PlainText
            text: section.title
            font.pixelSize: Dimensions.fontLG
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.alignment: Qt.AlignVCenter
        }

        BusyIndicator {
            visible: section.loading
            running: section.loading
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
        }

        Item { Layout.fillWidth: true }

        // Refresh button — hover: spins in place; press: spins while it
        // shrinks then springs back to full size. Durations come from
        // Dimensions tokens so the "Uygulama Animasyonları" preference
        // (enableAnimations) governs it like everything else.
        Rectangle {
            visible: section.refreshable && !section.loading
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: 12
            color: _refreshMouse.containsMouse ? Theme.surfaceLight : "transparent"

            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

            Text {
                id: refreshIcon
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "↻"
                font.pixelSize: 16
                font.weight: Font.Light
                color: _refreshMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                // Continuous in-place spin on hover (render thread = smooth);
                // stays alive through the click pulse so the press spins too.
                RotationAnimator {
                    target: refreshIcon
                    from: 0; to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: SettingsManager.enableAnimations
                             && (_refreshMouse.containsMouse || pressPulse.running)
                }

                // Press: shrink, then spring back to full size while spinning.
                SequentialAnimation {
                    id: pressPulse
                    NumberAnimation {
                        target: refreshIcon; property: "scale"
                        to: 0.6; duration: Dimensions.animFast
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: refreshIcon; property: "scale"
                        to: 1.0; duration: Dimensions.animMedium
                        easing.type: Easing.OutBack
                    }
                }
            }

            MouseArea {
                id: _refreshMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { pressPulse.restart(); section.refreshClicked() }

                ToolTip.visible: containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("Yenile")
            }
        }

        // Outdated patch notification badge
        Rectangle {
            id: notifBadge
            visible: section.notificationCount > 0
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: _notifRow.width + 16
            Layout.preferredHeight: 24
            radius: 12
            color: _notifMouse.containsMouse ? Theme.warning10 : Theme.warning06
            border.width: 1
            border.color: Theme.warning20

            Behavior on color { ColorAnimation { duration: 150 } }

            Row {
                id: _notifRow
                anchors.centerIn: parent
                spacing: 5

                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.warning
                }

                Text {
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                    text: section.notificationText
                    font.pixelSize: Dimensions.fontXS
                    font.weight: Font.Medium
                    color: Theme.warning
                }
            }

            MouseArea {
                id: _notifMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                ToolTip.visible: containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("%1 yamanız için güncelleme mevcut").arg(section.notificationCount)
            }
        }
    }

    // Separator
    SettingsDivider { variant: "section" }

    // Game strip with edge controls
    Item {
        id: stripContainer
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (section.model || []).length > 0

        HorizontalGameStrip {
            id: gameStrip
            anchors.fill: parent
            model: section.model
            onGameClicked: (gameId, gameName, installPath, engine) =>
                section.gameClicked(gameId, gameName, installPath, engine)
        }
    }

    // Empty state
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (section.model || []).length === 0 && !section.loading
        text: section.emptyText
        font.pixelSize: Dimensions.fontSM
        color: Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
