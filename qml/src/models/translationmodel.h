/**
 * @file translationmodel.h
 * @brief Translation Model for QML
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

#include "../services/corebridge.h"
#include "../makineai_metatypes.h"

namespace makineai {

/**
 * @brief Translation Model
 *
 * QAbstractListModel for displaying translation entries in QML.
 * Supports filtering by status, category, and search text.
 */
class TranslationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(int translatedCount READ translatedCount NOTIFY countChanged)
    Q_PROPERTY(int issueCount READ issueCount NOTIFY countChanged)
    Q_PROPERTY(double progress READ progress NOTIFY countChanged)
    Q_PROPERTY(double averageQAScore READ averageQAScore NOTIFY qaScoreChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int filterStatus READ filterStatus WRITE setFilterStatus NOTIFY filterStatusChanged)
    Q_PROPERTY(int filterCategory READ filterCategory WRITE setFilterCategory NOTIFY filterCategoryChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QString activeGameId READ activeGameId NOTIFY activeGameChanged)

public:
    enum Roles {
        EntryKeyRole = Qt::UserRole + 1,
        FilePathRole,
        SourceTextRole,
        TargetTextRole,
        ContextRole,
        CategoryRole,
        QAScoreRole,
        HasIssuesRole,
        LineNumberRole,
        IsTranslatedRole,
        IndexRole
    };
    Q_ENUM(Roles)

    explicit TranslationModel(QObject *parent = nullptr);
    ~TranslationModel() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Properties
    int totalCount() const { return static_cast<int>(m_allEntries.size()); }
    int translatedCount() const;
    int issueCount() const;
    double progress() const;
    double averageQAScore() const;
    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& text);
    int filterStatus() const { return static_cast<int>(m_filterStatus); }
    void setFilterStatus(int status);
    int filterCategory() const { return static_cast<int>(m_filterCategory); }
    void setFilterCategory(int category);
    bool isLoading() const { return m_isLoading; }
    QString activeGameId() const { return m_activeGameId; }

    // Q_INVOKABLE methods
    Q_INVOKABLE void loadForGame(const QString& gameId, const QString& gamePath, const QString& engine);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap getEntry(int index) const;
    Q_INVOKABLE void setTranslation(int index, const QString& targetText);
    Q_INVOKABLE void runQACheck(int index);
    Q_INVOKABLE void runAllQAChecks();
    Q_INVOKABLE void applyTMMatches();
    Q_INVOKABLE void applyGlossary(int index);
    Q_INVOKABLE QList<TMMatchQt> getTMMatches(int index, int limit = 5);
    Q_INVOKABLE QList<GlossaryTermQt> getGlossaryTerms(int index);
    Q_INVOKABLE QVariantList getQAIssues(int index) const;

signals:
    void countChanged();
    void qaScoreChanged();
    void filterTextChanged();
    void filterStatusChanged();
    void filterCategoryChanged();
    void isLoadingChanged();
    void activeGameChanged();
    void entryUpdated(int index);
    void qaCheckCompleted(int index, int score, bool passed);
    void tmMatchApplied(int index, double similarity);
    void loadCompleted(int totalCount);
    void loadError(const QString& error);

private:
    void applyFilters();
    bool matchesFilter(const TranslationEntryQt& entry) const;
    void setupCoreBridge();
    void updateEntry(int allIndex, const TranslationEntryQt& entry);
    int filteredToAllIndex(int filteredIndex) const;
    int allToFilteredIndex(int allIndex) const;

    CoreBridge* m_coreBridge{nullptr};
    QList<TranslationEntryQt> m_allEntries;
    QList<int> m_filteredIndices;  // indices into m_allEntries

    QString m_filterText;
    EntryStatusFilter m_filterStatus{EntryStatusFilter::All};
    EntryCategoryFilter m_filterCategory{EntryCategoryFilter::All};
    bool m_isLoading{false};
    QString m_activeGameId;
    QString m_activeGamePath;
    QString m_activeEngine;
    double m_averageQAScore{100.0};
};

/**
 * @brief Filtered Translation Model
 *
 * QSortFilterProxyModel for advanced filtering.
 */
class FilteredTranslationModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool showOnlyUntranslated READ showOnlyUntranslated WRITE setShowOnlyUntranslated NOTIFY showOnlyUntranslatedChanged)
    Q_PROPERTY(bool showOnlyWithIssues READ showOnlyWithIssues WRITE setShowOnlyWithIssues NOTIFY showOnlyWithIssuesChanged)

public:
    explicit FilteredTranslationModel(QObject *parent = nullptr);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& text);

    bool showOnlyUntranslated() const { return m_showOnlyUntranslated; }
    void setShowOnlyUntranslated(bool show);

    bool showOnlyWithIssues() const { return m_showOnlyWithIssues; }
    void setShowOnlyWithIssues(bool show);

signals:
    void filterTextChanged();
    void showOnlyUntranslatedChanged();
    void showOnlyWithIssuesChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    bool m_showOnlyUntranslated{false};
    bool m_showOnlyWithIssues{false};
};

} // namespace makineai
