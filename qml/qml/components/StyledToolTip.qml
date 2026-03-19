import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0

/**
 * StyledToolTip — Modern, minimal tooltip matching app design.
 */
ToolTip {
    id: tip
    delay: 400
    timeout: 4000

    contentItem: Text {
        text: tip.text
        font.pixelSize: Dimensions.fontCaption
        font.weight: Font.Medium
        color: Theme.textPrimary
        textFormat: Text.PlainText
    }

    background: Rectangle {
        color: Theme.surface50
        border.color: Theme.primary12
        border.width: 1
        radius: Dimensions.radiusSM
    }

    padding: Dimensions.paddingXS
    leftPadding: Dimensions.paddingSM
    rightPadding: Dimensions.paddingSM
}
