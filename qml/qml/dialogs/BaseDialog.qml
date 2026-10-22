// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * BaseDialog.qml - The launcher's sheet: an elevated rounded card over a
 * dimmed page, centred, opening with a short settle.
 *
 * Subclass and set header/contentItem/footer as needed.
 *
 * Properties:
 *   accentColor — the dialog's tone (icon, main button); default the accent
 *
 * Signals:
 *   cancelled() — emitted on Escape or close button
 */
Dialog {
    id: root

    property color accentColor: Theme.primary

    signal cancelled()

    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0
    topPadding: 0
    bottomPadding: 0

    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.animMedium; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Dimensions.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.2 }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animFast }
            NumberAnimation { property: "scale"; from: 1; to: 0.97; duration: Dimensions.animFast }
        }
    }

    background: Item {
        RectangularShadow {
            anchors.fill: sheet
            radius: sheet.radius
            offset.y: 12
            blur: 40
            spread: -4
            color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.55 : 0.22)
        }
        Rectangle {
            id: sheet
            anchors.fill: parent
            radius: Dimensions.radiusSheet
            color: Theme.surfaceElevated
            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.06)
            border.width: 1
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.scrim
        Behavior on opacity { NumberAnimation { duration: Dimensions.animMedium } }
    }

    // Escape under CloseOnEscape rejects a Dialog. A Keys handler never ran:
    // a Popup is not an Item, so Escape closed the dialog without a word and
    // an update flow stayed pending into the next install.
    onRejected: root.cancelled()
}
