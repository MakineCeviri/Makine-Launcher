/**
 * @file integrityservice.h
 * @brief Binary self-integrity verification service
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Thin Qt wrapper around makine::integrity core module.
 * Verifies that the application binary has not been tampered with by
 * comparing its SHA-256 hash against a known-good hash generated at
 * build time.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>

namespace makine {

class IntegrityService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool verified READ verified NOTIFY verificationComplete)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString status READ status NOTIFY verificationComplete)

public:
    explicit IntegrityService(QObject *parent = nullptr);
    ~IntegrityService() override;

    static IntegrityService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool verified() const { return m_verified; }
    bool checking() const { return m_checking; }
    QString status() const { return m_status; }

    Q_INVOKABLE void verify();

signals:
    void verificationComplete();
    void checkingChanged();

private:
    void performCheck();

    bool m_verified{true};
    bool m_checking{false};
    QString m_status{"pending"};
};

} // namespace makine
