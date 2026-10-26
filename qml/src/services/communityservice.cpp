// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file communityservice.cpp
 * @brief The website's community wall — see communityservice.h
 */

#include "communityservice.h"
#include "networksecurity.h"
#include "settingsmanager.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <functional>

Q_LOGGING_CATEGORY(lcCommunity, "makine.community")

namespace makine {

namespace {

constexpr auto kSite = "https://makineceviri.org";
constexpr auto kTokenKey = "community/token";

QString typeLabel(const QString& type)
{
    if (type == QLatin1String("soru")) return CommunityService::tr("Soru");
    if (type == QLatin1String("oneri")) return CommunityService::tr("Öneri");
    if (type == QLatin1String("istek")) return CommunityService::tr("Yama isteği");
    if (type == QLatin1String("arkadas")) return CommunityService::tr("Oyun arkadaşı");
    if (type == QLatin1String("anket")) return CommunityService::tr("Anket");
    if (type == QLatin1String("ekip")) return CommunityService::tr("Duyuru");
    return CommunityService::tr("Sohbet");
}

// "Ahmet Yılmaz" → "AY", "sosyalkutu55" → "S"
QString initials(const QString& name)
{
    const QStringList words = name.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString out;
    for (const QString& w : words) {
        out += w.at(0).toUpper();
        if (out.size() == 2) break;
    }
    return out.isEmpty() ? QStringLiteral("?") : out;
}

// The site serves avatars from its own origin (a proxy to Discord's CDN)
QString absoluteUrl(const QString& path)
{
    if (path.isEmpty() || path.startsWith(QLatin1String("https://"))) return path;
    if (!path.startsWith(QLatin1Char('/'))) return {};
    return QLatin1String(kSite) + path;
}

QVariantMap shapeAuthor(const QJsonObject& author, const QString& fallbackName)
{
    QVariantMap a = author.toVariantMap();
    const QString name = author.value(QLatin1String("name")).toString(fallbackName);
    a[QStringLiteral("name")] = name;
    a[QStringLiteral("initials")] = initials(name);
    a[QStringLiteral("avatarUrl")] = absoluteUrl(author.value(QLatin1String("avatar")).toString());
    return a;
}

// A poll's options with their share, ready for bars; `mine` the member's pick
void shapePoll(QVariantMap& p, const QJsonObject& poll)
{
    const QJsonArray options = poll.value(QLatin1String("options")).toArray();
    const QJsonArray counts = poll.value(QLatin1String("counts")).toArray();
    const int total = poll.value(QLatin1String("total")).toInt();
    const int mine = poll.value(QLatin1String("mine")).toInt(-1);
    QVariantList rows;
    for (qsizetype i = 0; i < options.size(); ++i) {
        const int n = i < counts.size() ? counts.at(i).toInt() : 0;
        rows.append(QVariantMap{
            {QStringLiteral("text"), options.at(i).toString()},
            {QStringLiteral("count"), n},
            {QStringLiteral("share"), total > 0 ? double(n) / total : 0.0},
            {QStringLiteral("mine"), i == mine},
        });
    }
    p[QStringLiteral("pollRows")] = rows;
    p[QStringLiteral("pollTotal")] = total;
    p[QStringLiteral("pollClosed")] = poll.value(QLatin1String("closed")).toBool();
}

QVariantMap shapePost(const QJsonObject& post, const QDateTime& now)
{
    QVariantMap p = post.toVariantMap();
    const QString name = post.value(QLatin1String("name")).toString();
    p[QStringLiteral("author")] = shapeAuthor(post.value(QLatin1String("author")).toObject(), name);
    p[QStringLiteral("typeLabel")] = typeLabel(post.value(QLatin1String("type")).toString());
    p[QStringLiteral("when")] = CommunityService::relativeTime(post.value(QLatin1String("createdAt")).toString(), now);

    QVariantList replies;
    for (const QJsonValue& v : post.value(QLatin1String("replies")).toArray()) {
        const QJsonObject r = v.toObject();
        QVariantMap shaped = r.toVariantMap();
        shaped[QStringLiteral("author")] = shapeAuthor(r.value(QLatin1String("author")).toObject(),
                                                       r.value(QLatin1String("name")).toString());
        shaped[QStringLiteral("when")] = CommunityService::relativeTime(r.value(QLatin1String("createdAt")).toString(), now);
        replies.append(shaped);
    }
    p[QStringLiteral("replies")] = replies;
    p[QStringLiteral("replyCount")] = replies.size();

    const QJsonObject poll = post.value(QLatin1String("poll")).toObject();
    if (!poll.isEmpty())
        shapePoll(p, poll);
    return p;
}

// What a failed write says, in the site's own words when it gave any
QString failureText(int status, const QJsonObject& body)
{
    const QString said = body.value(QLatin1String("error")).toString();
    if (!said.isEmpty()) return said;
    if (status == 0) return CommunityService::tr("Siteye ulaşılamadı. İnternet bağlantını kontrol edip yeniden dene.");
    if (status == 429) return CommunityService::tr("Biraz yavaş: birkaç saniye bekleyip yeniden dene.");
    return CommunityService::tr("Olmadı (%1). Biraz sonra yeniden dene.").arg(status);
}

} // namespace

CommunityService::CommunityService(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);
    connect(&m_pollTimer, &QTimer::timeout, this, &CommunityService::pollConnect);

