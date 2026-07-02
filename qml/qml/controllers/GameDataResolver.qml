// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameDataResolver.qml - Resolves game metadata for detail screen navigation
 *
 * All business logic moved to GameService.resolveGameData() (C++).
 * This controller is a thin QML delegate.
 */
QtObject {
    id: resolver

    function resolve(gameId, gameName, installPath, engine, forceAutoInstall) {
        return GameService.resolveGameData(gameId, gameName, installPath, engine, forceAutoInstall)
    }
}
