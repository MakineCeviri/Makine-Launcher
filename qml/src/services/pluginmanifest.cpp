// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginmanifest.h"

#include <makine/plugin/abi.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <optional>

namespace {

const QRegularExpression& idPattern()
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9][a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+$"));
    return re;
}

const QRegularExpression& versionPattern()
{
    static const QRegularExpression re(QStringLiteral("^\\d+\\.\\d+\\.\\d+(-[0-9A-Za-z.-]+)?$"));
    return re;
}

const QRegularExpression& keyPattern()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,63}$"));
    return re;
}

std::nullopt_t reject(QString* error, const QString& message)
{
    if (error) *error = message;
    return std::nullopt;
}

bool isSafeEntry(const QString& entry)
{
    if (entry.isEmpty() || !entry.endsWith(QLatin1String(".dll"), Qt::CaseInsensitive))
        return false;
    if (entry.startsWith(u'/') || entry.startsWith(u'\\') || entry.contains(u':'))
        return false;
    const auto parts = QString(entry).replace(u'\\', u'/').split(u'/');
    for (const auto& part : parts)
        if (part.isEmpty() || part == QLatin1String("..") || part == QLatin1String("."))
            return false;
    return true;
}

// Checks one setting and returns it with select options normalised to {value, label}.
std::optional<QJsonObject> parseSetting(const QJsonObject& in, QString* error)
{
    QJsonObject s = in;
    const QString key = s.value(u"key").toString();
    const QString type = s.value(u"type").toString();
    if (!keyPattern().match(key).hasMatch())
        return reject(error, QStringLiteral("setting key \"%1\" is invalid").arg(key));
    if (!pluginmanifest::settingTypes().contains(type))
        return reject(error, QStringLiteral("setting %1: unknown type \"%2\"").arg(key, type));
    if (s.value(u"label").toString().trimmed().isEmpty())
        return reject(error, QStringLiteral("setting %1: label is missing").arg(key));

    if (type == QLatin1String("select")) {
        QJsonArray options;
        for (const auto& o : s.value(u"options").toArray()) {
            const QString value = o.isString() ? o.toString() : o.toObject().value(u"value").toString();
            const QString label = o.isString() ? o.toString() : o.toObject().value(u"label").toString(value);
            if (value.isEmpty())
                return reject(error, QStringLiteral("setting %1: option without a value").arg(key));
            options.append(QJsonObject{{"value", value}, {"label", label}});
        }
        if (options.isEmpty())
            return reject(error, QStringLiteral("setting %1: select needs options").arg(key));
        s.insert(u"options", options);
    }
    if (type == QLatin1String("number") && s.contains(u"min") && s.contains(u"max")
        && s.value(u"min").toDouble() > s.value(u"max").toDouble())
        return reject(error, QStringLiteral("setting %1: min is above max").arg(key));
    if (type == QLatin1String("path")) {
        const QString mode = s.value(u"mode").toString(QStringLiteral("folder"));
        if (mode != QLatin1String("folder") && mode != QLatin1String("file"))
            return reject(error, QStringLiteral("setting %1: mode must be file or folder").arg(key));
        s.insert(u"mode", mode);
    }
    if (s.contains(u"default")) {
        if (type == QLatin1String("secret"))
            return reject(error, QStringLiteral("setting %1: a secret cannot have a default").arg(key));
        if (!pluginmanifest::isValidSettingValue(s, s.value(u"default")))
            return reject(error, QStringLiteral("setting %1: default does not fit the type").arg(key));
    }
    return s;
}

} // namespace