    // A member connected before: the token, DPAPI-encrypted
    const QByteArray stored = QSettings().value(QLatin1String(kTokenKey)).toByteArray();
    if (!stored.isEmpty()) {
        m_token = QString::fromUtf8(SettingsManager::unprotectData(QByteArray::fromBase64(stored)));
        if (!m_token.isEmpty())
            loadMember();
    }
}

QString CommunityService::relativeTime(const QString& timestamp, const QDateTime& now)
{
    // Posts carry ISO with a Z; the side rails "YYYY-MM-DD HH:MM:SS", also UTC
    QDateTime t = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!t.isValid())
        t = QDateTime::fromString(timestamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!t.isValid())
        return {};
    t.setTimeZone(QTimeZone::UTC);

    const qint64 secs = t.secsTo(now);
    if (secs < 60) return tr("az önce");
    if (secs < 3600) return tr("%1 dk önce").arg(secs / 60);
    if (secs < 86400) return tr("%1 sa önce").arg(secs / 3600);
    const qint64 days = t.toLocalTime().date().daysTo(now.toLocalTime().date());
    if (days <= 1) return tr("dün");
    if (days < 7) return tr("%1 gün önce").arg(days);
    return QLocale(QLocale::Turkish).toString(t.toLocalTime().date(), QStringLiteral("d MMM"));
}

QVariantMap CommunityService::parseWall(const QByteArray& json, const QDateTime& now)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (!root.value(QLatin1String("ok")).toBool())
        return {};

    QVariantList posts;
    for (const QJsonValue& v : root.value(QLatin1String("posts")).toArray())
        posts.append(shapePost(v.toObject(), now));

    const QJsonObject metaObj = root.value(QLatin1String("meta")).toObject();
    QVariantMap meta = metaObj.toVariantMap();

    QVariantList open;
    for (const QJsonValue& v : metaObj.value(QLatin1String("open")).toArray()) {
        QVariantMap q = v.toObject().toVariantMap();
        q[QStringLiteral("when")] = relativeTime(q.value(QStringLiteral("createdAt")).toString(), now);
        open.append(q);
    }
    meta[QStringLiteral("open")] = open;

    QVariantList leaders;
    for (const QJsonValue& v : metaObj.value(QLatin1String("leaders")).toArray()) {
        const QJsonObject o = v.toObject();
        QVariantMap l = shapeAuthor(o, o.value(QLatin1String("name")).toString());
        l[QStringLiteral("gained")] = o.value(QLatin1String("gained")).toInt();
        leaders.append(l);
    }
    meta[QStringLiteral("leaders")] = leaders;

    // Most wanted patches: the game's header art, and how near the target
    QVariantList wanted;
    for (const QJsonValue& v : metaObj.value(QLatin1String("wanted")).toArray()) {
        const QJsonObject o = v.toObject();
        QVariantMap w = o.toVariantMap();
        const int app = o.value(QLatin1String("app")).toInt();
        if (app > 0)
            w[QStringLiteral("art")] = QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/header.jpg").arg(app);
        const int target = o.value(QLatin1String("target")).toInt();
        w[QStringLiteral("progress")] = target > 0 ? qMin(1.0, double(o.value(QLatin1String("votes")).toInt()) / target) : 0.0;
        wanted.append(w);
    }
    meta[QStringLiteral("wanted")] = wanted;

    return {{QStringLiteral("posts"), posts}, {QStringLiteral("meta"), meta}};
}

