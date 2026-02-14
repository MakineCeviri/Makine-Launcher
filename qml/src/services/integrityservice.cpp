/**
 * @file integrityservice.cpp
 * @brief Binary self-integrity verification implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "integrityservice.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QtConcurrent>

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

    m_checking = true;
    emit checkingChanged();

    (void)QtConcurrent::run([this]() {
        performCheck();
    });
}

void IntegrityService::performCheck()
{
    const QString exePath = QCoreApplication::applicationFilePath();
    const QString hashFilePath = exePath + ".sha256";

    // Check if hash file exists
    if (!QFileInfo::exists(hashFilePath)) {
        QMetaObject::invokeMethod(this, [this]() {
            m_verified = true; // No hash file = dev build, skip verification
            m_checking = false;
            m_status = "skipped";
            qDebug() << "Integrity check: no .sha256 file found (dev build), skipping";
            emit checkingChanged();
            emit verificationComplete();
        }, Qt::QueuedConnection);
        return;
    }

    // Read expected hash
    const QString expectedHash = readExpectedHash(hashFilePath);
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
    const QString actualHash = computeFileHash(exePath);
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

    // Compare
    const bool match = (actualHash == expectedHash);

    QMetaObject::invokeMethod(this, [this, match, actualHash, expectedHash]() {
        m_verified = match;
        m_checking = false;
        m_status = match ? "verified" : "failed";

        if (match) {
            qDebug() << "Integrity check: PASSED";
        } else {
            qWarning() << "Integrity check: FAILED";
            qWarning() << "  Expected:" << expectedHash;
            qWarning() << "  Actual:  " << actualHash;
        }

        emit checkingChanged();
        emit verificationComplete();
    }, Qt::QueuedConnection);
}

QString IntegrityService::computeFileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);

    // Read in 64KB chunks to avoid loading entire binary into memory
    constexpr qint64 chunkSize = 65536;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty()) break;
        hasher.addData(chunk);
    }

    // Check for I/O errors during read (would produce incorrect hash)
    if (file.error() != QFileDevice::NoError) {
        qWarning() << "IntegrityService: I/O error reading" << filePath << file.errorString();
        return {};
    }

    return hasher.result().toHex().toLower();
}

QString IntegrityService::readExpectedHash(const QString& hashFilePath)
{
    QFile file(hashFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    // Format: "<hash>  <filename>\n" or just "<hash>\n"
    const QString line = QString::fromUtf8(file.readLine()).trimmed();
    if (line.isEmpty()) return {};

    // Extract hash (first 64 hex chars = SHA-256)
    const QString hash = line.split(QRegularExpression("\\s+")).first().toLower();
    if (hash.length() != 64) return {};

    // Validate hex characters
    static const QRegularExpression hexPattern("^[0-9a-f]{64}$");
    if (!hexPattern.match(hash).hasMatch()) return {};

    return hash;
}

} // namespace makineai
