import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * AllGamesDialog.qml - Modal dialog showing all supported games with search and filter
 */
Dialog {
    id: root

    property var games: []
    property string searchText: ""
    property string selectedCategory: "all"
    property bool batchMode: false
    property var selectedGameIds: ({})
    property int selectedCount: Object.keys(selectedGameIds).length

    signal gameSelected(string gameId)

    function toggleBatchSelection(gameId) {
        var copy = Object.assign({}, selectedGameIds)
        if (copy[gameId])
            delete copy[gameId]
        else
            copy[gameId] = true
        selectedGameIds = copy
    }

    function selectAllFiltered() {
        var copy = Object.assign({}, selectedGameIds)
        for (var i = 0; i < filteredGames.length; i++)
            copy[filteredGames[i].id] = true
        selectedGameIds = copy
    }

    function deselectAll() {
        selectedGameIds = ({})
    }

    function startBatchInstall() {
        var ids = Object.keys(selectedGameIds)
        if (ids.length > 0) {
            BatchOperationService.batchInstall(ids)
            batchMode = false
            selectedGameIds = ({})
            root.close()
        }
    }

    title: qsTr("Tüm Desteklenen Oyunlar")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 900
    height: 700

    // Center in parent
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    // Filter games based on search and category
    property var filteredGames: {
        var result = root.games
        if (searchText.length > 0) {
            var search = searchText.toLowerCase()
            result = result.filter(g => (g.name || "").toLowerCase().includes(search))
        }
        if (selectedCategory === "verified") {
            result = result.filter(g => g.isVerified === true)
        } else if (selectedCategory === "translated") {
            result = result.filter(g => g.hasTranslation === true)
        } else if (selectedCategory === "untranslated") {
            result = result.filter(g => !g.hasTranslation)
        }
        return result
    }

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
    }

    // Custom header
    header: Rectangle {
        height: 140
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            // Top row with title and close
            RowLayout {
                spacing: 16

                // Icon
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: Dimensions.radiusStandard
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.withAlpha(Theme.gold, 0.2) }
                        GradientStop { position: 1.0; color: Theme.withAlpha(Theme.olive, 0.2) }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAE"  // Game controller
                        font.pixelSize: Dimensions.headlineLarge
                    }
                }

                ColumnLayout {
                    spacing: 4

                    Text {
                        text: qsTr("Tüm Desteklenen Oyunlar")
                        font.pixelSize: Dimensions.headlineLarge
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }

                    Text {
                        text: qsTr("%1 oyun listeleniyor").arg(filteredGames.length)
                        font.pixelSize: Dimensions.fontBody
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }

                // Batch mode toggle
                Rectangle {
                    Layout.preferredWidth: batchToggleRow.width + 16
                    Layout.preferredHeight: 32
                    radius: Dimensions.radiusStandard
                    color: batchMode
                           ? Theme.withAlpha(Theme.primary, 0.15)
                           : (batchToggleMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04))
                    border.color: batchMode ? Theme.primary : "transparent"
                    border.width: 1
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Batch mode")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: { batchMode = !batchMode; if (!batchMode) deselectAll() }
                    Keys.onSpacePressed: { batchMode = !batchMode; if (!batchMode) deselectAll() }

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    Row {
                        id: batchToggleRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "\u2611"
                            font.pixelSize: Dimensions.fontMD
                            color: batchMode ? Theme.primary : Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: qsTr("Toplu Seçim")
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.Medium
                            color: batchMode ? Theme.primary : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
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
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: batchToggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            batchMode = !batchMode
                            if (!batchMode) deselectAll()
                        }
                    }
                }

                // Close button
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Dimensions.radiusStandard
                    color: closeDialogMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Close")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: root.close()
                    Keys.onSpacePressed: root.close()

                    Text {
                        anchors.centerIn: parent
                        text: "\u00D7"
                        font.pixelSize: Dimensions.headlineLarge
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
                        id: closeDialogMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }

            // Search and filter row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // Search input
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.color: searchInput.activeFocus ? Theme.primary : Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1

                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: "\uD83D\uDD0D"
                            font.pixelSize: Dimensions.fontLG
                            color: Theme.textMuted
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            font.pixelSize: Dimensions.fontMD
                            color: Theme.textPrimary
                            clip: true
                            selectByMouse: true
                            Accessible.role: Accessible.EditableText
                            Accessible.name: qsTr("Search games")
                            onTextChanged: root.searchText = text

                            Text {
                                anchors.fill: parent
                                text: qsTr("Oyun ara...")
                                font.pixelSize: Dimensions.fontMD
                                color: Theme.textMuted
                                visible: !searchInput.text && !searchInput.activeFocus
                            }
                        }

                        // Clear button
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 12
                            color: clearSearchMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                            visible: searchInput.text.length > 0
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Clear search")

                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                font.pixelSize: Dimensions.fontMD
                                color: Theme.textMuted
                            }

                            MouseArea {
                                id: clearSearchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: searchInput.text = ""
                            }
                        }
                    }
                }

                // Category filters
                RowLayout {
                    spacing: 8

                    CategoryButton {
                        text: qsTr("Tümü")
                        category: "all"
                        isSelected: selectedCategory === "all"
                        onClicked: selectedCategory = "all"
                    }

                    CategoryButton {
                        text: qsTr("Onaylı")
                        category: "verified"
                        isSelected: selectedCategory === "verified"
                        onClicked: selectedCategory = "verified"
                    }

                    CategoryButton {
                        text: qsTr("Çevirisi Var")
                        category: "translated"
                        isSelected: selectedCategory === "translated"
                        onClicked: selectedCategory = "translated"
                    }

                    CategoryButton {
                        text: qsTr("Çevirisi Yok")
                        category: "untranslated"
                        isSelected: selectedCategory === "untranslated"
                        onClicked: selectedCategory = "untranslated"
                    }
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }
    }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "transparent" }
            contentItem: Rectangle {
                implicitWidth: 8
                radius: Dimensions.radiusStandard
                color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
            }
        }

        // Games grid
        GridLayout {
            id: gamesGrid
            width: parent.width - 48
            anchors.horizontalCenter: parent.horizontalCenter
            columns: Math.max(1, Math.floor((parent.width - 48) / (Dimensions.cardWidth + Dimensions.cardGap)))
            columnSpacing: Dimensions.cardGap
            rowSpacing: Dimensions.cardGap

            Repeater {
                model: root.filteredGames

                // Game card
                Rectangle {
                    Layout.preferredWidth: Dimensions.cardWidth
                    Layout.preferredHeight: Dimensions.cardHeight
                    radius: Dimensions.cardBorderRadius
                    color: Theme.surface
                    clip: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name || qsTr("Unknown")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: {
                        if (batchMode)
                            toggleBatchSelection(modelData.id)
                        else {
                            root.gameSelected(modelData.id)
                            root.close()
                        }
                    }
                    Keys.onSpacePressed: Keys.onReturnPressed(event)

                    scale: cardMouse.containsMouse ? 1.05 : 1.0
                    transformOrigin: Item.Center

                    Behavior on scale {
                        NumberAnimation {
                            duration: Dimensions.animNormal
                            easing.type: Easing.OutCubic
                        }
                    }

                    // Background gradient
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.surface }
                            GradientStop { position: 1.0; color: Theme.surfaceActive }
                        }
                    }

                    // Game image
                    Image {
                        id: gameImg
                        anchors.fill: parent
                        source: modelData.headerImageUrl || ""
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                        asynchronous: true
                        cache: true
                        visible: status === Image.Ready
                    }

                    // Placeholder
                    Text {
                        anchors.centerIn: parent
                        text: modelData.name ? modelData.name.substring(0, 2).toUpperCase() : ""
                        font.pixelSize: Dimensions.fontXL
                        font.weight: Font.Bold
                        color: Theme.textMuted
                        visible: !gameImg.visible
                    }

                    // Bottom gradient overlay
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.5
                        radius: Dimensions.cardBorderRadius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.9) }
                        }
                    }

                    // Status badges
                    Row {
                        visible: modelData.isVerified === true || modelData.hasTranslation === true
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4

                        Rectangle {
                            visible: modelData.hasTranslation === true
                            width: 24; height: 16
                            radius: Dimensions.badgeRadius
                            color: Theme.turkishRed
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "TR"
                                font.pixelSize: Dimensions.fontMicro
                                font.weight: Font.Bold
                                color: "white"
                            }
                        }

                        Rectangle {
                            visible: modelData.isVerified === true
                            width: 16; height: 16
                            radius: 8
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                font.pixelSize: Dimensions.fontMini
                                font.weight: Font.Bold
                                color: "white"
                            }
                        }
                    }

                    // Engine badge
                    Rectangle {
                        visible: (modelData.engine || "") !== ""
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.bottomMargin: 28
                        anchors.leftMargin: 8
                        width: engineLabel.width + 12
                        height: 18
                        radius: Dimensions.badgeRadius
                        color: Qt.rgba(0, 0, 0, 0.7)

                        Text {
                            id: engineLabel
                            anchors.centerIn: parent
                            text: modelData.engine || ""
                            font.pixelSize: Dimensions.fontMini
                            font.weight: Font.Medium
                            color: Theme.textMuted
                        }
                    }

                    // Game name
                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 10
                        text: modelData.name || qsTr("Unknown")
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: "white"
                        elide: Text.ElideRight
                    }

                    // Hover overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        opacity: cardMouse.containsMouse ? 1.0 : 0.0
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.splashGold, 0.08) }
                            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.pink, 0.12) }
                        }

                        Behavior on opacity { NumberAnimation { duration: Dimensions.transitionDuration } }
                    }

                    // Border glow
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.withAlpha(Theme.splashGold, 0.6)
                        opacity: cardMouse.containsMouse ? 0.6 : 0

                        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
                    }

                    // Batch selection checkbox overlay
                    Rectangle {
                        visible: batchMode
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.topMargin: 6
                        anchors.leftMargin: 6
                        width: 22
                        height: 22
                        radius: 4
                        color: selectedGameIds[modelData.id]
                               ? Theme.primary
                               : Qt.rgba(0, 0, 0, 0.6)
                        border.color: selectedGameIds[modelData.id]
                                      ? Theme.primary
                                      : Qt.rgba(1, 1, 1, 0.3)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.Bold
                            color: "white"
                            visible: selectedGameIds[modelData.id] === true
                        }
                    }

                    // Selected tint overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        color: Theme.withAlpha(Theme.primary, 0.15)
                        visible: batchMode && selectedGameIds[modelData.id] === true
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
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (batchMode)
                                toggleBatchSelection(modelData.id)
                            else {
                                root.gameSelected(modelData.id)
                                root.close()
                            }
                        }
                    }
                }
            }
        }

        // Empty state
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16
            visible: root.filteredGames.length === 0

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\uD83D\uDE14"
                font.pixelSize: Dimensions.displayLarge
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Oyun bulunamadı")
                font.pixelSize: Dimensions.fontTitle
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Arama kriterlerini değiştirmeyi deneyin")
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted
            }
        }
    }

    // ===== BATCH ACTION FOOTER =====
    footer: Rectangle {
        height: batchMode ? 56 : 0
        color: Theme.surface
        visible: batchMode
        clip: true

        Behavior on height {
            NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 12

            Text {
                text: selectedCount > 0
                      ? qsTr("%1 oyun seçildi").arg(selectedCount)
                      : qsTr("Seçim yapın")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            // Select all
            Rectangle {
                Layout.preferredWidth: selectAllLabel.width + 16
                Layout.preferredHeight: 30
                radius: Dimensions.radiusStandard
                color: selectAllMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Select all")
                activeFocusOnTab: true
                Keys.onReturnPressed: selectAllFiltered()
                Keys.onSpacePressed: selectAllFiltered()

                Text {
                    id: selectAllLabel
                    anchors.centerIn: parent
                    text: qsTr("Tümünü Seç")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: Theme.textSecondary
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
                    id: selectAllMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: selectAllFiltered()
                }
            }

            // Deselect all
            Rectangle {
                visible: selectedCount > 0
                Layout.preferredWidth: deselectLabel.width + 16
                Layout.preferredHeight: 30
                radius: Dimensions.radiusStandard
                color: deselectMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Deselect all")
                activeFocusOnTab: true
                Keys.onReturnPressed: deselectAll()
                Keys.onSpacePressed: deselectAll()

                Text {
                    id: deselectLabel
                    anchors.centerIn: parent
                    text: qsTr("Seçimi Kaldır")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: Theme.textSecondary
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
                    id: deselectMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: deselectAll()
                }
            }

            // Install button
            Rectangle {
                Layout.preferredWidth: installBtnLabel.width + 24
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: selectedCount > 0
                       ? (installBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary)
                       : Qt.rgba(1, 1, 1, 0.06)
                opacity: selectedCount > 0 ? 1.0 : 0.5
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Install translations")
                activeFocusOnTab: true
                Keys.onReturnPressed: { if (selectedCount > 0) startBatchInstall() }
                Keys.onSpacePressed: { if (selectedCount > 0) startBatchInstall() }

                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Text {
                    id: installBtnLabel
                    anchors.centerIn: parent
                    text: selectedCount > 0 ? qsTr("Çevirileri Kur (%1)").arg(selectedCount) : qsTr("Çevirileri Kur")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: selectedCount > 0 ? "white" : Theme.textMuted
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
                    id: installBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: selectedCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: selectedCount > 0
                    onClicked: startBatchInstall()
                }
            }
        }
    }

    // Category button component
    component CategoryButton: Rectangle {
        property string text: ""
        property string category: ""
        property bool isSelected: false
        signal clicked()

        Accessible.role: Accessible.Button
        Accessible.name: text
        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: catBtnLabel.width + 24
        height: 36
        radius: Dimensions.radiusStandard
        color: isSelected ? Theme.withAlpha(Theme.primary, 0.15) : (catBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
        border.color: isSelected ? Theme.primary : (catBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent")
        border.width: 1

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

        Text {
            id: catBtnLabel
            anchors.centerIn: parent
            text: parent.text
            font.pixelSize: Dimensions.fontBody
            font.weight: isSelected ? Font.DemiBold : Font.Medium
            color: isSelected ? Theme.primary : Theme.textSecondary
        }

        MouseArea {
            id: catBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
