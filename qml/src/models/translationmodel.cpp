/**
 * @file translationmodel.cpp
 * @brief Translation Model implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "translationmodel.h"

#include <QDebug>
#include <QtConcurrent>

namespace makineai {

// ========== TranslationModel ==========

TranslationModel::TranslationModel(QObject *parent)
    : QAbstractListModel(parent)
{
    setupCoreBridge();
}

TranslationModel::~TranslationModel() = default;

void TranslationModel::setupCoreBridge()
{
    m_coreBridge = CoreBridge::instance();

    connect(m_coreBridge, &CoreBridge::extractionStarted, this, [this]() {
        m_isLoading = true;
        emit isLoadingChanged();
    });

    connect(m_coreBridge, &CoreBridge::extractionProgress, this, [this](qreal /*progress*/, const QString& /*status*/) {
        // Could emit progress updates here if needed
    });

    connect(m_coreBridge, &CoreBridge::extractionCompleted, this, [this](int count) {
        beginResetModel();
        m_allEntries.clear();
        m_filteredIndices.clear();

        for (const auto& entry : m_coreBridge->extractedStrings()) {
            m_allEntries.append(entry);
        }

        applyFilters();
        endResetModel();

        m_isLoading = false;
        emit isLoadingChanged();
        emit countChanged();
        emit loadCompleted(count);
    });

    connect(m_coreBridge, &CoreBridge::extractionError, this, [this](const QString& error) {
        m_isLoading = false;
        emit isLoadingChanged();
        emit loadError(error);
    });

    connect(m_coreBridge, &CoreBridge::qaCheckCompleted, this, [this](int index, const QAResultQt& result) {
        if (index >= 0 && index < m_allEntries.size()) {
            m_allEntries[index].qaScore = result.score;
            m_allEntries[index].hasIssues = !result.passed;

            int filteredIdx = allToFilteredIndex(index);
            if (filteredIdx >= 0) {
                QModelIndex modelIndex = this->index(filteredIdx);
                emit dataChanged(modelIndex, modelIndex, {QAScoreRole, HasIssuesRole});
            }

            emit qaCheckCompleted(index, result.score, result.passed);
            emit qaScoreChanged();
        }
    });

    connect(m_coreBridge, &CoreBridge::tmMatchFound, this, [this](int index, const TMMatchQt& match) {
        if (index >= 0 && index < m_allEntries.size() && m_allEntries[index].targetText.isEmpty()) {
            m_allEntries[index].targetText = match.targetText;

            int filteredIdx = allToFilteredIndex(index);
            if (filteredIdx >= 0) {
                QModelIndex modelIndex = this->index(filteredIdx);
                emit dataChanged(modelIndex, modelIndex, {TargetTextRole, IsTranslatedRole});
            }

            emit tmMatchApplied(index, match.similarity);
            emit countChanged();
        }
    });
}

int TranslationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_filteredIndices.size());
}

QVariant TranslationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return QVariant();
    }

    int allIndex = m_filteredIndices.at(index.row());
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return QVariant();
    }

    const auto& entry = m_allEntries.at(allIndex);

    switch (role) {
    case EntryKeyRole:
        return entry.entryKey;
    case FilePathRole:
        return entry.filePath;
    case SourceTextRole:
        return entry.sourceText;
    case TargetTextRole:
        return entry.targetText;
    case ContextRole:
        return entry.context;
    case CategoryRole:
        return entry.category;
    case QAScoreRole:
        return entry.qaScore;
    case HasIssuesRole:
        return entry.hasIssues;
    case LineNumberRole:
        return entry.lineNumber;
    case IsTranslatedRole:
        return !entry.targetText.isEmpty();
    case IndexRole:
        return allIndex;
    default:
        return QVariant();
    }
}

bool TranslationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return false;
    }

    int allIndex = m_filteredIndices.at(index.row());
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return false;
    }

    if (role == TargetTextRole) {
        m_allEntries[allIndex].targetText = value.toString();
        emit dataChanged(index, index, {TargetTextRole, IsTranslatedRole});
        emit entryUpdated(allIndex);
        emit countChanged();
        return true;
    }

    return false;
}

Qt::ItemFlags TranslationModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> TranslationModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        {EntryKeyRole, "entryKey"},
        {FilePathRole, "filePath"},
        {SourceTextRole, "sourceText"},
        {TargetTextRole, "targetText"},
        {ContextRole, "context"},
        {CategoryRole, "category"},
        {QAScoreRole, "qaScore"},
        {HasIssuesRole, "hasIssues"},
        {LineNumberRole, "lineNumber"},
        {IsTranslatedRole, "isTranslated"},
        {IndexRole, "entryIndex"}
    };
    return roles;
}

