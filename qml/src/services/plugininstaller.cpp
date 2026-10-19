// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "plugininstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QUuid>

#include <optional>

#ifndef MAKINE_UI_ONLY
#include "mkpkformat.h"
#endif

namespace {

constexpr qsizetype kMaxPackageSize = 200LL * 1024 * 1024;

QString randomName()
{
    return QUuid::createUuid().toString(QUuid::Id128).left(12);
}

// A host that just exited can hold its DLL open for a moment longer.
bool renameWithRetry(const QString& from, const QString& to)
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (QDir().rename(from, to)) return true;
        QThread::msleep(100);
    }
    return false;
}

std::optional<PluginManifest> readManifest(const QString& dir, QString* error)
{
    QFile file(dir + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("manifest.json is missing");
        return std::nullopt;
    }
    auto manifest = PluginManifest::parse(file.readAll(), error);
    if (manifest && !QFileInfo(dir + u'/' + manifest->entry).isFile()) {
        *error = QStringLiteral("the package does not contain its entry %1").arg(manifest->entry);
        return std::nullopt;
    }
    return manifest;
}

} // namespace

namespace plugininstaller {

std::optional<StagedPlugin> stage(const QByteArray& package, const QString& stagingRoot, QString* error)
{
    if (package.size() < 5 || !package.startsWith("MKPK") || package.at(4) != 2) {
        *error = QStringLiteral("not a plugin package (MKPK v2)");
        return std::nullopt;
    }
    if (package.size() > kMaxPackageSize) {
        *error = QStringLiteral("package is larger than 200 MB");
        return std::nullopt;
    }
#ifndef MAKINE_UI_ONLY
    const QString dir = stagingRoot + u'/' + randomName();
    if (!QDir().mkpath(dir)) {
        *error = QStringLiteral("cannot create %1").arg(dir);
        return std::nullopt;
    }
    makine::mkpk::MkpkError mkpkError{""};
    const int files = makine::mkpk::process_mkpkg(reinterpret_cast<const uint8_t*>(package.constData()),
                                                  static_cast<size_t>(package.size()),
                                                  dir.toStdWString(), &mkpkError);
    std::optional<PluginManifest> manifest;
    if (files < 0)
        *error = QStringLiteral("extraction failed: %1").arg(QString::fromStdString(mkpkError.message));
    else
        manifest = readManifest(dir, error);
    if (!manifest) {
        QDir(dir).removeRecursively();
        return std::nullopt;
    }
    return StagedPlugin{dir, *manifest};
#else
    Q_UNUSED(stagingRoot)
    *error = QStringLiteral("plugin packages need the full build");
    return std::nullopt;
#endif
}

bool commit(const StagedPlugin& staged, const QString& pluginsRoot, QString* error)
{
    const QString target = pluginsRoot + u'/' + staged.manifest.id;
    const QString previous = pluginsRoot + QStringLiteral("/.previous/") + staged.manifest.id;

    QDir(previous).removeRecursively();
    QDir().mkpath(pluginsRoot + QStringLiteral("/.previous"));
    const bool hadInstalled = QFileInfo::exists(target);
    if (hadInstalled && !renameWithRetry(target, previous)) {
        *error = QStringLiteral("the installed version is in use and cannot be replaced");
        return false;
    }
    if (!renameWithRetry(staged.dir, target)) {
        if (hadInstalled) renameWithRetry(previous, target);
        *error = QStringLiteral("cannot move the new version into place");
        return false;
    }
    return true;
}

bool rollback(const QString& pluginsRoot, const QString& id)
{
    const QString target = pluginsRoot + u'/' + id;
    const QString previous = pluginsRoot + QStringLiteral("/.previous/") + id;
    if (!QFileInfo::exists(previous)) return false;
    const QString trash = pluginsRoot + QStringLiteral("/.trash/") + randomName();
    QDir().mkpath(pluginsRoot + QStringLiteral("/.trash"));
    if (QFileInfo::exists(target) && !renameWithRetry(target, trash)) return false;
    QDir(trash).removeRecursively();
    return renameWithRetry(previous, target);
}

void dropPrevious(const QString& pluginsRoot, const QString& id)
{
    QDir(pluginsRoot + QStringLiteral("/.previous/") + id).removeRecursively();
}

bool remove(const QString& pluginsRoot, const QString& id, QString* error)
{
    const QString target = pluginsRoot + u'/' + id;
    if (!QFileInfo::exists(target)) return true;
    // Rename first: a delete that fails half-way must not leave a broken plugin
    // that still looks installed.
    const QString trash = pluginsRoot + QStringLiteral("/.trash/") + randomName();
    QDir().mkpath(pluginsRoot + QStringLiteral("/.trash"));
    if (!renameWithRetry(target, trash)) {
        *error = QStringLiteral("plugin files are in use");
        return false;
    }
    QDir(trash).removeRecursively();
    dropPrevious(pluginsRoot, id);
    return true;
}

void cleanUp(const QString& pluginsRoot)
{
    QDir(pluginsRoot + QStringLiteral("/.staging")).removeRecursively();
    QDir(pluginsRoot + QStringLiteral("/.trash")).removeRecursively();
}

QList<InstalledFolder> scan(const QString& pluginsRoot)
{
    QList<InstalledFolder> found;
    const auto names = QDir(pluginsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& name : names) {
        if (name.startsWith(u'.')) continue;
        InstalledFolder folder{pluginsRoot + u'/' + name, std::nullopt, {}};
        folder.manifest = readManifest(folder.dir, &folder.error);
        if (folder.manifest && folder.manifest->id != name) {
            folder.error = QStringLiteral("manifest id %1 does not match folder %2").arg(folder.manifest->id, name);
            folder.manifest.reset();
        }
        found.append(folder);
    }
    return found;
}

} // namespace plugininstaller
