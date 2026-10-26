// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TopDissolve — a scrolled page fades out under the window's top strip, and
 * at the panel's foot while more of it waits below, instead of being cut
 * there by a straight line.
 *
 * The page itself is masked (EdgeFade), so what is under it — the lit
 * background — shows through exactly. Only while there is something to
 * fade: a page that fits, at rest, is not layered and costs nothing.
 *
 *   TopDissolve { flick: page }
 */
Item {
    id: dissolve

    property Flickable flick: null
    property real size: 56
    property real footSize: 48

    // Draws nothing itself
    height: 0

    EdgeFade {
        target: dissolve.flick
        topFade: dissolve.flick && dissolve.flick.contentY > 4 ? dissolve.size : 0
        bottomFade: dissolve.flick && !dissolve.flick.atYEnd
                    && dissolve.flick.contentHeight > dissolve.flick.height + 4 ? dissolve.footSize : 0
    }
}
