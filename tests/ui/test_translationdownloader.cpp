// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// TranslationDownloader against a local HTTP server (QTcpServer on 127.0.0.1):
// the download queue, a cancel that lands between two attempts, and what a
// failed extraction leaves behind. No CDN, no network.

#include <gtest/gtest.h>

#include "apppaths.h"
#include "packagestamp.h"
#include "translationdownloader.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#include <cstring>
#include <memory>

#include <zstd.h>

using namespace makine;

namespace {

// One canned reply per path; counts the requests it answers.
class TinyServer : public QObject
{
public:
    struct Reply { int status; QByteArray body; };

    TinyServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                auto buf = std::make_shared<QByteArray>();
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
                connect(s, &QTcpSocket::readyRead, s, [this, s, buf] {
                    *buf += s->readAll();
                    if (!buf->contains("\r\n\r\n")) return;
                    const QList<QByteArray> line = buf->left(buf->indexOf("\r\n")).split(' ');
                    const QString path = line.size() > 1 ? QString::fromLatin1(line[1]) : QString();
                    ++hits[path];
                    const Reply r = replies.value(path, {404, {}});
                    s->write("HTTP/1.1 " + QByteArray::number(r.status) + " X\r\n"
                             "Content-Length: " + QByteArray::number(r.body.size()) + "\r\n"
                             "Connection: close\r\n\r\n" + r.body);
                    s->disconnectFromHost();
                });
            }
        });
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    QString url(const QString& path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path);
    }

    QHash<QString, Reply> replies;
    QHash<QString, int> hits;

private:
    QTcpServer m_server;
};

template <typename Pred>
bool waitFor(Pred pred, int ms)
{
    QElapsedTimer t;
    t.start();
    while (!pred() && t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return pred();
}

QByteArray tarEntry(const QByteArray& name, qint64 size, const QByteArray& data)
{
    QByteArray header(512, '\0');
    std::memcpy(header.data(), name.constData(), size_t(name.size()));
    const QByteArray octal = QByteArray::number(size, 8).rightJustified(11, '0');
    std::memcpy(header.data() + 124, octal.constData(), 11);
    header[156] = '0';
    std::memcpy(header.data() + 257, "ustar", 5);
    return header + data + QByteArray((512 - data.size() % 512) % 512, '\0');
}

// An unencrypted (v2) package whose second entry claims more bytes than the
// archive holds: the first file is written, then extraction fails.
QByteArray truncatedPackage()
{
    const QByteArray tar = tarEntry("good.txt", 5, "hello") + tarEntry("cut.bin", 4096, {});
    QByteArray zst(int(ZSTD_compressBound(size_t(tar.size()))), '\0');
    const size_t n = ZSTD_compress(zst.data(), size_t(zst.size()), tar.constData(),
                                   size_t(tar.size()), 3);
    zst.resize(int(n));
    return QByteArray("MKPK\x02", 5) + zst;
}

} // namespace

TEST(TranslationDownloader, QueuedDownloadStartsWhenTheActiveOneEnds)
{
    // One download at a time; a second game's waits for the slot. The queue
    // only moved when a .part file could not be opened, so it waited forever.
    TinyServer server;   // both answer 404
    QTemporaryDir data;
    TranslationDownloader dl;
    dl.setDataPath(data.path());
    QStringList failed;
    QObject::connect(&dl, &TranslationDownloader::downloadError,
                     [&](const QString& id, const QString&) { failed << id; });

    dl.downloadPackage("q1", server.url("/q1.makine"), "Q1");
    dl.downloadPackage("q2", server.url("/q2.makine"), "Q2");

    EXPECT_TRUE(waitFor([&] { return failed.size() == 2; }, 15000))
        << failed.join(',').toStdString();
    EXPECT_EQ(failed, QStringList({"q1", "q2"}));
    EXPECT_EQ(server.hits.value("/q2.makine"), 1);
}

TEST(TranslationDownloader, CancelDuringRetryWaitStopsWithoutAnotherAttempt)
{
    // A 503 is retried after 2 s. Cancelled in that wait there is no reply to
    // abort, and the retry used to download the whole package before the
    // cancel took effect.
    TinyServer server;
    server.replies["/r.makine"] = {503, {}};
    QTemporaryDir data;
    TranslationDownloader dl;
    dl.setDataPath(data.path());
    bool retrying = false, cancelled = false, failed = false;
    QObject::connect(&dl, &TranslationDownloader::downloadRetrying,
                     [&](const QString&, int, int) { retrying = true; });
    QObject::connect(&dl, &TranslationDownloader::downloadCancelled,
                     [&](const QString&) { cancelled = true; });
    QObject::connect(&dl, &TranslationDownloader::downloadError,
                     [&](const QString&, const QString&) { failed = true; });

    dl.downloadPackage("r", server.url("/r.makine"), "R");
    ASSERT_TRUE(waitFor([&] { return retrying; }, 15000));
    dl.cancelDownload("r");

    ASSERT_TRUE(waitFor([&] { return cancelled || failed; }, 20000));
    EXPECT_TRUE(cancelled);
    EXPECT_FALSE(failed);
    EXPECT_EQ(server.hits.value("/r.makine"), 1);
    EXPECT_FALSE(dl.isDownloading("r"));
}

TEST(TranslationDownloader, FailedExtractionLeavesNothingAnInstallWouldReuse)
{
    // The same package was extracted into this folder before (its stamp
    // matches the catalogue). Extracted again, it failed half-way: the stamp
    // stayed, the half-written folder stayed, and the next install used it as
    // a complete package.
    const QByteArray body = truncatedPackage();
    const QString sha = QString::fromLatin1(
        QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex());
    TinyServer server;
    server.replies["/t.makine"] = {200, body};
    QTemporaryDir data;
    QDir().mkpath(AppPaths::dataDir());
    pkgstamp::set("Trunc", sha);

    TranslationDownloader dl;
    dl.setDataPath(data.path());
    bool failed = false, ready = false;
    QObject::connect(&dl, &TranslationDownloader::downloadError,
                     [&](const QString&, const QString&) { failed = true; });
    QObject::connect(&dl, &TranslationDownloader::packageReady,
                     [&](const QString&, const QString&) { ready = true; });

    dl.downloadPackage("t", server.url("/t.makine"), "Trunc", 0, "sha256:" + sha);
    ASSERT_TRUE(waitFor([&] { return failed || ready; }, 20000));
    EXPECT_TRUE(failed);
    EXPECT_TRUE(pkgstamp::get("Trunc").isEmpty());
    EXPECT_FALSE(QDir(data.path() + "/Trunc").exists());
    pkgstamp::clear("Trunc");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // What the downloader keeps under AppLocalDataLocation stays out of the real app's.
    QStandardPaths::setTestModeEnabled(true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
