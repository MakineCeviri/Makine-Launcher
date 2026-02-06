import QtQuick
import QtQuick.Layouts
import MakineAI 1.0

/**
 * MetacriticScoreBadge.qml - Metacritic puan rozeti
 */
Rectangle {
    id: root
    property int score: 0

    Layout.preferredWidth: 48
    Layout.preferredHeight: 48
    radius: Dimensions.radiusStandard

    color: {
        if (root.score >= 75) return Theme.scoreExcellent
        if (root.score >= 50) return Theme.scoreFair
        if (root.score > 0) return Theme.scoreBad
        return Theme.textMuted
    }

    Text {
        anchors.centerIn: parent
        text: root.score > 0 ? root.score : "--"
        font.pixelSize: 20
        font.weight: Font.Bold
        color: "white"
    }
}
