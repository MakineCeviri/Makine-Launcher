/**
 * @file gamelistmodel.cpp
 * @brief Game List Model implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "gamelistmodel.h"

#include <QDebug>
#include <algorithm>

namespace makineai {

// ========== GameListModel ==========

GameListModel::GameListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    setupCoreBridge();
}

GameListModel::~GameListModel() = default;

void GameListModel::setupCoreBridge()
{
    m_coreBridge = CoreBridge::instance();

    connect(m_coreBridge, &CoreBridge::scanStarted, this, [this]() {
        m_isLoading = true;
        emit isLoadingChanged();
    });

    connect(m_coreBridge, &CoreBridge::scanCompleted, this, [this](int /*count*/) {
        beginResetModel();
        m_allGames.clear();
        for (const auto& game : m_coreBridge->detectedGames()) {
            m_allGames.append(game);
        }
        applyFilters();
        endResetModel();

        m_isLoading = false;
        emit isLoadingChanged();
        emit countChanged();
    });

    connect(m_coreBridge, &CoreBridge::gameDetected, this, [this](const QString& gameId, const QString& /*gameName*/) {
        // Find the game in coreBridge and add it
        for (const auto& game : m_coreBridge->detectedGames()) {
            if (game.id == gameId) {
                addGame(game);
                break;
            }
        }
    });
}

int GameListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_filteredGames.size());
}

QVariant GameListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredGames.size()) {
        return QVariant();
    }

    const auto& game = m_filteredGames.at(index.row());

    switch (role) {
    case IdRole:
        return game.id;
    case NameRole:
        return game.name;
    case InstallPathRole:
        return game.installPath;
    case EngineRole:
        return game.engine;
    case SourceRole:
        return game.source;
    case SteamAppIdRole:
        return game.steamAppId;
    case HeaderImageRole:
        return game.headerImageUrl;
    case IsVerifiedRole:
        return game.isVerified;
    case HasTranslationRole:
        return game.hasTranslation;
    case TranslationStatusRole:
        return game.translationStatus;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> GameListModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        {IdRole, "gameId"},
        {NameRole, "name"},
        {InstallPathRole, "installPath"},
        {EngineRole, "engine"},
        {SourceRole, "source"},
        {SteamAppIdRole, "steamAppId"},
        {HeaderImageRole, "headerImage"},
        {IsVerifiedRole, "isVerified"},
        {HasTranslationRole, "hasTranslation"},
        {TranslationStatusRole, "translationStatus"}
    };
    return roles;
}

void GameListModel::setFilterText(const QString& text)
{
    if (m_filterText == text) {
        return;
    }

    m_filterText = text;
    emit filterTextChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
    emit countChanged();
}

void GameListModel::setFilterStore(int store)
{
    auto storeFilter = static_cast<GameStoreFilter>(store);
    if (m_filterStore == storeFilter) {
        return;
    }

    m_filterStore = storeFilter;
    emit filterStoreChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
    emit countChanged();
}

void GameListModel::setFilterEngine(const QString& engine)
{
    if (m_filterEngine == engine) {
        return;
    }

    m_filterEngine = engine;
    emit filterEngineChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
    emit countChanged();
}

void GameListModel::setSortOrder(int order)
{
    auto sortOrder = static_cast<GameSortOrder>(order);
    if (m_sortOrder == sortOrder) {
        return;
    }

    m_sortOrder = sortOrder;
    emit sortOrderChanged();

    beginResetModel();
    applySort();
    endResetModel();
}

void GameListModel::refresh()
{
    if (m_coreBridge) {
        m_coreBridge->scanAllLibraries();
    }
}

void GameListModel::clear()
{
    beginResetModel();
    m_allGames.clear();
    m_filteredGames.clear();
    endResetModel();
    emit countChanged();
}

QVariantMap GameListModel::getGame(int index) const
{
    QVariantMap result;

    if (index < 0 || index >= m_filteredGames.size()) {
        return result;
    }

    const auto& game = m_filteredGames.at(index);

    result["id"] = game.id;
    result["name"] = game.name;
    result["installPath"] = game.installPath;
    result["engine"] = game.engine;
    result["source"] = game.source;
    result["steamAppId"] = game.steamAppId;
    result["headerImageUrl"] = game.headerImageUrl;
    result["isVerified"] = game.isVerified;
    result["hasTranslation"] = game.hasTranslation;
    result["translationStatus"] = game.translationStatus;

    return result;
}

int GameListModel::findByPath(const QString& path) const
{
    for (int i = 0; i < m_filteredGames.size(); ++i) {
        if (m_filteredGames.at(i).installPath == path) {
            return i;
        }
    }
    return -1;
}