namespace pluginmanifest {

const QStringList& knownCapabilities()
{
    static const QStringList list = {
        QStringLiteral("network"),          // talks to the internet
        QStringLiteral("screen-capture"),   // reads pixels of the screen or a window
        QStringLiteral("overlay"),          // draws over games through the launcher overlay
        QStringLiteral("game-files-read"),  // gets game install paths and reads files there
        QStringLiteral("game-files-write"), // changes files in game folders
        QStringLiteral("process-inject"),   // attaches to a running game process
    };
    return list;
}

const QStringList& knownEvents()
{
    static const QStringList list = {
        QStringLiteral("game.started"),
        QStringLiteral("game.stopped"),
        QStringLiteral("translation.installed"),
        QStringLiteral("translation.uninstalled"),
        QStringLiteral("settings.changed"),
    };
    return list;
}

const QStringList& settingTypes()
{
    static const QStringList list = {
        QStringLiteral("toggle"), QStringLiteral("select"), QStringLiteral("text"),
        QStringLiteral("secret"), QStringLiteral("number"), QStringLiteral("path"),
    };
    return list;
}

bool isValidId(const QString& id)
{
    return id.size() <= 64 && idPattern().match(id).hasMatch();
}

bool isValidVersion(const QString& version)
{
    return versionPattern().match(version).hasMatch();
}

int compareVersions(const QString& a, const QString& b)
{
    const QString coreA = a.section(u'-', 0, 0), preA = a.section(u'-', 1);
    const QString coreB = b.section(u'-', 0, 0), preB = b.section(u'-', 1);
    const auto partsA = coreA.split(u'.'), partsB = coreB.split(u'.');
    for (qsizetype i = 0; i < qMax(partsA.size(), partsB.size()); ++i) {
        const int va = i < partsA.size() ? partsA[i].toInt() : 0;
        const int vb = i < partsB.size() ? partsB[i].toInt() : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    if (preA.isEmpty() != preB.isEmpty()) return preA.isEmpty() ? 1 : -1;
    return preA.compare(preB) < 0 ? -1 : (preA == preB ? 0 : 1);
}

bool isSupportedAbi(int abi)
{
    return abi >= static_cast<int>(MAKINE_PLUGIN_ABI_MIN_VERSION) && abi <= static_cast<int>(MAKINE_PLUGIN_ABI_VERSION);
}

bool isValidSettingValue(const QJsonObject& setting, const QJsonValue& value)
{
    const QString type = setting.value(u"type").toString();
    if (type == QLatin1String("toggle")) return value.isBool();
    if (type == QLatin1String("number")) {
        if (!value.isDouble()) return false;
        const double v = value.toDouble();
        if (setting.contains(u"min") && v < setting.value(u"min").toDouble()) return false;
        if (setting.contains(u"max") && v > setting.value(u"max").toDouble()) return false;
        return true;
    }
    if (!value.isString() || value.toString().size() > 4096) return false;
    if (type == QLatin1String("select")) {
        for (const auto& o : setting.value(u"options").toArray())
            if (o.toObject().value(u"value").toString() == value.toString()) return true;
        return false;
    }
    return true;   // text, secret, path
}

} // namespace pluginmanifest

std::optional<PluginManifest> PluginManifest::parse(const QByteArray& json, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return reject(error, QStringLiteral("manifest.json is not a JSON object"));
    const QJsonObject o = doc.object();

    if (o.value(u"schema").toInt() != 2)
        return reject(error, QStringLiteral("manifest schema must be 2"));

    PluginManifest m;
    m.id = o.value(u"id").toString();
    m.name = o.value(u"name").toString().trimmed();
    m.version = o.value(u"version").toString();
    m.description = o.value(u"description").toString().trimmed();
    m.publisher = o.value(u"publisher").toString().trimmed();
    m.license = o.value(u"license").toString();
    m.homepage = o.value(u"homepage").toString();
    m.entry = o.value(u"entry").toString();
    m.minLauncher = o.value(u"minLauncher").toString(QStringLiteral("0.0.0"));
    m.abi = o.value(u"abi").toInt();

    if (!pluginmanifest::isValidId(m.id))
        return reject(error, QStringLiteral("id \"%1\" is not reverse-DNS (e.g. org.example.plugin)").arg(m.id));
    if (m.name.isEmpty() || m.name.size() > 64)
        return reject(error, QStringLiteral("name is missing or longer than 64 characters"));
    if (!pluginmanifest::isValidVersion(m.version))
        return reject(error, QStringLiteral("version \"%1\" is not semver").arg(m.version));
    if (!pluginmanifest::isValidVersion(m.minLauncher))
        return reject(error, QStringLiteral("minLauncher \"%1\" is not semver").arg(m.minLauncher));
    if (!pluginmanifest::isSupportedAbi(m.abi))
        return reject(error, QStringLiteral("abi %1 is not supported (this launcher speaks %2 to %3)")
                               .arg(m.abi).arg(MAKINE_PLUGIN_ABI_MIN_VERSION).arg(MAKINE_PLUGIN_ABI_VERSION));
    if (!isSafeEntry(m.entry))
        return reject(error, QStringLiteral("entry \"%1\" must be a .dll inside the plugin folder").arg(m.entry));
    if (m.publisher.isEmpty())
        return reject(error, QStringLiteral("publisher is missing"));
    if (m.description.size() > 500)
        return reject(error, QStringLiteral("description is longer than 500 characters"));

    QSet<QString> seen;
    for (const auto& c : o.value(u"capabilities").toArray()) {
        const PluginCapability cap{c.toObject().value(u"id").toString(),
                                   c.toObject().value(u"reason").toString().trimmed()};
        if (!pluginmanifest::knownCapabilities().contains(cap.id))
            return reject(error, QStringLiteral("unknown capability \"%1\"").arg(cap.id));
        if (cap.reason.isEmpty())
            return reject(error, QStringLiteral("capability %1 needs a reason").arg(cap.id));
        if (seen.contains(cap.id))
            return reject(error, QStringLiteral("capability %1 is listed twice").arg(cap.id));
        seen.insert(cap.id);
        m.capabilities.append(cap);
    }

    const QJsonObject contributes = o.value(u"contributes").toObject();
    seen.clear();
    for (const auto& s : contributes.value(u"settings").toArray()) {
        auto setting = parseSetting(s.toObject(), error);
        if (!setting) return std::nullopt;
        const QString key = setting->value(u"key").toString();
        if (seen.contains(key))
            return reject(error, QStringLiteral("setting %1 is listed twice").arg(key));
        seen.insert(key);
        m.settings.append(*setting);
    }

    seen.clear();
    for (const auto& a : contributes.value(u"actions").toArray()) {
        const QJsonObject ao = a.toObject();
        PluginAction action{ao.value(u"id").toString(), ao.value(u"label").toString().trimmed(),
                            ao.value(u"description").toString(), ao.value(u"method").toString()};
        if (action.method.isEmpty()) action.method = action.id;
        if (!keyPattern().match(action.id).hasMatch() || action.label.isEmpty())
            return reject(error, QStringLiteral("action \"%1\" needs a valid id and a label").arg(action.id));
        if (seen.contains(action.id))
            return reject(error, QStringLiteral("action %1 is listed twice").arg(action.id));
        seen.insert(action.id);
        m.actions.append(action);
    }

    for (const auto& e : contributes.value(u"events").toArray()) {
        if (!pluginmanifest::knownEvents().contains(e.toString()))
            return reject(error, QStringLiteral("unknown event \"%1\"").arg(e.toString()));
        if (!m.events.contains(e.toString())) m.events.append(e.toString());
    }

    return m;
}

bool PluginManifest::hasCapability(const QString& capability) const
{
    for (const auto& c : capabilities)
        if (c.id == capability) return true;
    return false;
}

QStringList PluginManifest::capabilityIds() const
{
    QStringList ids;
    for (const auto& c : capabilities) ids.append(c.id);
    return ids;
}

QJsonObject PluginManifest::setting(const QString& key) const
{
    for (const auto& s : settings)
        if (s.value(u"key").toString() == key) return s;
    return {};
}
