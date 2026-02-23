import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * SettingsSidebar.qml - Settings screen sidebar navigation
 */
Rectangle {
    id: root

    property int selectedCategory: 0
    property var categories: []

    signal categorySelected(int index)

    width: Dimensions.sidebarWidth
    color: Theme.withAlpha(Theme.surface, 0.5)

    // Local CategoryItem component (matches SettingsScreen inline version)
    component CategoryItem: Rectangle {
        id: catItem
        property int categoryIndex: 0
        property string name: ""
        property bool isSelected: false
        signal clicked()

        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        Accessible.role: Accessible.Button
        Accessible.name: name
        Accessible.onPressAction: clicked()

        height: 36
        radius: 0
        color: isSelected
            ? Theme.withAlpha(Theme.textPrimary, 0.08)
            : catMouse.pressed
                ? Theme.withAlpha(Theme.textPrimary, 0.06)
                : catMouse.containsMouse
                    ? Theme.withAlpha(Theme.textPrimary, 0.04)
                    : "transparent"

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        // Active indicator bar
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: isSelected ? 20 : 0
            radius: 1.5
            color: Theme.primary
            opacity: isSelected ? 1.0 : 0.0
            Behavior on height { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Dimensions.marginML
            text: name
            font.pixelSize: Dimensions.fontMD
            font.weight: isSelected ? Font.DemiBold : Font.Normal
            color: isSelected ? Theme.textPrimary
                 : catMouse.containsMouse ? Theme.textPrimary
                 : Theme.textSecondary
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }

        MouseArea {
            id: catMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: catItem.clicked()
        }
    }

    // Right border
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.withAlpha(Theme.textPrimary, 0.06)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.navbarHeight

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Dimensions.marginML
                text: qsTr("Ayarlar")
                font.pixelSize: Dimensions.headlineLarge
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }

        // Divider
        SettingsDivider { variant: "section" }

        Item { Layout.preferredHeight: 4 }

        // Category list
        Repeater {
            model: root.categories

            CategoryItem {
                Layout.fillWidth: true
                categoryIndex: index
                name: modelData.name
                isSelected: root.selectedCategory === index
                onClicked: {
                    root.categorySelected(index)
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Version info at bottom
        Label {
            Layout.leftMargin: Dimensions.marginML
            Layout.bottomMargin: Dimensions.marginML
            text: Dimensions.appName + " " + Dimensions.appVersionFull
            font.pixelSize: Dimensions.fontSM
            color: Theme.textMuted
        }
    }
}