void CommunityService::refresh(const QString& type)
{
    if (m_reply)
        m_reply->abort();

    QUrl url(QLatin1String(kSite) + QStringLiteral("/api/wall"));
    if (!type.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("type"), type);
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    security::hardenRequest(request);
    // Signed in: "liked" and a poll's pick are the member's
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    m_type = type;
    setLoading(true);
    QNetworkReply* reply = m_nam.get(request);
    m_reply = reply;
    security::installSizeGuard(reply, security::kMaxJsonResponseBytes);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError && m_reply != reply)
            return;  // a newer refresh took over
        setLoading(false);

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcCommunity) << "wall:" << reply->errorString();
            setError(tr("Topluluk yüklenemedi. İnternet bağlantını kontrol edip yeniden dene."));
            return;
        }
        const QVariantMap wall = parseWall(reply->readAll(), QDateTime::currentDateTimeUtc());
        if (wall.isEmpty()) {
            qCWarning(lcCommunity) << "wall: unexpected reply";
            setError(tr("Topluluğun cevabı okunamadı. Biraz sonra yeniden dene."));
            return;
        }
        setError(QString());
        m_posts = wall.value(QStringLiteral("posts")).toList();
        m_meta = wall.value(QStringLiteral("meta")).toMap();
        emit wallChanged();
    });
}

// ── Requests with the member's token ─────────────────────────────────────

void CommunityService::send(const QString& method, const QString& path, const QJsonObject& body,
                            const JsonHandler& done)
{
    QNetworkRequest request(QUrl(QLatin1String(kSite) + path));
    security::hardenRequest(request);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    QNetworkReply* reply = nullptr;
    if (method == QLatin1String("GET")) {
        reply = m_nam.get(request);
    } else {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_nam.post(request, body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact));
    }
    security::installSizeGuard(reply, security::kMaxJsonResponseBytes);
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void CommunityService::setToken(const QString& token)
{
    m_token = token;
    QSettings settings;
    if (token.isEmpty())
        settings.remove(QLatin1String(kTokenKey));
    else
        settings.setValue(QLatin1String(kTokenKey), SettingsManager::protectData(token.toUtf8()).toBase64());
    if (token.isEmpty())
        m_member.clear();
    emit memberChanged();
}

void CommunityService::loadMember()
{
    send(QStringLiteral("GET"), QStringLiteral("/api/uye/launcher/ben"), {}, [this](int status, const QJsonObject& body) {
        if (status == 401) {            // revoked on the site, or the account is gone
            setToken(QString());
            return;
        }
        if (!body.value(QLatin1String("ok")).toBool())
            return;                     // offline: keep the token, try next start
        m_member = shapeAuthor(body.value(QLatin1String("member")).toObject(), QString());
        emit memberChanged();
    });
}

void CommunityService::connectAccount()
{
    if (connecting()) return;
    send(QStringLiteral("POST"), QStringLiteral("/api/uye/launcher/baslat"), {}, [this](int status, const QJsonObject& body) {
        if (!body.value(QLatin1String("ok")).toBool()) {
            setNotice(failureText(status, body));
            return;
        }
        m_device = body.value(QLatin1String("device")).toString();
        m_connectCode = body.value(QLatin1String("code")).toString();
        m_connectExpires = QDateTime::currentDateTimeUtc().addSecs(body.value(QLatin1String("expiresIn")).toInt(600));
        m_pollTimer.start(qMax(2, body.value(QLatin1String("interval")).toInt(3)) * 1000);
        emit connectChanged();
        // The site's own page: sign in there (Discord or e-mail) and confirm the code
        QDesktopServices::openUrl(QUrl(body.value(QLatin1String("url")).toString()));
    });
}

