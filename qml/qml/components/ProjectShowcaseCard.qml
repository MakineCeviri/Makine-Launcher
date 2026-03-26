import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

// Project card - clean design with subtle accent
Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string status: ""
    property string emoji: ""
    property color statusColor: Theme.primary
    property color accentColor: Theme.primary
    property real progress: -1
    property int entryIndex: 0
    property bool animationsEnabled: true

    Layout.fillWidth: true
    implicitHeight: 96
    radius: Dimensions.radiusStandard
    color: hovered ? Theme.textPrimary06
                   : Theme.textPrimary03
    border.color: hovered ? Theme.withAlpha(root.accentColor, 0.25)
                          : Theme.textPrimary08
    border.width: 1

    property bool hovered: false
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

    // Hover lift
    transform: Translate {
        y: root.hovered ? -1 : 0
        Behavior on y { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }
    }

    // Entry animation
    opacity: 0; scale: 0.98
    Component.onCompleted: entryAnim.start()
    SequentialAnimation {
        id: entryAnim
        PauseAnimation { duration: root.entryIndex * 50 }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "scale"; from: 0.98; to: 1.0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
    }
    function replayAnimation() { root.opacity = 0; root.scale = 0.98; entryAnim.restart() }

    // Bottom progress bar (replaces accent line when progress exists)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.progress >= 0 ? 3 : 2
        radius: 1
        color: root.progress >= 0 ? Theme.textPrimary06 : "transparent"

        Rectangle {
            width: root.progress >= 0
                   ? parent.width * Math.max(root.progress, 0)
                   : (root.hovered ? parent.width : parent.width * 0.4)
            height: parent.height
            anchors.left: parent.left
            radius: parent.radius
            color: root.accentColor
            opacity: root.progress >= 0
                     ? 0.7
                     : (root.hovered ? 0.6 : 0.3)
            Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        }

        // Percentage label
        Label {
            textFormat: Text.PlainText
            anchors.right: parent.right
            anchors.rightMargin: Dimensions.spacingSM
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -10
            text: Math.round(root.progress * 100) + "%"
            font.pixelSize: Dimensions.fontMicro; font.weight: Font.Medium
            color: Theme.textMuted
            visible: root.progress > 0
        }
    }

    // Content
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginMS
        anchors.rightMargin: Dimensions.marginMS
        anchors.topMargin: Dimensions.marginMS
        anchors.bottomMargin: Dimensions.marginMS + (root.progress >= 0 ? 2 : 0)
        spacing: Dimensions.spacingLG

        // Emoji icon with accent ring
        Rectangle {
            Layout.preferredWidth: 38; Layout.preferredHeight: 38
            Layout.alignment: Qt.AlignVCenter
            radius: 19
            color: Theme.withAlpha(root.accentColor, root.hovered ? 0.12 : 0.06)
            border.color: Theme.withAlpha(root.accentColor, root.hovered ? 0.25 : 0.12)
            border.width: 1
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
            Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
            Label {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: root.emoji
                font.pixelSize: Dimensions.fontMD
            }
        }

        // Text content
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: Dimensions.spacingXXS

            // Title + status badge
            RowLayout {
                Layout.fillWidth: true; spacing: Dimensions.spacingMD
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true; text: root.title
                    font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                    color: Theme.textPrimary; elide: Text.ElideRight
                }
                Rectangle {
                    implicitWidth: statusLabel.implicitWidth + Dimensions.paddingSM * 2
                    implicitHeight: 20; radius: Dimensions.radiusFull
                    color: Theme.withAlpha(root.statusColor, 0.10)
                    border.color: Theme.withAlpha(root.statusColor, 0.15)
                    border.width: 1
                    Label {
                        textFormat: Text.PlainText
                        id: statusLabel; anchors.centerIn: parent; text: root.status
                        font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: root.statusColor
                    }
                }
            }

            // Description
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true; text: root.description
                font.pixelSize: Dimensions.fontXS; color: Theme.textSecondary
                wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                lineHeight: 1.3
            }

            Item { Layout.fillHeight: true }
        }
    }

    MouseArea {
        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton; onContainsMouseChanged: root.hovered = containsMouse
    }

    Accessible.role: Accessible.Button
    Accessible.name: root.title + " - " + root.status
    activeFocusOnTab: true
    Keys.onSpacePressed: root.clicked()
    Keys.onReturnPressed: root.clicked()

    // Focus indicator
    FocusRing {
        target: root
        offset: -1
    }
}
