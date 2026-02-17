import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

Flow {
    id: statsRoot
    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL
    Layout.rightMargin: Dimensions.marginXL
    spacing: Dimensions.spacingMD

    // Required properties from parent
    required property bool hasSteamDetails
    required property int metacriticScore
    required property string price
    required property int discountPercent
    required property var genres
    required property bool hasWindows
    required property bool hasMac
    required property bool hasLinux

    visible: hasSteamDetails

    // Metacritic
    Rectangle {
        visible: statsRoot.metacriticScore > 0
        width: mcRow.width + 20; height: 30
        radius: Dimensions.radiusFull
        property color mc: statsRoot.metacriticScore >= 75 ? Theme.scoreExcellent : statsRoot.metacriticScore >= 50 ? Theme.scoreFair : Theme.scorePoor
        color: Theme.withAlpha(mc, 0.10)
        border.color: Theme.withAlpha(mc, 0.20); border.width: 1
        Row {
            id: mcRow; anchors.centerIn: parent; spacing: Dimensions.spacingSM
            Text { text: "Metacritic"; font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
            Text { text: statsRoot.metacriticScore.toString(); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: parent.parent.mc; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // Price
    Rectangle {
        visible: statsRoot.price !== ""
        width: priceRow.width + 20; height: 30
        radius: Dimensions.radiusFull
        property color pc: statsRoot.discountPercent > 0 ? Theme.success : Theme.textSecondary
        color: Theme.withAlpha(pc, 0.10)
        border.color: Theme.withAlpha(pc, 0.20); border.width: 1
        Row {
            id: priceRow; anchors.centerIn: parent; spacing: Dimensions.spacingSM
            Text { visible: statsRoot.discountPercent > 0; text: "-" + statsRoot.discountPercent + "%"; font.pixelSize: Dimensions.fontCaption; font.weight: Font.Bold; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
            Text { text: statsRoot.price; font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: parent.parent.pc; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // Genres
    Rectangle {
        visible: statsRoot.genres.length > 0
        width: genreText.width + 20; height: 30
        radius: Dimensions.radiusFull
        color: Theme.withAlpha(Theme.textPrimary, 0.05)
        border.color: Theme.withAlpha(Theme.textPrimary, 0.08); border.width: 1
        Text { id: genreText; anchors.centerIn: parent; text: statsRoot.genres.slice(0, 2).join(", "); font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary }
    }

    // Platforms
    Rectangle {
        visible: statsRoot.hasWindows || statsRoot.hasMac || statsRoot.hasLinux
        width: platText.width + 20; height: 30
        radius: Dimensions.radiusFull
        color: Theme.withAlpha(Theme.textPrimary, 0.05)
        border.color: Theme.withAlpha(Theme.textPrimary, 0.08); border.width: 1
        Text {
            id: platText; anchors.centerIn: parent
            text: { var p = []; if (statsRoot.hasWindows) p.push("Win"); if (statsRoot.hasMac) p.push("Mac"); if (statsRoot.hasLinux) p.push("Linux"); return p.join(" / ") }
            font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary
        }
    }
}
