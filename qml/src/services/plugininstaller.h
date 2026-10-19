// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include "pluginmanifest.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

/**
 * Plugin folders on disk. Layout under AppPaths::pluginsDir():
 *
 *   <id>/                 installed plugin (manifest.json + files)
 *   .staging/<random>/    package being checked before it replaces anything
 *   .previous/<id>/       the version an update replaced, until the new one starts
 *   .trash/<random>/      a removed plugin whose files were still locked
 *
 * Every step that replaces a plugin is a rename, so a failure at any point
 * leaves either the old or the new version in place, never a mix.
 */
namespace plugininstaller {

struct StagedPlugin {
    QString dir;
    PluginManifest manifest;
};

/// Extracts a .mkplugin (MKPK v2, unencrypted) into a new folder under
/// stagingRoot and checks its manifest and entry DLL.
std::optional<StagedPlugin> stage(const QByteArray& package, const QString& stagingRoot,
                                  QString* error);

/// Moves the staged plugin to <pluginsRoot>/<id>; an installed version moves to
/// .previous/<id> first.
bool commit(const StagedPlugin& staged, const QString& pluginsRoot, QString* error);

/// Brings .previous/<id> back as the installed version.
bool rollback(const QString& pluginsRoot, const QString& id);

/// Forgets the version an update replaced, once the new one is known to work.
void dropPrevious(const QString& pluginsRoot, const QString& id);

/// Removes <pluginsRoot>/<id>.
bool remove(const QString& pluginsRoot, const QString& id, QString* error);

/// Clears leftovers of interrupted installs (.staging, .trash).
void cleanUp(const QString& pluginsRoot);

struct InstalledFolder {
    QString dir;
    std::optional<PluginManifest> manifest;
    QString error;   // why manifest is empty
};

/// Installed plugin folders. A manifest whose id differs from its folder name is
/// reported as an error rather than trusted.
QList<InstalledFolder> scan(const QString& pluginsRoot);

} // namespace plugininstaller
