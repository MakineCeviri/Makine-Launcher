/**
 * @file translationstatemanager.h
 * @brief Translation button 7-state resolver (replaces QML JS cascade)
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QString>

namespace makine {

class TranslationStateManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString label READ label NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleText READ accessibleText NOTIFY stateChanged)

public:
    enum class State { Download, Installing, Completed, Installed, Update, Broken, External };
    Q_ENUM(State)

    explicit TranslationStateManager(QObject* parent = nullptr);

    /// Re-evaluate state from view-model inputs. Emits stateChanged only on actual change.
    Q_INVOKABLE void evaluate(bool hasUpdate, bool packageInstalled,
                               bool isInstalling, bool installCompleted,
                               const QString& impactLevel,
                               const QString& externalUrl);

    QString state() const;
    QString label() const;
    QString accessibleText() const;

signals:
    void stateChanged();

private:
    State m_state{State::Download};
    QString m_stateStr{QStringLiteral("download")};
    QString m_label;
    QString m_accessibleText;

    static QString stateToString(State s);
    static QString stateToLabel(State s);
    static QString stateToAccessibleText(State s);
};

} // namespace makine