int TranslationModel::translatedCount() const
{
    int count = 0;
    for (const auto& entry : m_allEntries) {
        if (!entry.targetText.isEmpty()) {
            count++;
        }
    }
    return count;
}

int TranslationModel::issueCount() const
{
    int count = 0;
    for (const auto& entry : m_allEntries) {
        if (entry.hasIssues) {
            count++;
        }
    }
    return count;
}

double TranslationModel::progress() const
{
    if (m_allEntries.isEmpty()) {
        return 0.0;
    }
    return static_cast<double>(translatedCount()) / m_allEntries.size() * 100.0;
}

double TranslationModel::averageQAScore() const
{
    return m_averageQAScore;
}

void TranslationModel::setFilterText(const QString& text)
{
    if (m_filterText == text) {
        return;
    }

    m_filterText = text;
    emit filterTextChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
}

void TranslationModel::setFilterStatus(int status)
{
    auto statusFilter = static_cast<EntryStatusFilter>(status);
    if (m_filterStatus == statusFilter) {
        return;
    }

    m_filterStatus = statusFilter;
    emit filterStatusChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
}

void TranslationModel::setFilterCategory(int category)
{
    auto categoryFilter = static_cast<EntryCategoryFilter>(category);
    if (m_filterCategory == categoryFilter) {
        return;
    }

    m_filterCategory = categoryFilter;
    emit filterCategoryChanged();

    beginResetModel();
    applyFilters();
    endResetModel();
}

void TranslationModel::loadForGame(const QString& gameId, const QString& gamePath, const QString& engine)
{
    m_activeGameId = gameId;
    m_activeGamePath = gamePath;
    m_activeEngine = engine;
    emit activeGameChanged();

    if (m_coreBridge) {
        m_coreBridge->extractStrings(gamePath, engine);
    }
}

void TranslationModel::clear()
{
    beginResetModel();
    m_allEntries.clear();
    m_filteredIndices.clear();
    m_activeGameId.clear();
    m_activeGamePath.clear();
    m_activeEngine.clear();
    endResetModel();
    emit countChanged();
    emit activeGameChanged();
}

QVariantMap TranslationModel::getEntry(int index) const
{
    QVariantMap result;

    if (index < 0 || index >= m_filteredIndices.size()) {
        return result;
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return result;
    }

    const auto& entry = m_allEntries.at(allIndex);

    result["entryKey"] = entry.entryKey;
    result["filePath"] = entry.filePath;
    result["sourceText"] = entry.sourceText;
    result["targetText"] = entry.targetText;
    result["context"] = entry.context;
    result["category"] = entry.category;
    result["qaScore"] = entry.qaScore;
    result["hasIssues"] = entry.hasIssues;
    result["lineNumber"] = entry.lineNumber;
    result["isTranslated"] = !entry.targetText.isEmpty();
    result["index"] = allIndex;

    return result;
}

void TranslationModel::setTranslation(int index, const QString& targetText)
{
    if (index < 0 || index >= m_filteredIndices.size()) {
        return;
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return;
    }

    m_allEntries[allIndex].targetText = targetText;

    QModelIndex modelIndex = this->index(index);
    emit dataChanged(modelIndex, modelIndex, {TargetTextRole, IsTranslatedRole});
    emit entryUpdated(allIndex);
    emit countChanged();

    // Add to TM
    if (m_coreBridge && !targetText.isEmpty()) {
        m_coreBridge->addTMEntry(
            m_allEntries[allIndex].sourceText,
            targetText,
            m_activeGameId,
            m_allEntries[allIndex].context
        );
    }
}

void TranslationModel::runQACheck(int index)
{
    if (index < 0 || index >= m_filteredIndices.size()) {
        return;
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return;
    }

    const auto& entry = m_allEntries.at(allIndex);
    if (entry.targetText.isEmpty()) {
        return;
    }

    if (m_coreBridge) {
        auto result = m_coreBridge->performQACheck(
            entry.sourceText,
            entry.targetText,
            m_activeGameId,
            true
        );

        m_allEntries[allIndex].qaScore = result.score;
        m_allEntries[allIndex].hasIssues = !result.passed;

        QModelIndex modelIndex = this->index(index);
        emit dataChanged(modelIndex, modelIndex, {QAScoreRole, HasIssuesRole});
        emit qaCheckCompleted(allIndex, result.score, result.passed);
        emit qaScoreChanged();
    }
}

