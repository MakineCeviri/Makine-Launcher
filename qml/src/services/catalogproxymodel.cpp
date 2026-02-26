/**
 * @file catalogproxymodel.cpp
 * @brief Proxy model for catalog filtering, slicing, and wrapAround
 *
 * Inherits QAbstractListModel directly. All mapping is pre-computed in rebuild().
 * rowCount() and data() are pure O(1) lookups — no lazy rebuild, no const_cast.
 *
 * Signal forwarding:
 *   - Source modelReset / rowsInserted / rowsRemoved → full rebuild()
 *   - Source dataChanged → mapped proxy dataChanged (O(1) per affected row)
 *
 * @copyright (c) 2026 MakineAI Team
 */

#include "catalogproxymodel.h"
#include "supportedgamesmodel.h"

namespace makineai {

CatalogProxyModel::CatalogProxyModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// --- Source model management ---

void CatalogProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (m_source == model)
        return;

    disconnectSource();
    m_source = model;
    connectSource();

    emit sourceModelChanged();
    rebuild();
}

void CatalogProxyModel::connectSource()
{
    if (!m_source)
        return;

    // Full rebuild on structural changes (rare: catalog load/reload)
    connect(m_source, &QAbstractItemModel::modelReset,
            this, &CatalogProxyModel::rebuild);
    connect(m_source, &QAbstractItemModel::rowsInserted,
            this, &CatalogProxyModel::rebuild);
    connect(m_source, &QAbstractItemModel::rowsRemoved,
            this, &CatalogProxyModel::rebuild);

    // Granular forwarding for data changes (frequent: install/package status)
    connect(m_source, &QAbstractItemModel::dataChanged,
            this, &CatalogProxyModel::onSourceDataChanged);
}

void CatalogProxyModel::disconnectSource()
{
    if (!m_source)
        return;
    disconnect(m_source, nullptr, this, nullptr);
}

// --- Property setters ---

void CatalogProxyModel::setSearchFilter(const QString &filter)
{
    if (m_searchFilter == filter)
        return;
    m_searchFilter = filter;
    emit searchFilterChanged();
    rebuild();
}

void CatalogProxyModel::setRowOffset(int offset)
{
    if (m_rowOffset == offset)
        return;
    m_rowOffset = offset;
    emit rowOffsetChanged();
    rebuild();
}

void CatalogProxyModel::setRowLimit(int limit)
{
    if (m_rowLimit == limit)
        return;
    m_rowLimit = limit;
    emit rowLimitChanged();
    rebuild();
}

void CatalogProxyModel::setWrapAround(bool wrap)
{
    if (m_wrapAround == wrap)
        return;
    m_wrapAround = wrap;
    emit wrapAroundChanged();
    rebuild();
}

// --- QAbstractListModel interface ---

int CatalogProxyModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_exposedCount;
}

QVariant CatalogProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_source)
        return {};

    int sourceRow = mapToSource(index.row());
    if (sourceRow < 0)
        return {};

    return m_source->data(m_source->index(sourceRow, 0), role);
}

QHash<int, QByteArray> CatalogProxyModel::roleNames() const
{
    if (m_source)
        return m_source->roleNames();
    return {};
}

// --- Mapping logic ---

void CatalogProxyModel::rebuild()
{
    beginResetModel();

    int oldSourceCount = m_filteredRows.size();
    int oldExposedCount = m_exposedCount;

    // Step 1: collect filtered source rows
    m_filteredRows.clear();
    if (m_source) {
        int srcCount = m_source->rowCount();
        m_filteredRows.reserve(srcCount);
        for (int i = 0; i < srcCount; ++i) {
            if (acceptsRow(i))
                m_filteredRows.append(i);
        }
    }

    // Step 2: apply offset + limit to get the visible slice
    m_slicedRows.clear();
    int total = m_filteredRows.size();
    int offset = qBound(0, m_rowOffset, total);
    int limit = (m_rowLimit < 0) ? (total - offset) : qMin(m_rowLimit, total - offset);

    m_slicedRows.reserve(limit);
    for (int i = offset; i < offset + limit; ++i)
        m_slicedRows.append(m_filteredRows[i]);

    // Step 3: compute exposed count (wrapAround doubles the slice)
    int sliced = m_slicedRows.size();
    m_exposedCount = (sliced > 0 && m_wrapAround) ? sliced * 2 : sliced;

    endResetModel();

    if (m_filteredRows.size() != oldSourceCount)
        emit sourceCountChanged();
    if (m_exposedCount != oldExposedCount)
        emit countChanged();
}

bool CatalogProxyModel::acceptsRow(int sourceRow) const
{
    if (m_searchFilter.isEmpty())
        return true;

    QModelIndex idx = m_source->index(sourceRow, 0);
    QString name = m_source->data(idx, SupportedGamesModel::NameRole).toString();
    return name.contains(m_searchFilter, Qt::CaseInsensitive);
}

int CatalogProxyModel::mapToSource(int proxyRow) const
{
    int sliced = m_slicedRows.size();
    if (sliced == 0)
        return -1;

    int mappedRow = proxyRow % sliced;
    if (mappedRow < 0 || mappedRow >= sliced)
        return -1;

    return m_slicedRows[mappedRow];
}

void CatalogProxyModel::onSourceDataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight,
    const QList<int> &roles)
{
    // Map each affected source row to proxy row(s) and forward dataChanged.
    // This is the critical path for package install/uninstall — O(1) per row.

    int sliced = m_slicedRows.size();
    if (sliced == 0)
        return;

    for (int srcRow = topLeft.row(); srcRow <= bottomRight.row(); ++srcRow) {
        // Find this source row in our sliced mapping
        // Linear scan over slice (typically 129 items) — fast enough
        for (int i = 0; i < sliced; ++i) {
            if (m_slicedRows[i] == srcRow) {
                // Found in first copy — emit for this position
                QModelIndex proxyIdx = index(i, 0);
                emit dataChanged(proxyIdx, proxyIdx, roles);

                // If wrapAround, also emit for the mirrored position in second copy
                if (m_wrapAround) {
                    QModelIndex mirrorIdx = index(i + sliced, 0);
                    emit dataChanged(mirrorIdx, mirrorIdx, roles);
                }
                break; // source rows are unique in slice
            }
        }
    }
}

} // namespace makineai
