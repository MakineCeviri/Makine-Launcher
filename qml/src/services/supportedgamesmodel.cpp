/**
 * @file supportedgamesmodel.cpp
 * @brief QAbstractListModel implementation for supported games catalog
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "supportedgamesmodel.h"
#include <QVariantMap>

namespace makine {

SupportedGamesModel::SupportedGamesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SupportedGamesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant SupportedGamesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const auto &e = m_entries[index.row()];
    switch (role) {
    case GameIdRole:          return e.gameId;
    case NameRole:            return e.name;
    case SteamAppIdRole:      return e.steamAppId;
    case InstallPathRole:     return e.installPath;
    case SourceRole:          return e.source;
    case EngineRole:          return e.engine;
    case IsInstalledRole:     return e.isInstalled;
    case PackageInstalledRole:return e.packageInstalled;
    case ImageUrlRole:        return e.imageUrl;
    case DataUrlRole:         return e.dataUrl;
    case DownloadSizeRole:    return e.downloadSize;
    case ExternalUrlRole:     return e.externalUrl;
    default:                  return {};
    }
}

QHash<int, QByteArray> SupportedGamesModel::roleNames() const
{
    return {
        {GameIdRole,           "gameId"},
        {NameRole,             "name"},
        {SteamAppIdRole,       "steamAppId"},
        {InstallPathRole,      "installPath"},
        {SourceRole,           "source"},
        {EngineRole,           "engine"},
        {IsInstalledRole,      "isInstalled"},
        {PackageInstalledRole, "packageInstalled"},
        {ImageUrlRole,         "imageUrl"},
        {DataUrlRole,          "dataUrl"},
        {DownloadSizeRole,     "downloadSize"},
        {ExternalUrlRole,      "externalUrl"},
    };
}

void SupportedGamesModel::resetFromCatalog(const QVariantList &catalogData)
{
    beginResetModel();
    m_entries.clear();
    m_appIdToIndex.clear();
    m_entries.reserve(catalogData.size());
    m_appIdToIndex.reserve(catalogData.size());

    for (const auto &item : catalogData) {
        const QVariantMap map = item.toMap();
        CatalogEntry e;
        e.steamAppId    = map.value(QStringLiteral("steamAppId")).toString();
        e.name          = map.value(QStringLiteral("name")).toString();
        e.gameId        = map.value(QStringLiteral("id"), e.steamAppId).toString();
        e.source        = map.value(QStringLiteral("source")).toString();
        e.engine        = map.value(QStringLiteral("engine")).toString();
        e.imageUrl      = map.value(QStringLiteral("imageUrl")).toString();
        e.dataUrl       = map.value(QStringLiteral("dataUrl")).toString();
        e.downloadSize  = map.value(QStringLiteral("downloadSize")).toLongLong();
        e.externalUrl   = map.value(QStringLiteral("externalUrl")).toString();
        e.isInstalled   = map.value(QStringLiteral("isInstalled")).toBool();
        e.installPath   = map.value(QStringLiteral("installPath")).toString();
        e.packageInstalled = map.value(QStringLiteral("packageInstalled")).toBool();

        if (!e.steamAppId.isEmpty()) {
            m_appIdToIndex[e.steamAppId] = m_entries.size();
        }
        m_entries.append(std::move(e));
    }

    endResetModel();
    emit countChanged();
}

void SupportedGamesModel::updateInstallStatus(const QString &steamAppId, bool installed,
                                               const QString &path, const QString &id,
                                               const QString &source, const QString &engine)
{
    auto it = m_appIdToIndex.constFind(steamAppId);
    if (it == m_appIdToIndex.constEnd())
        return;

    int row = *it;
    auto &e = m_entries[row];
    bool changed = false;

    if (e.isInstalled != installed) { e.isInstalled = installed; changed = true; }
    if (!path.isEmpty() && e.installPath != path) { e.installPath = path; changed = true; }
    if (!id.isEmpty() && e.gameId != id) { e.gameId = id; changed = true; }
    if (!source.isEmpty() && e.source != source) { e.source = source; changed = true; }
    if (!engine.isEmpty() && e.engine != engine) { e.engine = engine; changed = true; }

    if (changed) {
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {GameIdRole, InstallPathRole, SourceRole, EngineRole, IsInstalledRole});
    }
}

void SupportedGamesModel::updatePackageStatus(const QString &steamAppId, bool packageInstalled)
{
    auto it = m_appIdToIndex.constFind(steamAppId);
    if (it == m_appIdToIndex.constEnd())
        return;

    int row = *it;
    auto &e = m_entries[row];
    if (e.packageInstalled == packageInstalled)
        return;

    e.packageInstalled = packageInstalled;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {PackageInstalledRole});
}

} // namespace makine