void CommunityService::pollConnect()
{
    if (m_pollInFlight) return;
    if (QDateTime::currentDateTimeUtc() > m_connectExpires) {
        cancelConnect();
        setNotice(tr("Bağlanma kodunun süresi doldu. Yeniden dene."));
        return;
    }
    m_pollInFlight = true;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("device"), m_device);
    send(QStringLiteral("GET"), QStringLiteral("/api/uye/launcher/durum?") + query.toString(QUrl::FullyEncoded), {},
         [this](int status, const QJsonObject& body) {
        m_pollInFlight = false;
        const QString state = body.value(QLatin1String("status")).toString();
        if (state == QLatin1String("approved")) {
            cancelConnect();
            signedInWith(body);
        } else if (status == 410 || state == QLatin1String("expired")) {
            cancelConnect();
            setNotice(tr("Bağlanma kodunun süresi doldu. Yeniden dene."));
        }
        // pending, or the network hiccupped: ask again on the next tick
    });
}

// {token, member} from either sign-in: kept, and the wall again as this member
void CommunityService::signedInWith(const QJsonObject& body)
{
    setToken(body.value(QLatin1String("token")).toString());
    m_member = shapeAuthor(body.value(QLatin1String("member")).toObject(), QString());
    emit memberChanged();
    loadMember();               // the fuller record (needName, abilities)
    setNotice(tr("Hesabın bağlandı. Artık buradan paylaşabilirsin."));
    refresh(m_type);
}

void CommunityService::sendCode(const QString& email)
{
    const QString address = email.trimmed();
    if (m_signingIn || address.isEmpty()) return;
    m_signingIn = true;
    emit emailSignInChanged();
    send(QStringLiteral("POST"), QStringLiteral("/api/uye/launcher/eposta"),
         QJsonObject{{QStringLiteral("email"), address}}, [this, address](int status, const QJsonObject& body) {
        m_signingIn = false;
        if (body.value(QLatin1String("ok")).toBool()) {
            m_codeEmail = address;
            setNotice(QString());
        } else {
            setNotice(failureText(status, body));
        }
        emit emailSignInChanged();
    });
}

void CommunityService::verifyCode(const QString& code)
{
    if (m_signingIn || m_codeEmail.isEmpty()) return;
    m_signingIn = true;
    emit emailSignInChanged();
    send(QStringLiteral("POST"), QStringLiteral("/api/uye/launcher/eposta/dogrula"),
         QJsonObject{{QStringLiteral("email"), m_codeEmail}, {QStringLiteral("code"), code.trimmed()}},
         [this](int status, const QJsonObject& body) {
        m_signingIn = false;
        if (body.value(QLatin1String("ok")).toBool() && !body.value(QLatin1String("token")).toString().isEmpty()) {
            m_codeEmail.clear();
            signedInWith(body);
        } else {
            setNotice(failureText(status, body));
        }
        emit emailSignInChanged();
    });
}

void CommunityService::resetCode()
{
    m_codeEmail.clear();
    emit emailSignInChanged();
}

void CommunityService::setName(const QString& name)
{
    if (m_token.isEmpty() || m_signingIn || name.trimmed().isEmpty()) return;
    m_signingIn = true;
    emit emailSignInChanged();
    send(QStringLiteral("POST"), QStringLiteral("/api/uye/launcher/profil"),
         QJsonObject{{QStringLiteral("name"), name.trimmed()}}, [this](int status, const QJsonObject& body) {
        m_signingIn = false;
        emit emailSignInChanged();
        const QJsonObject member = body.value(QLatin1String("member")).toObject();
        if (!body.value(QLatin1String("ok")).toBool() || member.isEmpty()) {
            setNotice(failureText(status, body));
            return;
        }
        m_member = shapeAuthor(member, QString());
        emit memberChanged();
        setNotice(tr("Adın kaydedildi. Artık paylaşabilirsin."));
    });
}

void CommunityService::cancelConnect()
{
    m_pollTimer.stop();
    m_device.clear();
    m_connectCode.clear();
    emit connectChanged();
}

void CommunityService::signOut()
{
    if (m_token.isEmpty()) return;
    send(QStringLiteral("POST"), QStringLiteral("/api/uye/launcher/cikis"), {}, [](int, const QJsonObject&) {});
    setToken(QString());
    setNotice(tr("Hesabın bu bilgisayardan çıkarıldı."));
    refresh(m_type);
}

