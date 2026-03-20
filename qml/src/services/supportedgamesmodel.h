/**
 * @file supportedgamesmodel.h
 * @brief QAbstractListModel for the 258-game supported catalog
 *
 * Replaces QVariantList supportedGames() to eliminate 6.3s QML freeze.
 * ListView receives granular dataChanged() instead of full list replacement.
 *
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace makine {

struct CatalogEntry {
    QString gameId;
    QString name;
    QString steamAppId;
    QString installPath;
    QString source;
    QString engine;
    QString imageUrl;
    QString dataUrl;
    QString externalUrl;
    qint64 downloadSize{0};
    bool isInstalled{false};
    bool packageInstalled{false};
};

class SupportedGamesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        GameIdRole = Qt::UserRole + 1,
        NameRole,
        SteamAppIdRole,
        InstallPathRole,
        SourceRole,
        EngineRole,
        IsInstalledRole,
        PackageInstalledRole,
        ImageUrlRole,
        DataUrlRole,
        DownloadSizeRole,
        ExternalUrlRole
    };
    Q_ENUM(Role)

    explicit SupportedGamesModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Replace entire catalog (called once on catalog load / remote sync).
    void resetFromCatalog(const QVariantList &catalogData);

    /// Update install status for a single game (O(1) via hash lookup).
    void updateInstallStatus(const QString &steamAppId, bool installed,
                             const QString &path, const QString &id = {},
                             const QString &source = {}, const QString &engine = {});

    /// Update package installed status for a single game (O(1)).
    void updatePackageStatus(const QString &steamAppId, bool packageInstalled);

signals:
    void countChanged();

private:
    QVector<CatalogEntry> m_entries;
    QHash<QString, int> m_appIdToIndex; // steamAppId -> row index
};

} // namespace makine
