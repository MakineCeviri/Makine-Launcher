// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

struct PluginCapability {
    QString id;
    QString reason;   // shown to the user when asking for consent
};

struct PluginAction {
    QString id;
    QString label;
    QString description;
    QString method;   // what invoke() receives when the button is pressed
};

/**
 * manifest.json of a v2 plugin (docs/plugins/manifest.md). parse() accepts a
 * manifest only when everything the launcher relies on is valid, so the rest of
 * the code never re-checks fields.
 */
struct PluginManifest {
    QString id;            // reverse-DNS, also the folder name
    QString name;
    QString version;       // semver
    QString description;
    QString publisher;
    QString license;
    QString homepage;
    QString entry;         // DLL path relative to the plugin folder
    QString minLauncher;
    int abi = 0;
    QList<PluginCapability> capabilities;
    QList<QJsonObject> settings;   // key, type, label, description?, default?, options?, min?, max?
    QList<PluginAction> actions;
    QStringList events;

    static std::optional<PluginManifest> parse(const QByteArray& json, QString* error = nullptr);

    bool hasCapability(const QString& capability) const;
    QStringList capabilityIds() const;
    QJsonObject setting(const QString& key) const;   // empty when unknown
};

namespace pluginmanifest {

/// Capabilities a manifest may declare. Anything else is rejected, so a plugin
/// that needs something newer never runs on a launcher that cannot honour it.
const QStringList& knownCapabilities();
/// Events the launcher sends to plugins that list them.
const QStringList& knownEvents();
/// Setting types the launcher can render.
const QStringList& settingTypes();

bool isValidId(const QString& id);        // reverse-DNS, safe as a folder name
bool isValidVersion(const QString& version);   // semver

/// Dotted numeric versions; a pre-release ("0.2.0-beta") sorts before its release.
int compareVersions(const QString& a, const QString& b);
/// ABI MIN_VERSION..VERSION: older plugins keep working on newer launchers.
bool isSupportedAbi(int abi);

/// Whether `value` is acceptable for `setting` (type, options, range).
bool isValidSettingValue(const QJsonObject& setting, const QJsonValue& value);

} // namespace pluginmanifest
