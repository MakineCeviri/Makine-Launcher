// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginregistry.h"
#include "cdnconfig.h"
#include "pluginprocess.h"   // lcPlugin

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

#include <optional>

#ifndef MAKINE_UI_ONLY
#include <openssl/evp.h>
#endif

namespace {

// Public half of the registry signing key. The private half lives outside the
// repository (see scripts/plugin_registry.py); rotating it needs a launcher release.
constexpr char kRegistryPublicKey[] = "DddwWINnLSFGKl2fZnji2AzY+mj5evH4kNW224nmRvI=";

constexpr qint64 kMaxPackageSize = 200LL * 1024 * 1024;

bool isSha256Hex(const QString& s)
{
    if (s.size() != 64) return false;
    for (const QChar c : s)
        if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f'))) return false;
    return true;
}

std::optional<PluginRegistryEntry> parseEntry(const QJsonObject& o, QString* why)
{
    PluginRegistryEntry e;
    e.id = o.value(u"id").toString();
    e.name = o.value(u"name").toString().trimmed();
    e.version = o.value(u"version").toString();
    e.description = o.value(u"description").toString();
    e.publisher = o.value(u"publisher").toString();
    e.homepage = o.value(u"homepage").toString();
    e.license = o.value(u"license").toString();
    e.minLauncher = o.value(u"minLauncher").toString(QStringLiteral("0.0.0"));
    e.url = o.value(u"url").toString();
    e.sha256 = o.value(u"sha256").toString();
    e.size = o.value(u"size").toInteger();
    e.abi = o.value(u"abi").toInt();

    const QString expectedPath = QStringLiteral("/data/plugins/%1/%2.mkplugin").arg(e.id, e.version);
    if (!pluginmanifest::isValidId(e.id)) *why = QStringLiteral("bad id");
    else if (e.name.isEmpty()) *why = QStringLiteral("no name");
    else if (!pluginmanifest::isValidVersion(e.version) || !pluginmanifest::isValidVersion(e.minLauncher))
        *why = QStringLiteral("bad version");
    else if (!makine::cdn::isCdnUrl(e.url) || QUrl(e.url).path() != expectedPath)
        *why = QStringLiteral("package URL is not %1 on the CDN").arg(expectedPath);
    else if (!isSha256Hex(e.sha256)) *why = QStringLiteral("bad sha256");
    else if (e.size <= 0 || e.size > kMaxPackageSize) *why = QStringLiteral("bad size");
    if (!why->isEmpty()) return std::nullopt;

    for (const auto& c : o.value(u"capabilities").toArray()) {
        const PluginCapability cap{c.toObject().value(u"id").toString(),
                                   c.toObject().value(u"reason").toString()};
        if (!pluginmanifest::knownCapabilities().contains(cap.id) || cap.reason.trimmed().isEmpty()) {
            *why = QStringLiteral("capability %1").arg(cap.id);
            return std::nullopt;
        }
        e.capabilities.append(cap);
    }
    return e;
}

} // namespace

namespace pluginregistry {

QByteArray publicKey()
{
    return QByteArray::fromBase64(kRegistryPublicKey);
}

bool verifySignature(const QByteArray& message, const QByteArray& signatureBase64,
                     const QByteArray& key)
{
#ifndef MAKINE_UI_ONLY
    const auto decoded = QByteArray::fromBase64Encoding(signatureBase64.trimmed(),
                                                        QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() != 64 || key.size() != 32) return false;

    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char*>(key.constData()), 32);
    if (!pkey) return false;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const bool ok = ctx
        && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1
        && EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char*>(decoded.decoded.constData()), 64,
                            reinterpret_cast<const unsigned char*>(message.constData()),
                            static_cast<size_t>(message.size())) == 1;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
#else
    Q_UNUSED(message) Q_UNUSED(signatureBase64) Q_UNUSED(key)
    return false;   // UI-only builds have no OpenSSL, so no registry
#endif
}

std::optional<QList<PluginRegistryEntry>> parseIndex(const QByteArray& json, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("index is not a JSON object");
        return std::nullopt;
    }
    if (doc.object().value(u"schema").toInt() != 1) {
        if (error) *error = QStringLiteral("unsupported index schema");
        return std::nullopt;
    }

    QList<PluginRegistryEntry> entries;
    for (const auto& item : doc.object().value(u"plugins").toArray()) {
        QString why;
        if (auto entry = parseEntry(item.toObject(), &why))
            entries.append(*entry);
        else
            qCWarning(lcPlugin) << "Registry entry skipped:" << item.toObject().value(u"id").toString() << why;
    }
    return entries;
}

} // namespace pluginregistry

PluginRegistry::PluginRegistry(QNetworkAccessManager* net, QObject* parent)
    : QObject(parent)
    , m_net(net)
{
}

const PluginRegistryEntry* PluginRegistry::find(const QString& id) const
{
    for (const auto& e : m_entries)
        if (e.id == id) return &e;
    return nullptr;
}

void PluginRegistry::refresh()
{
    if (m_pending > 0) return;
    m_index.clear();
    m_signature.clear();
    m_fetchError.clear();
    m_pending = 2;
    emit loadingChanged();

    // One query value for both files, fresh each minute: the edge cache must not
    // pair a new index with an old signature for longer than that.
    const QString bust = QString::number(QDateTime::currentSecsSinceEpoch() / 60);
    const auto fetch = [this, &bust](const char* url, QByteArray* into) {
        QUrl u(QString::fromLatin1(url));
        u.setQuery(QStringLiteral("t=") + bust);
        QNetworkRequest req(u);
        req.setRawHeader("User-Agent", makine::cdn::kUserAgent);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto* reply = m_net->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, into] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) *into = reply->readAll();
            else m_fetchError = reply->errorString();
            if (--m_pending == 0) finish();
        });
    };
    fetch(makine::cdn::kPluginIndex, &m_index);
    fetch(makine::cdn::kPluginIndexSig, &m_signature);
}

void PluginRegistry::finish()
{
    emit loadingChanged();
    QString error;
    if (!m_fetchError.isEmpty()) {
        error = m_fetchError;
    } else if (!pluginregistry::verifySignature(m_index, m_signature, pluginregistry::publicKey())) {
        error = QStringLiteral("registry signature does not verify");
        qCWarning(lcPlugin) << "Plugin registry rejected: bad signature";
    } else if (auto entries = pluginregistry::parseIndex(m_index, &error)) {
        m_entries = *entries;
        m_lastError.clear();
        qCInfo(lcPlugin) << "Plugin registry:" << m_entries.size() << "plugin(s)";
        emit updated();
        return;
    }
    m_lastError = error;
    emit failed(error);
}
