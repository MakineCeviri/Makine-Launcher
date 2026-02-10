/**
 * @file gamelistmodel.h
 * @brief Game List Model for QML
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

#include "../services/corebridge.h"
#include "../makineai_metatypes.h"

namespace makineai {

/**
 * @brief Game List Model
 *
 * QAbstractListModel for displaying detected games in QML.
 * Supports filtering by store, engine, and search text.
 * Supports sorting by name, engine, and recent activity.
 */
class GameListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int filterStore READ filterStore WRITE setFilterStore NOTIFY filterStoreChanged)
    Q_PROPERTY(QString filterEngine READ filterEngine WRITE setFilterEngine NOTIFY filterEngineChanged)
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        InstallPathRole,
        EngineRole,
        SourceRole,
        SteamAppIdRole,
        HeaderImageRole,
        IsVerifiedRole,
        HasTranslationRole,
        TranslationStatusRole
    };
    Q_ENUM(Roles)

    explicit GameListModel(QObject *parent = nullptr);
    ~GameListModel() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Properties
    int count() const { return static_cast<int>(m_filteredGames.size()); }
    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& text);
    int filterStore() const { return static_cast<int>(m_filterStore); }
    void setFilterStore(int store);
    QString filterEngine() const { return m_filterEngine; }
    void setFilterEngine(const QString& engine);
    int sortOrder() const { return static_cast<int>(m_sortOrder); }
    void setSortOrder(int order);
    bool isLoading() const { return m_isLoading; }

    // Q_INVOKABLE methods
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap getGame(int index) const;
    Q_INVOKABLE int findByPath(const QString& path) const;
    Q_INVOKABLE int findById(const QString& id) const;
    Q_INVOKABLE void addGame(const DetectedGame& game);
    Q_INVOKABLE void removeGame(const QString& id);

signals:
    void countChanged();
    void filterTextChanged();
    void filterStoreChanged();
    void filterEngineChanged();
    void sortOrderChanged();
    void isLoadingChanged();
    void gameAdded(const QString& gameId);
    void gameRemoved(const QString& gameId);

private:
    void applyFilters();
    void applySort();
    bool matchesFilter(const DetectedGame& game) const;
    void setupCoreBridge();

    CoreBridge* m_coreBridge{nullptr};
    QList<DetectedGame> m_allGames;
    QList<DetectedGame> m_filteredGames;

    QString m_filterText;
    GameStoreFilter m_filterStore{GameStoreFilter::All};
    QString m_filterEngine;
    GameSortOrder m_sortOrder{GameSortOrder::NameAsc};
    bool m_isLoading{false};
};

} // namespace makineai