void CommunityService::post(const QString& body, const QString& type, const QString& topicName)
{
    if (m_token.isEmpty() || m_posting) return;
    QJsonObject payload{
        {QStringLiteral("game"), QStringLiteral("topluluk")},
        {QStringLiteral("body"), body.trimmed()},
        {QStringLiteral("type"), type.isEmpty() ? QStringLiteral("sohbet") : type},
    };
    if (!topicName.trimmed().isEmpty())
        payload[QStringLiteral("topicName")] = topicName.trimmed().left(80);

    setPosting(true);
    send(QStringLiteral("POST"), QStringLiteral("/api/comments"), payload, [this](int status, const QJsonObject& b) {
        setPosting(false);
        if (!b.value(QLatin1String("ok")).toBool()) {
            if (status == 401) setToken(QString());
            if (b.value(QLatin1String("needName")).toBool())
                QDesktopServices::openUrl(QUrl(QLatin1String(kSite) + QStringLiteral("/topluluk/")));
            setNotice(failureText(status, b));
            return;
        }
        const bool published = b.value(QLatin1String("status")).toString() == QLatin1String("approved");
        setNotice(published ? tr("Paylaşıldı.")
                            : tr("Paylaşımın kısa bir incelemeden sonra yayına girecek."));
        emit posted(published);
        refresh(m_type);
    });
}

void CommunityService::reply(int parentId, const QString& body)
{
    if (m_token.isEmpty() || m_posting) return;
    const QJsonObject payload{
        {QStringLiteral("game"), QStringLiteral("topluluk")},
        {QStringLiteral("parentId"), parentId},
        {QStringLiteral("body"), body.trimmed()},
    };
    setPosting(true);
    send(QStringLiteral("POST"), QStringLiteral("/api/comments"), payload, [this](int status, const QJsonObject& b) {
        setPosting(false);
        if (!b.value(QLatin1String("ok")).toBool()) {
            if (status == 401) setToken(QString());
            setNotice(failureText(status, b));
            return;
        }
        const bool published = b.value(QLatin1String("status")).toString() == QLatin1String("approved");
        setNotice(published ? tr("Yanıtın gönderildi.")
                            : tr("Yanıtın kısa bir incelemeden sonra görünecek."));
        emit posted(published);
        refresh(m_type);
    });
}

void CommunityService::like(int postId)
{
    if (m_token.isEmpty()) return;
    send(QStringLiteral("POST"), QStringLiteral("/api/wall/like"), {{QStringLiteral("id"), postId}},
         [this, postId](int status, const QJsonObject& b) {
        if (!b.value(QLatin1String("ok")).toBool()) {
            setNotice(failureText(status, b));
            return;
        }
        const bool liked = b.value(QLatin1String("liked")).toBool();
        const int likes = b.value(QLatin1String("likes")).toInt();
        updatePost(postId, [liked, likes](QVariantMap& p) {
            p[QStringLiteral("liked")] = liked;
            p[QStringLiteral("likes")] = likes;
        });
    });
}

void CommunityService::vote(int pollId, int choice)
{
    if (m_token.isEmpty()) return;
    send(QStringLiteral("POST"), QStringLiteral("/api/wall/oy"),
         {{QStringLiteral("id"), pollId}, {QStringLiteral("choice"), choice}},
         [this, pollId](int status, const QJsonObject& b) {
        if (!b.value(QLatin1String("ok")).toBool()) {
            setNotice(failureText(status, b));
            return;
        }
        const QJsonObject poll = b.value(QLatin1String("poll")).toObject();
        updatePost(pollId, [&poll](QVariantMap& p) { shapePoll(p, poll); });
    });
}

void CommunityService::updatePost(int postId, const std::function<void(QVariantMap&)>& change)
{
    for (QVariant& v : m_posts) {
        QVariantMap p = v.toMap();
        if (p.value(QStringLiteral("id")).toInt() != postId) continue;
        change(p);
        v = p;
        emit wallChanged();
        return;
    }
}

void CommunityService::clearNotice() { setNotice(QString()); }

void CommunityService::setLoading(bool loading)
{
    if (m_loading == loading) return;
    m_loading = loading;
    emit loadingChanged();
}

void CommunityService::setError(const QString& error)
{
    if (m_error == error) return;
    m_error = error;
    emit errorChanged();
}

void CommunityService::setNotice(const QString& notice)
{
    if (m_notice == notice) return;
    m_notice = notice;
    emit noticeChanged();
}

void CommunityService::setPosting(bool posting)
{
    if (m_posting == posting) return;
    m_posting = posting;
    emit postingChanged();
}

} // namespace makine
