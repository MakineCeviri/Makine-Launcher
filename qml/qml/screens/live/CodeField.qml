// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * CodeField — a short input (the TV's 4-digit code, an address, a passphrase),
 * styled like the plugin settings' fields.
 */
TextField {
    id: cf
    implicitHeight: 34
    leftPadding: 10
    rightPadding: 10
    horizontalAlignment: TextInput.AlignHCenter
    font.pixelSize: Dimensions.body
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    background: Rectangle {
        radius: Dimensions.radiusMD
        color: Theme.primary06
        border.color: cf.activeFocus ? Theme.primary : Theme.primary12
        border.width: 1
    }
}