void TranslationModel::applyGlossary(int index)
{
    if (index < 0 || index >= m_filteredIndices.size()) {
        return;
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return;
    }

    auto& entry = m_allEntries[allIndex];

    // Apply glossary to the target text (or source if no target yet)
    QString textToProcess = entry.targetText.isEmpty() ? entry.sourceText : entry.targetText;

    if (m_coreBridge) {
        QString result = m_coreBridge->applyGlossary(textToProcess, m_activeGameId);
        if (result != textToProcess) {
            entry.targetText = result;

            QModelIndex modelIndex = this->index(index);
            emit dataChanged(modelIndex, modelIndex, {TargetTextRole, IsTranslatedRole});
            emit entryUpdated(allIndex);
            emit countChanged();
        }
    }
}

QList<TMMatchQt> TranslationModel::getTMMatches(int index, int limit)
{
    if (index < 0 || index >= m_filteredIndices.size()) {
        return {};
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return {};
    }

    if (m_coreBridge) {
        return m_coreBridge->findTMMatches(
            m_allEntries[allIndex].sourceText,
            m_activeGameId,
            m_activeEngine,
            limit
        );
    }

    return {};
}

QList<GlossaryTermQt> TranslationModel::getGlossaryTerms(int index)
{
    if (index < 0 || index >= m_filteredIndices.size()) {
        return {};
    }

    int allIndex = m_filteredIndices.at(index);
    if (allIndex < 0 || allIndex >= m_allEntries.size()) {
        return {};
    }

    if (m_coreBridge) {
        return m_coreBridge->findTermsInText(
            m_allEntries[allIndex].sourceText,
            m_activeGameId
        );
    }

    return {};
}

QVariantList TranslationModel::getQAIssues(int index) const
{
    QVariantList result;

    // For now, return empty list - QA issues stored separately
    // Could store QAIssueQt list in entry if needed

    Q_UNUSED(index);
    return result;
}

void TranslationModel::applyFilters()
{
    m_filteredIndices.clear();

    for (int i = 0; i < m_allEntries.size(); ++i) {
        if (matchesFilter(m_allEntries.at(i))) {
            m_filteredIndices.append(i);
        }
    }
}

bool TranslationModel::matchesFilter(const TranslationEntryQt& entry) const
{
    // Text filter
    if (!m_filterText.isEmpty()) {
        bool matches = entry.sourceText.contains(m_filterText, Qt::CaseInsensitive) ||
                       entry.targetText.contains(m_filterText, Qt::CaseInsensitive) ||
                       entry.entryKey.contains(m_filterText, Qt::CaseInsensitive) ||
                       entry.context.contains(m_filterText, Qt::CaseInsensitive);
        if (!matches) {
            return false;
        }
    }

    // Status filter
    switch (m_filterStatus) {
    case EntryStatusFilter::Untranslated:
        if (!entry.targetText.isEmpty()) {
            return false;
        }
        break;
    case EntryStatusFilter::Translated:
        if (entry.targetText.isEmpty()) {
            return false;
        }
        break;
    case EntryStatusFilter::HasIssues:
        if (!entry.hasIssues) {
            return false;
        }
        break;
    default:
        break;
    }

    // Category filter
    if (m_filterCategory != EntryCategoryFilter::All) {
        QString categoryStr;
        switch (m_filterCategory) {
        case EntryCategoryFilter::Dialog:
            categoryStr = "dialog";
            break;
        case EntryCategoryFilter::UI:
            categoryStr = "ui";
            break;
        case EntryCategoryFilter::Item:
            categoryStr = "item";
            break;
        case EntryCategoryFilter::Skill:
            categoryStr = "skill";
            break;
        case EntryCategoryFilter::System:
            categoryStr = "system";
            break;
        case EntryCategoryFilter::Narration:
            categoryStr = "narration";
            break;
        case EntryCategoryFilter::Other:
            categoryStr = "other";
            break;
        default:
            break;
        }
        if (!categoryStr.isEmpty() && entry.category != categoryStr) {
            return false;
        }
    }

    return true;
}

int TranslationModel::filteredToAllIndex(int filteredIndex) const
{
    if (filteredIndex < 0 || filteredIndex >= m_filteredIndices.size()) {
        return -1;
    }
    return m_filteredIndices.at(filteredIndex);
}

int TranslationModel::allToFilteredIndex(int allIndex) const
{
    return m_filteredIndices.indexOf(allIndex);
}

} // namespace makineai
