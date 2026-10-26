// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file communityservice.h
 * @brief The website's community wall (makineceviri.org/topluluk)
 *
 * GET https://makineceviri.org/api/wall answers anonymously: the latest
 * threads and the side rails (most wanted patches, unanswered questions,
 * this month's most active members, counts).
 *
 * Taking part needs a member. The launcher signs in by e-mail the way the
 * site does — a 6-digit code to the address (/api/uye/launcher/eposta),
 * typed back here (/api/uye/launcher/eposta/dogrula) — or, for Discord, the
 * way TVs do: it asks the site for a device code (/api/uye/launcher/baslat),
 * opens the site's page for it in the browser, where the member signs in and
 * confirms, and polls until the site hands over the token
 * (/api/uye/launcher/durum). Either way the result is a bearer token. The token is kept DPAPI-encrypted (SettingsManager::protectData) and sent
 * as `Authorization: Bearer` to post, reply, like and vote; the same rules
 * hold as on the site (moderation, rate limits, 500 characters).
 *
 * The C++ side shapes each post for the page (type label, "3 saat önce",
 * initials, absolute avatar URL, poll shares); QML only binds.
 */

#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QNetworkReply;
class QJsonObject;

namespace makine {

class CommunityService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList posts READ posts NOTIFY wallChanged)
    Q_PROPERTY(QVariantMap meta READ meta NOTIFY wallChanged)
    Q_PROPERTY(QString type READ type NOTIFY wallChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(bool signedIn READ signedIn NOTIFY memberChanged)
    Q_PROPERTY(QVariantMap member READ member NOTIFY memberChanged)
    /// While connecting: the code the site's page shows ("C5RK-F6DH")
    Q_PROPERTY(QString connectCode READ connectCode NOTIFY connectChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectChanged)
    /// E-mail sign-in: the address the code went to ("" before), a request in flight
    Q_PROPERTY(QString codeEmail READ codeEmail NOTIFY emailSignInChanged)
    Q_PROPERTY(bool signingIn READ signingIn NOTIFY emailSignInChanged)
    Q_PROPERTY(bool posting READ posting NOTIFY postingChanged)
    /// The last post/like/vote outcome in a sentence ("" when none)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

public:
    explicit CommunityService(QObject* parent = nullptr);

    QVariantList posts() const { return m_posts; }
    QVariantMap meta() const { return m_meta; }
    QString type() const { return m_type; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    bool signedIn() const { return !m_token.isEmpty(); }
    QVariantMap member() const { return m_member; }
    QString connectCode() const { return m_connectCode; }
    bool connecting() const { return m_pollTimer.isActive(); }
    QString codeEmail() const { return m_codeEmail; }
    bool signingIn() const { return m_signingIn; }
    bool posting() const { return m_posting; }
    QString notice() const { return m_notice; }

    /// The wall again, of one post type ("" every type · soru · oneri ·
    /// istek · arkadas · sohbet · anket · ekip)
    Q_INVOKABLE void refresh(const QString& type = QString());

    /// Sign in by e-mail: a code to the address, then the code typed back
    Q_INVOKABLE void sendCode(const QString& email);
    Q_INVOKABLE void verifyCode(const QString& code);
    /// Back to the address (another one, or the code never came)
    Q_INVOKABLE void resetCode();
    /// An e-mail member's name on the wall (member.needName until chosen)
    Q_INVOKABLE void setName(const QString& name);
    /// Sign in through the site (Discord): a code, the site's page in the browser, then polling
    Q_INVOKABLE void connectAccount();
    Q_INVOKABLE void cancelConnect();
    Q_INVOKABLE void signOut();

    /// A new post on the wall. `type` sohbet · soru · oneri · istek · arkadas;
    /// `topicName` the game it is about ("" none; a patch request needs one)
    Q_INVOKABLE void post(const QString& body, const QString& type, const QString& topicName);
    Q_INVOKABLE void reply(int parentId, const QString& body);
    Q_INVOKABLE void like(int postId);
    Q_INVOKABLE void vote(int pollId, int choice);
    Q_INVOKABLE void clearNotice();

    /// /api/wall's JSON → {posts, meta}, each post shaped for the page.
    /// An empty map when the reply is not the wall. `now` dates the posts.
    static QVariantMap parseWall(const QByteArray& json, const QDateTime& now);
    /// "az önce" · "5 dk önce" · "3 sa önce" · "dün" · "4 gün önce" · "12 Eki"
    static QString relativeTime(const QString& timestamp, const QDateTime& now);

signals:
    void wallChanged();
    void loadingChanged();
    void errorChanged();
    void memberChanged();
    void connectChanged();
    void emailSignInChanged();
    void postingChanged();
    void noticeChanged();
    /// A post or reply went out (approved or waiting for review)
    void posted(bool published);

private:
    using JsonHandler = std::function<void(int status, const QJsonObject& body)>;
    void send(const QString& method, const QString& path, const QJsonObject& body, const JsonHandler& done);
    void setLoading(bool loading);
    void setError(const QString& error);
    void setNotice(const QString& notice);
    void setPosting(bool posting);
    void setToken(const QString& token);
    void loadMember();
    void pollConnect();
    void signedInWith(const QJsonObject& body);
    void updatePost(int postId, const std::function<void(QVariantMap&)>& change);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_reply;
    QVariantList m_posts;
    QVariantMap m_meta;
    QString m_type;
    bool m_loading = false;
    QString m_error;

    QString m_token;
    QVariantMap m_member;
    QString m_device;
    QString m_connectCode;
    QTimer m_pollTimer;
    QDateTime m_connectExpires;
    bool m_pollInFlight = false;
    QString m_codeEmail;
    bool m_signingIn = false;
    bool m_posting = false;
    QString m_notice;
};

} // namespace makine
