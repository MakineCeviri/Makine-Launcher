// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// The TLS error policy every launcher QNetworkAccessManager installs
// (networksecurity.h), exercised against a real man in the middle: a local
// HTTP proxy that answers CONNECT for cdn.makineceviri.org and completes the
// TLS handshake itself. Nothing leaves the machine.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <memory>

#include "networksecurity.h"

namespace {

// Google Trust Services WE1, the public intermediate behind Cloudflare's
// certificate for our domains (valid until 2029-02-20). Its key hash was the
// launcher's "backup pin", so a chain carrying it counted as ours.
constexpr char kWe1Der[] =
    "MIICnzCCAiWgAwIBAgIQf/MZd5csIkp2FV0TttaF4zAKBggqhkjOPQQDAzBHMQsw"
    "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU"
    "MBIGA1UEAxMLR1RTIFJvb3QgUjQwHhcNMjMxMjEzMDkwMDAwWhcNMjkwMjIwMTQw"
    "MDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZp"
    "Y2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARvzTr+"
    "Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4mMTG6J7/EFmP"
    "LCaY9eYmJbsPAvpWo4H+MIH7MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggr"
    "BgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQU"
    "kHeSNWfE/6jMqeZ72YB5e8yT+TgwHwYDVR0jBBgwFoAUgEzW63T/STaj1dj8tT7F"
    "avCUHYwwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzAChhhodHRwOi8vaS5wa2ku"
    "Z29vZy9yNC5jcnQwKwYDVR0fBCQwIjAgoB6gHIYaaHR0cDovL2MucGtpLmdvb2cv"
    "ci9yNC5jcmwwEwYDVR0gBAwwCjAIBgZngQwBAgEwCgYIKoZIzj0EAwMDaAAwZQIx"
    "AOcCq1HW90OVznX+0RGU1cxAQXomvtgM8zItPZCuFQ8jSBJSjz5keROv9aYsAm5V"
    "sQIwJonMaAFi54mrfhfoFNZEfuNMSQ6/bIBiNLiyoX46FohQvKeIoJ99cx7sUkFN"
    "7uJW";

QByteArray bioBytes(BIO* bio)
{
    char* data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    QByteArray out(data, static_cast<qsizetype>(len));
    BIO_free(bio);
    return out;
}

// A throwaway key and a certificate for `host` signed with it, made per run so
// no key material is kept in the repository.
bool makeSelfSigned(const char* host, QSslCertificate& cert, QSslKey& key)
{
    EVP_PKEY* pkey = EVP_EC_gen("P-256");
    X509* x = X509_new();
    if (!pkey || !x) return false;
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), -3600);
    X509_gmtime_adj(X509_getm_notAfter(x), 3600L * 24);
    X509_set_pubkey(x, pkey);
    X509_NAME* name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>(host), -1, -1, 0);
    X509_set_issuer_name(x, name);
    const bool signedOk = X509_sign(x, pkey, EVP_sha256()) > 0;

    BIO* certBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, x);
    BIO* keyBio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    X509_free(x);
    EVP_PKEY_free(pkey);

    cert = QSslCertificate(bioBytes(certBio), QSsl::Pem);
    key = QSslKey(bioBytes(keyBio), QSsl::Ec, QSsl::Pem);
    return signedOk && !cert.isNull() && !key.isNull();
}

// Answers CONNECT, then speaks TLS with the chain it was given and serves a
// catalogue of its own.
class InterceptingProxy : public QTcpServer {
public:
    QList<QSslCertificate> chain;
    QSslKey key;

protected:
    void incomingConnection(qintptr fd) override
    {
        auto* s = new QSslSocket(this);
        s->setSocketDescriptor(fd);
        s->setLocalCertificateChain(chain);
        s->setPrivateKey(key);
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(s, &QSslSocket::readyRead, s, [s, buffer]() {
            buffer->append(s->readAll());
            if (!buffer->contains("\r\n\r\n")) return;
            buffer->clear();
            if (!s->isEncrypted()) {
                s->write("HTTP/1.1 200 Connection established\r\n\r\n");
                s->startServerEncryption();
                return;
            }
            s->write("HTTP/1.1 200 OK\r\nContent-Length: 20\r\nConnection: close\r\n\r\n"
                     "{\"packages\":\"fake\"}\n");
            s->disconnectFromHost();
        });
    }
};

} // namespace

TEST(TlsPolicy, ForgedChainCarryingThePublicIntermediateIsRefused)
{
    // The OpenSSL backend: it serves the forged handshake here, and as a client
    // it reports the chain as the server sent it — the case where appending
    // the public WE1 certificate was enough. (Release ZIPs ship Schannel only;
    // there the old check needed a certificate WE1 really signed.)
    if (!QSslSocket::availableBackends().contains(QStringLiteral("openssl")))
        GTEST_SKIP() << "OpenSSL TLS backend not available";
    ASSERT_TRUE(QSslSocket::setActiveBackend(QStringLiteral("openssl")));

    InterceptingProxy proxy;
    QSslCertificate leaf;
    ASSERT_TRUE(makeSelfSigned("cdn.makineceviri.org", leaf, proxy.key));
    const QSslCertificate we1(QByteArray::fromBase64(kWe1Der), QSsl::Der);
    ASSERT_FALSE(we1.isNull());
    proxy.chain = {leaf, we1};
    ASSERT_TRUE(proxy.listen(QHostAddress::LocalHost));

    QNetworkAccessManager nam;
    nam.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"),
                               proxy.serverPort()));
    makine::security::installTlsPinning(&nam);

    QNetworkRequest req(QUrl(QStringLiteral("https://cdn.makineceviri.org/assets/index.json")));
    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(reply->isFinished()) << "no answer within 10 s";
    EXPECT_EQ(reply->error(), QNetworkReply::SslHandshakeFailedError) << qPrintable(reply->errorString());
    EXPECT_FALSE(reply->readAll().contains("fake"));
    reply->deleteLater();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
