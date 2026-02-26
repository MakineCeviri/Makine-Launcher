/**
 * @file catalogproxymodel.h
 * @brief Lightweight proxy model for catalog search, row splitting, and wrapAround
 *
 * Inherits QAbstractListModel directly (NOT QSortFilterProxyModel) to avoid
 * QSFPM's internal mapping fighting our custom offset/limit/wrap logic.
 *
 * Signal forwarding:
 *   - Source modelReset    → proxy beginResetModel/endResetModel (full rebuild)
 *   - Source dataChanged   → proxy dataChanged (mapped, O(1) per row)
 *   - Source rows inserted → proxy beginResetModel/endResetModel (rare: catalog reload)
 *
 * rowCount() is pure — O(1), no side effects, no const_cast.
 *
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QAbstractListModel>
#include <QVector>

namespace makineai {

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
    int sourceCount() const { return m_filteredRows.size(); }

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
    void connectSource();
    void disconnectSource();
    void rebuild(); // full mapping rebuild with beginResetModel/endResetModel
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                             const QList<int> &roles);

    bool acceptsRow(int sourceRow) const;
    int mapToSource(int proxyRow) const; // proxy row → source row, O(1)

    QAbstractItemModel *m_source{nullptr};
    QString m_searchFilter;
    int m_rowOffset{0};
    int m_rowLimit{-1}; // -1 = no limit
    bool m_wrapAround{false};

    // Pre-computed mapping (rebuilt on filter/offset/limit/source change)
    QVector<int> m_filteredRows; // source rows that pass filter
    QVector<int> m_slicedRows;   // filteredRows[offset..offset+limit]
    int m_exposedCount{0};       // final count: sliced * (wrap ? 2 : 1)
};

} // namespace makineai
