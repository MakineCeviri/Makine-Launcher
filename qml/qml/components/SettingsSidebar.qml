import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * SettingsSidebar.qml - Settings screen sidebar navigation
 */
Rectangle {
    id: root

    property int selectedCategory: 0
    property var categories: []

    signal categorySelected(int index)

    width: Dimensions.sidebarWidth
    color: Theme.surface50

    // Local SidebarCategoryItem component (matches SettingsScreen inline version)
    component SidebarCategoryItem: Rectangle {
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
            ? Theme.primary10
            : catMouse.pressed
                ? Theme.textPrimary06
                : catMouse.containsMouse
                    ? Theme.textPrimary04
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
            textFormat: Text.PlainText
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.navbarHeight

            Label {
                textFormat: Text.PlainText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Dimensions.marginML
                text: qsTr("Ayarlar")
                font.pixelSize: Dimensions.headlineLarge
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }

        Item { Layout.preferredHeight: 4 }

        // Category list
        Repeater {
            model: root.categories

            // Item wrapper to include optional separator before plugin categories
            ColumnLayout {
                required property int index
                required property var modelData
                Layout.fillWidth: true
                spacing: 0

                // Separator before first plugin category
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: Dimensions.spacingSM
                    Layout.bottomMargin: Dimensions.spacingSM
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    color: Theme.primary08
                    visible: modelData.isPlugin && (index === 0 || !root.categories[index - 1].isPlugin)
                }

                SidebarCategoryItem {
                    Layout.fillWidth: true
                    categoryIndex: parent.index
                    name: parent.modelData.name
                    isSelected: root.selectedCategory === parent.index
                    onClicked: root.categorySelected(parent.index)
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Version info at bottom
        Label {
            textFormat: Text.PlainText
            Layout.leftMargin: Dimensions.marginML
            Layout.bottomMargin: Dimensions.marginML
            text: Dimensions.appName + " " + Dimensions.appVersionFull
            font.pixelSize: Dimensions.fontSM
            color: Theme.textMuted
        }
    }
}
