/**
 * @file catalogproxymodel.h
 * @brief High-performance fuzzy search proxy with fzf-style scoring
 *
 * Inherits QAbstractListModel directly (NOT QSortFilterProxyModel) to avoid
 * QSFPM's internal mapping fighting our custom offset/limit/wrap logic.
 *
 * Search features:
 *   - fzf/Sublime-style fuzzy subsequence matching with scoring
 *   - Turkish-aware character normalization (ı→i, ö→o, ü→u, ç→c, ş→s, ğ→g)
 *   - Acronym matching: "gta" → Grand Theft Auto, "rdr" → Red Dead Redemption
 *   - Consecutive match bonus, word boundary bonus, prefix bonus
 *   - Results sorted by relevance score (best match first)
 *   - Pre-built search index (zero-cost per query)
 *
 * Signal forwarding:
 *   - Source modelReset    → proxy rebuild (full)
 *   - Source dataChanged   → proxy dataChanged (mapped, O(1) per row)
 *   - Source rows inserted → proxy rebuild (rare: catalog reload)
 *
 * rowCount() is pure — O(1), no side effects, no const_cast.
 *
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QString>

#include <vector>

namespace makine {

class CatalogProxyModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int rowOffset READ rowOffset WRITE setRowOffset NOTIFY rowOffsetChanged)
    Q_PROPERTY(int rowLimit READ rowLimit WRITE setRowLimit NOTIFY rowLimitChanged)
    Q_PROPERTY(bool wrapAround READ wrapAround WRITE setWrapAround NOTIFY wrapAroundChanged)
    Q_PROPERTY(int sourceCount READ sourceCount NOTIFY sourceCountChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit CatalogProxyModel(QObject *parent = nullptr);

    QAbstractItemModel* sourceModel() const { return m_source; }
    void setSourceModel(QAbstractItemModel *model);

    QString searchFilter() const { return m_searchFilter; }
    void setSearchFilter(const QString &filter);

    int rowOffset() const { return m_rowOffset; }
    void setRowOffset(int offset);

    int rowLimit() const { return m_rowLimit; }
    void setRowLimit(int limit);

    bool wrapAround() const { return m_wrapAround; }
    void setWrapAround(bool wrap);

    /// Number of items after filtering (before offset/limit/wrap).
    int sourceCount() const { return m_filteredCount; }

    // QAbstractListModel interface — all O(1), no side effects
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void sourceModelChanged();
    void searchFilterChanged();
    void rowOffsetChanged();
    void rowLimitChanged();
    void wrapAroundChanged();
    void sourceCountChanged();
    void countChanged();

private:
    // --- Search index entry (pre-computed per game, built once) ---
    struct SearchEntry {
        QString normalized;              // lowercase + Turkish-folded name
        std::vector<int> wordStarts;     // indices where words begin (for boundary bonus)
    };

    // --- Scored row for ranked results ---
    struct ScoredRow {
        int sourceRow;
        int score;
    };

    // --- Core search algorithm ---

    /// Normalize a string: lowercase + Turkish character folding
    static QString normalize(const QString &input);

    /// fzf-style fuzzy score. Returns -1 if no match.
    static int fuzzyScore(const QString &pattern, const SearchEntry &entry);

    // --- Index management ---
    void buildSearchIndex();

    // --- Source model wiring ---
    void connectSource();
    void disconnectSource();
    void rebuild();
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                             const QList<int> &roles);

    int mapToSource(int proxyRow) const;

    // --- State ---
    QAbstractItemModel *m_source{nullptr};
    QString m_searchFilter;
    QString m_normalizedFilter;      // pre-normalized filter for matching
    int m_rowOffset{0};
    int m_rowLimit{-1};
    bool m_wrapAround{false};

    // Search index (built once on source model load)
    std::vector<SearchEntry> m_searchIndex;

    // Pre-computed mapping (rebuilt on filter/offset/limit change)
    QVector<int> m_slicedRows;
    // Reverse map: sourceRow → proxy index in m_slicedRows (-1 if not visible).
    // Built alongside m_slicedRows — enables O(1) onSourceDataChanged dispatch.
    QHash<int, int> m_sourceToProxy;
    int m_filteredCount{0};
    int m_exposedCount{0};
};

} // namespace makine
