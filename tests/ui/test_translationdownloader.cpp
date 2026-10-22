// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// TranslationDownloader against a local HTTP server (QTcpServer on 127.0.0.1):
// the download queue. No CDN, no network.

#include <gtest/gtest.h>

#include "translationdownloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#include <memory>

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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // What the downloader keeps under AppLocalDataLocation stays out of the real app's.
    QStandardPaths::setTestModeEnabled(true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
