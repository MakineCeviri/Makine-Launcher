import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * BaseDialog.qml - Shared dialog base with glassmorphism, transitions, and modal overlay
 *
 * Provides: enter/exit transitions, glass background, modal dimming, centering, Escape key.
 * Subclass and set header/contentItem/footer as needed.
 *
 * Properties:
 *   accentColor — border accent (default: Theme.accent)
 *
 * Signals:
 *   cancelled() — emitted on Escape or close button
 */
Dialog {
    id: root

    property color accentColor: Theme.accent

    signal cancelled()

    modal: true
    closePolicy: Popup.CloseOnEscape

    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.animMedium; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: Dimensions.animMedium; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animFast }
            NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: Dimensions.animFast }
        }
    }

    background: Rectangle {
        radius: Dimensions.radiusMD
        color: Theme.glassBackground
        border.color: Theme.withAlpha(root.accentColor, 0.15)
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: "transparent"
            border.color: Theme.glassHighlight
            border.width: 1
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.bgPrimary60
        Behavior on opacity { NumberAnimation { duration: Dimensions.animMedium } }
    }

    Keys.onEscapePressed: { root.cancelled(); root.close() }
}
