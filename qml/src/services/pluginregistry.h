// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include "pluginmanifest.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

class QNetworkAccessManager;

/// One plugin offered by the signed registry (assets/plugins/index.json).
struct PluginRegistryEntry {
    QString id;
    QString name;
    QString version;
    QString description;
    QString publisher;
    QString homepage;
    QString license;
    QString minLauncher;
    QString url;       // https://cdn.makineceviri.org/data/plugins/<id>/<version>.mkplugin
    QString sha256;    // lowercase hex of the package
    qint64 size = 0;
    int abi = 0;
    QList<PluginCapability> capabilities;
};

namespace pluginregistry {

/// The Ed25519 key the registry is signed with (scripts/plugin_registry.py).
QByteArray publicKey();

/// Ed25519 check of `message` against a base64 signature.
bool verifySignature(const QByteArray& message, const QByteArray& signatureBase64,
                     const QByteArray& publicKey);

/// Entries of a verified index. Entries that break a rule are dropped with a
/// warning; a malformed document fails as a whole.
std::optional<QList<PluginRegistryEntry>> parseIndex(const QByteArray& json, QString* error = nullptr);

} // namespace pluginregistry

/**
 * Fetches the registry and its signature from the CDN. An index whose signature
 * does not verify is discarded: plugins are native code, so a CDN or storage
 * compromise must not be able to put a package in front of users.
 */
class PluginRegistry : public QObject {
    Q_OBJECT

public:
    explicit PluginRegistry(QNetworkAccessManager* net, QObject* parent = nullptr);

    void refresh();
    bool isLoading() const { return m_pending > 0; }
    const QList<PluginRegistryEntry>& entries() const { return m_entries; }
    const PluginRegistryEntry* find(const QString& id) const;
    QString lastError() const { return m_lastError; }

signals:
    void loadingChanged();
    void updated();
    void failed(const QString& reason);

private:
    void finish();

    QNetworkAccessManager* m_net;
    QList<PluginRegistryEntry> m_entries;
    QByteArray m_index;
    QByteArray m_signature;
    QString m_fetchError;
    QString m_lastError;
    int m_pending = 0;
};
