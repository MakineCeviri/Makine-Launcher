/**
 * @file catalogproxymodel.cpp
 * @brief High-performance fuzzy search proxy with fzf-style scoring
 *
 * Search engine inspired by fzf, Sublime Text, and VS Code file finder:
 *   1. Pre-built index: normalized names + word boundary positions (built once)
 *   2. Fuzzy subsequence matching with multi-signal scoring
 *   3. Results ranked by relevance (best match first during search)
 *   4. Turkish-aware normalization (ı→i, ö→o, ü→u, ç→c, ş→s, ğ→g)
 *
 * Scoring signals:
 *   - Exact substring match: highest tier (+200 base)
 *   - Prefix match: +50 bonus
 *   - Word boundary hit: +15 per char at word start
 *   - Consecutive chars: +8 per consecutive (compounds)
 *   - Acronym match: all chars hit word boundaries
 *   - Gap penalty: -2 per gap character
 *   - Pattern coverage: bonus for matching larger fraction of name
 *
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "catalogproxymodel.h"
#include "supportedgamesmodel.h"

#include <algorithm>
#include <vector>

namespace makine {

// =============================================================================
// Turkish-aware normalization
// =============================================================================

QString CatalogProxyModel::normalize(const QString &input)
{
    QString out;
    out.reserve(input.size());

    for (const QChar ch : input) {
        const char16_t c = ch.unicode();

        // Fast path: pure ASCII (covers the vast majority of game names)
        // Avoids QChar::toLower() virtual dispatch for the common case.
        if (c < 0x80) {
            out += (c >= u'A' && c <= u'Z') ? QChar(char16_t(c + 32)) : ch;
            continue;
        }

        // Turkish special characters → ASCII equivalents (sparse check)
        switch (c) {
        case 0x0131: out += u'i'; break;  // ı → i
        case 0x0130: out += u'i'; break;  // İ → i
        case 0x00F6: out += u'o'; break;  // ö → o
        case 0x00D6: out += u'o'; break;  // Ö → o
        case 0x00FC: out += u'u'; break;  // ü → u
        case 0x00DC: out += u'u'; break;  // Ü → u
        case 0x00E7: out += u'c'; break;  // ç → c
        case 0x00C7: out += u'c'; break;  // Ç → c
        case 0x015F: out += u's'; break;  // ş → s
        case 0x015E: out += u's'; break;  // Ş → s
        case 0x011F: out += u'g'; break;  // ğ → g
        case 0x011E: out += u'g'; break;  // Ğ → g
        default:
            out += ch.toLower();
            break;
        }
    }
    return out;
}

// =============================================================================
// Search index builder
// =============================================================================

void CatalogProxyModel::buildSearchIndex()
{
    m_searchIndex.clear();
    if (!m_source)
        return;

    const int count = m_source->rowCount();
    m_searchIndex.reserve(count);

    for (int i = 0; i < count; ++i) {
        SearchEntry entry;
        const QString name = m_source->data(
            m_source->index(i, 0), SupportedGamesModel::NameRole).toString();

        entry.normalized = normalize(name);

        // Pre-compute word boundary positions
        const int len = entry.normalized.size();
        if (len > 0)
            entry.wordStarts.push_back(0); // first char is always a word start

        for (int j = 1; j < len; ++j) {
            const QChar prev = entry.normalized[j - 1];
            // Word boundary: after space, dash, underscore, colon, dot, apostrophe
            if (prev == u' ' || prev == u'-' || prev == u'_' ||
                prev == u':' || prev == u'.' || prev == u'\'' ||
                prev == u'(' || prev == u')') {
                entry.wordStarts.push_back(j);
            }
        }

        m_searchIndex.push_back(std::move(entry));
    }
}

// =============================================================================
// fzf-style fuzzy scoring
// =============================================================================

int CatalogProxyModel::fuzzyScore(const QString &pattern, const SearchEntry &entry)
{
    if (pattern.isEmpty())
        return 1; // empty pattern matches everything

    const QString &text = entry.normalized;
    const int pLen = pattern.size();
    const int tLen = text.size();

    if (pLen > tLen)
        return -1; // pattern longer than text — impossible match

    // --- Quick reject: check if all pattern chars exist in text ---
    {
        int pi = 0;
        for (int ti = 0; ti < tLen && pi < pLen; ++ti) {
            if (text[ti] == pattern[pi])
                ++pi;
        }
        if (pi < pLen)
            return -1; // not all chars found — no match
    }

    // --- Tier 1: Exact substring match (highest score) ---
    const int exactPos = text.indexOf(pattern);
    if (exactPos >= 0) {
        int score = 200 + pLen * 10;

        // Prefix bonus
        if (exactPos == 0)
            score += 50;

        // Word boundary bonus for exact match start position
        for (const int ws : entry.wordStarts) {
            if (ws == exactPos) {
                score += 30;
                break;
            }
        }

        // Full match bonus (pattern == entire text)
        if (pLen == tLen)
            score += 100;

        // Coverage bonus: pattern is a large fraction of text
        score += (pLen * 20) / tLen;

        return score;
    }

    // --- Tier 2: Fuzzy subsequence match with scoring ---
    //
    // Greedy forward scan that prefers:
    //   1. Word boundary hits
    //   2. Consecutive characters
    //   3. Early positions
    //
    // For each pattern char, scan forward in text. If the next matching char
    // is at a word boundary, prefer it (look ahead up to 8 chars).

    int score = 0;
    int pi = 0;
    int lastMatchPos = -1;
    int consecutiveCount = 0;
    int wordBoundaryHits = 0;

    for (int ti = 0; ti < tLen && pi < pLen; ++ti) {
        if (text[ti] != pattern[pi])
            continue;

        // Look-ahead: is there a word boundary match within 8 chars?
        bool usedLookahead = false;
        if (lastMatchPos >= 0 && (ti - lastMatchPos) > 1) {
            // We have a gap — check if a better (word boundary) match exists ahead
            for (int look = ti + 1; look < tLen && look <= ti + 8; ++look) {
                if (text[look] == pattern[pi]) {
                    bool isWordStart = false;
                    for (const int ws : entry.wordStarts) {
                        if (ws == look) { isWordStart = true; break; }
                        if (ws > look) break;
                    }
                    if (isWordStart) {
                        // Skip to word boundary match
                        ti = look;
                        usedLookahead = true;
                        break;
                    }
                }
            }
        }

        // Score this match
        const int pos = ti;
        score += 10; // base match point

        // Word boundary bonus
        bool atWordStart = false;
        for (const int ws : entry.wordStarts) {
            if (ws == pos) { atWordStart = true; break; }
            if (ws > pos) break;
        }
        if (atWordStart) {
            score += 15;
            ++wordBoundaryHits;
        }

        // First character bonus
        if (pos == 0)
            score += 20;

        // Consecutive match bonus (compounds: 8, 16, 24...)
        if (lastMatchPos == pos - 1) {
            ++consecutiveCount;
            score += consecutiveCount * 8;
        } else {
            consecutiveCount = 0;
            // Gap penalty: penalize distance from last match
            if (lastMatchPos >= 0) {
                int gap = pos - lastMatchPos - 1;
                score -= std::min(gap * 2, 20); // cap penalty at -20
            }
        }

        lastMatchPos = pos;
        ++pi;
    }

    if (pi < pLen)
        return -1; // didn't match all pattern chars

    // Acronym bonus: if ALL pattern chars hit word boundaries
    if (wordBoundaryHits == pLen && pLen >= 2)
        score += 40;

    // Coverage bonus: longer patterns matching get a boost
    score += (pLen * 15) / tLen;

    return score;
}

// =============================================================================
// Constructor
// =============================================================================

CatalogProxyModel::CatalogProxyModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// =============================================================================
// Source model management
// =============================================================================

void CatalogProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (m_source == model)
        return;

    disconnectSource();
    m_source = model;
    connectSource();
    buildSearchIndex();

    emit sourceModelChanged();
    rebuild();
}

void CatalogProxyModel::connectSource()
{
    if (!m_source)
        return;

    // Full rebuild on structural changes (rare: catalog load/reload)
    connect(m_source, &QAbstractItemModel::modelReset, this, [this]() {
        buildSearchIndex();
        rebuild();
    });
    connect(m_source, &QAbstractItemModel::rowsInserted, this, [this]() {
        buildSearchIndex();
        rebuild();
    });
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, [this]() {
        buildSearchIndex();
        rebuild();
    });

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

// =============================================================================
// Property setters
// =============================================================================

void CatalogProxyModel::setSearchFilter(const QString &filter)
{
    if (m_searchFilter == filter)
        return;
    m_searchFilter = filter;
    m_normalizedFilter = normalize(filter);
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

// =============================================================================
// QAbstractListModel interface
// =============================================================================

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

// =============================================================================
// Mapping + rebuild
// =============================================================================

void CatalogProxyModel::rebuild()
{
    beginResetModel();

    const int oldFilteredCount = m_filteredCount;
    const int oldExposedCount = m_exposedCount;

    m_slicedRows.clear();
    m_sourceToProxy.clear();
    m_filteredCount = 0;

    if (!m_source || m_searchIndex.empty()) {
        m_exposedCount = 0;
        endResetModel();
        if (oldFilteredCount != 0) emit sourceCountChanged();
        if (oldExposedCount != 0) emit countChanged();
        return;
    }

    const int srcCount = static_cast<int>(m_searchIndex.size());

    if (m_normalizedFilter.isEmpty()) {
        // No filter — pass all rows in original order
        m_filteredCount = srcCount;

        const int offset = qBound(0, m_rowOffset, srcCount);
        const int limit = (m_rowLimit < 0)
            ? (srcCount - offset)
            : qMin(m_rowLimit, srcCount - offset);

        m_slicedRows.reserve(limit);
        m_sourceToProxy.reserve(limit);
        for (int i = offset; i < offset + limit; ++i) {
            m_sourceToProxy[i] = m_slicedRows.size();
            m_slicedRows.append(i);
        }
    } else {
        // Score all entries and collect matches
        std::vector<ScoredRow> scored;
        scored.reserve(srcCount);

        for (int i = 0; i < srcCount; ++i) {
            const int s = fuzzyScore(m_normalizedFilter, m_searchIndex[i]);
            if (s > 0)
                scored.push_back({i, s});
        }

        // Sort by score descending (best match first)
        std::sort(scored.begin(), scored.end(),
                  [](const ScoredRow &a, const ScoredRow &b) {
                      return a.score > b.score;
                  });

        m_filteredCount = static_cast<int>(scored.size());

        // Apply offset + limit
        const int total = m_filteredCount;
        const int offset = qBound(0, m_rowOffset, total);
        const int limit = (m_rowLimit < 0)
            ? (total - offset)
            : qMin(m_rowLimit, total - offset);

        m_slicedRows.reserve(limit);
        m_sourceToProxy.reserve(limit);
        for (int i = offset; i < offset + limit; ++i) {
            m_sourceToProxy[scored[i].sourceRow] = m_slicedRows.size();
            m_slicedRows.append(scored[i].sourceRow);
        }
    }

    // Compute exposed count (wrapAround doubles the visible rows)
    const int sliced = m_slicedRows.size();
    m_exposedCount = (sliced > 0 && m_wrapAround) ? sliced * 2 : sliced;

    endResetModel();

    if (m_filteredCount != oldFilteredCount)
        emit sourceCountChanged();
    if (m_exposedCount != oldExposedCount)
        emit countChanged();
}

int CatalogProxyModel::mapToSource(int proxyRow) const
{
    const int sliced = m_slicedRows.size();
    if (sliced == 0)
        return -1;

    const int mappedRow = proxyRow % sliced;
    if (mappedRow < 0 || mappedRow >= sliced)
        return -1;

    return m_slicedRows[mappedRow];
}

void CatalogProxyModel::onSourceDataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight,
    const QList<int> &roles)
{
    // O(1) per source row via m_sourceToProxy reverse map.
    // Previously O(n * m_slicedRows.size()) — ~67,600 comparisons for 260 games.
    if (m_sourceToProxy.isEmpty())
        return;

    const int sliced = m_slicedRows.size();
    for (int srcRow = topLeft.row(); srcRow <= bottomRight.row(); ++srcRow) {
        auto it = m_sourceToProxy.constFind(srcRow);
        if (it == m_sourceToProxy.constEnd())
            continue;

        const int proxyRow = it.value();
        QModelIndex proxyIdx = index(proxyRow, 0);
        emit dataChanged(proxyIdx, proxyIdx, roles);

        if (m_wrapAround) {
            QModelIndex mirrorIdx = index(proxyRow + sliced, 0);
            emit dataChanged(mirrorIdx, mirrorIdx, roles);
        }
    }
}

} // namespace makine