int GameListModel::findById(const QString& id) const
{
    for (int i = 0; i < m_filteredGames.size(); ++i) {
        if (m_filteredGames.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void GameListModel::addGame(const DetectedGame& game)
{
    // Check if already exists
    for (const auto& existing : m_allGames) {
        if (existing.id == game.id) {
            return;
        }
    }

    m_allGames.append(game);

    if (matchesFilter(game)) {
        beginInsertRows(QModelIndex(), m_filteredGames.size(), m_filteredGames.size());
        m_filteredGames.append(game);
        applySort();
        endInsertRows();
        emit countChanged();
    }

    emit gameAdded(game.id);
}

void GameListModel::removeGame(const QString& id)
{
    // Remove from all games
    for (int i = 0; i < m_allGames.size(); ++i) {
        if (m_allGames.at(i).id == id) {
            m_allGames.removeAt(i);
            break;
        }
    }

    // Remove from filtered games
    for (int i = 0; i < m_filteredGames.size(); ++i) {
        if (m_filteredGames.at(i).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_filteredGames.removeAt(i);
            endRemoveRows();
            emit countChanged();
            break;
        }
    }

    emit gameRemoved(id);
}

void GameListModel::applyFilters()
{
    m_filteredGames.clear();

    for (const auto& game : m_allGames) {
        if (matchesFilter(game)) {
            m_filteredGames.append(game);
        }
    }

    applySort();
}

void GameListModel::applySort()
{
    std::sort(m_filteredGames.begin(), m_filteredGames.end(),
        [this](const DetectedGame& a, const DetectedGame& b) {
            switch (m_sortOrder) {
            case GameSortOrder::NameAsc:
                return a.name.toLower() < b.name.toLower();
            case GameSortOrder::NameDesc:
                return a.name.toLower() > b.name.toLower();
            case GameSortOrder::EngineAsc:
                return a.engine < b.engine;
            case GameSortOrder::EngineDesc:
                return a.engine > b.engine;
            case GameSortOrder::RecentFirst:
                // For now, just sort by name - could add lastPlayed tracking later
                return a.name.toLower() < b.name.toLower();
            default:
                return a.name.toLower() < b.name.toLower();
            }
        });
}

bool GameListModel::matchesFilter(const DetectedGame& game) const
{
    // Text filter
    if (!m_filterText.isEmpty()) {
        bool matches = game.name.contains(m_filterText, Qt::CaseInsensitive) ||
                       game.engine.contains(m_filterText, Qt::CaseInsensitive) ||
                       game.installPath.contains(m_filterText, Qt::CaseInsensitive);
        if (!matches) {
            return false;
        }
    }

    // Store filter
    if (m_filterStore != GameStoreFilter::All) {
        QString storeStr;
        switch (m_filterStore) {
        case GameStoreFilter::Steam:
            storeStr = "steam";
            break;
        case GameStoreFilter::Epic:
            storeStr = "epic";
            break;
        case GameStoreFilter::GOG:
            storeStr = "gog";
            break;
        case GameStoreFilter::Manual:
            storeStr = "manual";
            break;
        default:
            break;
        }
        if (!storeStr.isEmpty() && game.source != storeStr) {
            return false;
        }
    }

    // Engine filter
    if (!m_filterEngine.isEmpty() && game.engine != m_filterEngine) {
        return false;
    }

    return true;
}

// ========== FilteredGameListModel ==========

FilteredGameListModel::FilteredGameListModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FilteredGameListModel::setFilterText(const QString& text)
{
    if (m_filterText == text) {
        return;
    }

    beginFilterChange();
    m_filterText = text;
    endFilterChange();
    emit filterTextChanged();
}

void FilteredGameListModel::setShowOnlyWithTranslation(bool show)
{
    if (m_showOnlyWithTranslation == show) {
        return;
    }

    beginFilterChange();
    m_showOnlyWithTranslation = show;
    endFilterChange();
    emit showOnlyWithTranslationChanged();
}

bool FilteredGameListModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    // Text filter
    if (!m_filterText.isEmpty()) {
        QString name = index.data(GameListModel::NameRole).toString();
        QString engine = index.data(GameListModel::EngineRole).toString();
        QString path = index.data(GameListModel::InstallPathRole).toString();

        bool matches = name.contains(m_filterText, Qt::CaseInsensitive) ||
                       engine.contains(m_filterText, Qt::CaseInsensitive) ||
                       path.contains(m_filterText, Qt::CaseInsensitive);
        if (!matches) {
            return false;
        }
    }

    // Translation filter
    if (m_showOnlyWithTranslation) {
        bool hasTranslation = index.data(GameListModel::HasTranslationRole).toBool();
        if (!hasTranslation) {
            return false;
        }
    }

    return true;
}

bool FilteredGameListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QString leftName = left.data(GameListModel::NameRole).toString();
    QString rightName = right.data(GameListModel::NameRole).toString();
    return leftName.toLower() < rightName.toLower();
}

} // namespace makineai
