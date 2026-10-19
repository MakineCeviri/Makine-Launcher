// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Registry trust chain: Ed25519 signature over the exact index bytes, then
// per-entry rules for what a signed index may point at.

#include "pluginregistry.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>

namespace {

struct KeyPair {
    QByteArray publicKey;
    EVP_PKEY* key = nullptr;
    KeyPair()
    {
        key = EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519");
        size_t len = 32;
        publicKey.resize(32);
        EVP_PKEY_get_raw_public_key(key, reinterpret_cast<unsigned char*>(publicKey.data()), &len);
    }
    ~KeyPair() { EVP_PKEY_free(key); }
    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;

    QByteArray sign(const QByteArray& message) const
    {
        QByteArray sig(64, '\0');
        size_t len = 64;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key);
        EVP_DigestSign(ctx, reinterpret_cast<unsigned char*>(sig.data()), &len,
                       reinterpret_cast<const unsigned char*>(message.constData()), message.size());
        EVP_MD_CTX_free(ctx);
        return sig.toBase64() + "\n";
    }
};

QJsonObject entry()
{
    return {
        {"id", "org.makineceviri.live"},
        {"name", "Makine Live"},
        {"version", "1.0.0"},
        {"publisher", "Makine Çeviri"},
        {"abi", 2},
        {"minLauncher", "0.2.0"},
        {"url", "https://cdn.makineceviri.org/data/plugins/org.makineceviri.live/1.0.0.mkplugin"},
        {"sha256", QString(64, u'a')},
        {"size", 1024},
        {"capabilities", QJsonArray{QJsonObject{{"id", "network"}, {"reason", "Çeviri"}}}},
    };
}

QByteArray index(const QJsonArray& plugins)
{
    return QJsonDocument(QJsonObject{{"schema", 1}, {"plugins", plugins}}).toJson();
}

// How many entries survive when `field` of the sample entry is replaced by `value`.
qsizetype surviving(const QString& field, const QJsonValue& value)
{
    QJsonObject e = entry();
    e.insert(field, value);
    const auto parsed = pluginregistry::parseIndex(index({e}));
    return parsed ? parsed->size() : -1;
}

} // namespace

TEST(PluginRegistry, EmbeddedKeyIsAnEd25519PublicKey)
{
    EXPECT_EQ(pluginregistry::publicKey().size(), 32);
}

TEST(PluginRegistry, SignatureCoversTheExactBytes)
{
    const KeyPair keys;
    const QByteArray data = index({entry()});
    const QByteArray sig = keys.sign(data);
    EXPECT_TRUE(pluginregistry::verifySignature(data, sig, keys.publicKey));

    QByteArray tampered = data;
    tampered[tampered.size() / 2] = tampered[tampered.size() / 2] ^ 1;
    EXPECT_FALSE(pluginregistry::verifySignature(tampered, sig, keys.publicKey));
    EXPECT_FALSE(pluginregistry::verifySignature(data + " ", sig, keys.publicKey));

    const KeyPair other;
    EXPECT_FALSE(pluginregistry::verifySignature(data, sig, other.publicKey));
    EXPECT_FALSE(pluginregistry::verifySignature(data, "not base64 !!", keys.publicKey));
    EXPECT_FALSE(pluginregistry::verifySignature(data, QByteArray(), keys.publicKey));
    // The production key did not sign this test index.
    EXPECT_FALSE(pluginregistry::verifySignature(data, sig, pluginregistry::publicKey()));
}

TEST(PluginRegistry, ProductionKeyVerifiesWhatThePythonSignerWrote)
{
    // scripts/plugin_registry.py sign over this exact string with the real key.
    // Not an index, so the signature is useless to anyone replaying it.
    const QByteArray message = "makine-plugin-registry-selftest";
    const QByteArray sig = "FRk4xRRyMP2pBhBFKTIzHUBlqM3PoUkf3TS+21nTWzGYrWgy6vYrwSXY+9hUyHVU3OlGo3+1qPIxmusRNJ59Aw==\n";
    EXPECT_TRUE(pluginregistry::verifySignature(message, sig, pluginregistry::publicKey()));
    EXPECT_FALSE(pluginregistry::parseIndex(message));
}

TEST(PluginRegistry, ParsesAValidEntry)
{
    const auto parsed = pluginregistry::parseIndex(index({entry()}));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(parsed->size(), 1);
    EXPECT_EQ(parsed->at(0).id, QStringLiteral("org.makineceviri.live"));
    EXPECT_EQ(parsed->at(0).capabilities.size(), 1);
}

TEST(PluginRegistry, EntriesMustPointAtTheirOwnCdnPath)
{
    EXPECT_EQ(surviving("url", "https://evil.example/data/plugins/org.makineceviri.live/1.0.0.mkplugin"), 0);
    EXPECT_EQ(surviving("url", "http://cdn.makineceviri.org/data/plugins/org.makineceviri.live/1.0.0.mkplugin"), 0);
    EXPECT_EQ(surviving("url", "https://cdn.makineceviri.org/data/plugins/org.other/1.0.0.mkplugin"), 0);
    EXPECT_EQ(surviving("url", "https://cdn.makineceviri.org/data/plugins/org.makineceviri.live/2.0.0.mkplugin"), 0);
    EXPECT_EQ(surviving("sha256", "ABC"), 0);
    EXPECT_EQ(surviving("sha256", QString(64, u'A')), 0);
    EXPECT_EQ(surviving("size", 0), 0);
    EXPECT_EQ(surviving("size", 500.0 * 1024 * 1024), 0);
    EXPECT_EQ(surviving("id", "Live"), 0);
    EXPECT_EQ(surviving("capabilities", QJsonArray{QJsonObject{{"id", "root"}, {"reason", "x"}}}), 0);
}

TEST(PluginRegistry, MalformedIndexFailsAsAWhole)
{
    EXPECT_FALSE(pluginregistry::parseIndex("not json"));
    EXPECT_FALSE(pluginregistry::parseIndex(QJsonDocument(QJsonObject{{"schema", 9}}).toJson()));
}
