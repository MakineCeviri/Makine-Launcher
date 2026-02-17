/**
 * @file integrityservice.cpp
 * @brief Binary self-integrity verification — thin Qt wrapper
 * @copyright (c) 2026 MakineAI Team
 *
 * Delegates hash computation to makineai::integrity core module
 * when available, falls back to QCryptographicHash for UI-only builds.
 */

#include "integrityservice.h"
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFileInfo>
#include <QDebug>

#ifndef MAKINEAI_UI_ONLY
#include <makineai/file_integrity.hpp>
#else
#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#endif

namespace makineai {

IntegrityService::IntegrityService(QObject *parent)
    : QObject(parent)
{
    // Run verification asynchronously after event loop starts
    QMetaObject::invokeMethod(this, &IntegrityService::verify, Qt::QueuedConnection);
}

IntegrityService::~IntegrityService() = default;

IntegrityService* IntegrityService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new IntegrityService();
}

void IntegrityService::verify()
{
    if (m_checking) return;

#ifndef MAKINEAI_RELEASE_VERIFIED
    // Skip integrity check unless explicitly enabled for release distribution
    m_verified = true;
    m_status = "skipped";
    qDebug() << "Integrity check: skipped (not a verified release build)";
    emit verificationComplete();
    return;
#endif

    m_checking = true;
    emit checkingChanged();

    (void)QtConcurrent::run([this]() {
        performCheck();
    });
}

void IntegrityService::performCheck()
{
    const QString exePath = QCoreApplication::applicationFilePath();

#ifndef MAKINEAI_UI_ONLY
    // Core module: makineai::integrity handles everything
    auto result = integrity::verifyFile(exePath.toStdString());

    if (!result) {
        // No hash file or I/O error
        const auto& err = result.error();
        if (err.code() == ErrorCode::FileNotFound) {
            // No .sha256 file = dev build
            QMetaObject::invokeMethod(this, [this]() {
                m_verified = true;
                m_checking = false;
                m_status = "skipped";
                qDebug() << "Integrity check: no .sha256 file found (dev build), skipping";
                emit checkingChanged();
                emit verificationComplete();
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                m_verified = false;
                m_checking = false;
                m_status = "error";
                qWarning() << "Integrity check: verification error";
                emit checkingChanged();
                emit verificationComplete();
            }, Qt::QueuedConnection);
        }
        return;
    }

    const bool match = *result;
    QMetaObject::invokeMethod(this, [this, match]() {
        m_verified = match;
        m_checking = false;
        m_status = match ? "verified" : "failed";
        qDebug() << "Integrity check:" << (match ? "PASSED" : "FAILED");
        emit checkingChanged();
        emit verificationComplete();
    }, Qt::QueuedConnection);

#else
    // UI-only fallback: QCryptographicHash
    const QString hashFilePath = exePath + ".sha256";

    if (!QFileInfo::exists(hashFilePath)) {
        QMetaObject::invokeMethod(this, [this]() {
            m_verified = true;
            m_checking = false;
            m_status = "skipped";
            qDebug() << "Integrity check: no .sha256 file found (dev build), skipping";
            emit checkingChanged();
            emit verificationComplete();
        }, Qt::QueuedConnection);
        return;
    }

    // Read expected hash
    QFile hashFile(hashFilePath);
    QString expectedHash;
    if (hashFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString line = QString::fromUtf8(hashFile.readLine()).trimmed();
        expectedHash = line.split(QRegularExpression("\\s+")).first().toLower();
        if (expectedHash.length() != 64) expectedHash.clear();
    }

    if (expectedHash.isEmpty()) {
        QMetaObject::invokeMethod(this, [this]() {
            m_verified = false;
            m_checking = false;
            m_status = "error";
            qWarning() << "Integrity check: failed to read expected hash";
            emit checkingChanged();
            emit verificationComplete();
        }, Qt::QueuedConnection);
        return;
    }

    // Compute actual hash
    QFile exeFile(exePath);
    QString actualHash;
    if (exeFile.open(QIODevice::ReadOnly)) {
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        constexpr qint64 chunkSize = 65536;
        while (!exeFile.atEnd()) {
            const QByteArray chunk = exeFile.read(chunkSize);
            if (chunk.isEmpty()) break;
            hasher.addData(chunk);
        }
        if (exeFile.error() == QFileDevice::NoError) {
            actualHash = hasher.result().toHex().toLower();
        }
    }

    if (actualHash.isEmpty()) {
        QMetaObject::invokeMethod(this, [this]() {
            m_verified = false;
            m_checking = false;
            m_status = "error";
            qWarning() << "Integrity check: failed to compute binary hash";
            emit checkingChanged();
            emit verificationComplete();
        }, Qt::QueuedConnection);
        return;
    }

    const bool match = (actualHash == expectedHash);
    QMetaObject::invokeMethod(this, [this, match]() {
        m_verified = match;
        m_checking = false;
        m_status = match ? "verified" : "failed";
        qDebug() << "Integrity check:" << (match ? "PASSED" : "FAILED");
        emit checkingChanged();
        emit verificationComplete();
    }, Qt::QueuedConnection);
#endif
}

} // namespace makineai
