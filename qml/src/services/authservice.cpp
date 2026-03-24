/**
 * @file authservice.cpp
 * @brief Authentication service implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "authservice.h"
#include "apppaths.h"
#include "networksecurity.h"
#include "crashreporter.h"

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSslError>
#include <QSslCertificate>
#include <QSslKey>

#ifndef MAKINE_UI_ONLY
#include <makine/credential_store.hpp>
#endif

Q_LOGGING_CATEGORY(lcAuth, "makine.security")

namespace {
constexpr const char* AUTH_BASE_URL = "https://makineceviri.org/hesap";
constexpr const char* API_BASE_URL = "https://makineceviri.org/api/auth";
constexpr const char* CLIENT_ID = "makine-launcher";
constexpr const char* CRED_KEY = "RefreshToken";
}

namespace makine {

AuthService* AuthService::s_instance = nullptr;

AuthService::AuthService(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
    , m_callbackTimeout(new QTimer(this))
{
    s_instance = this;

    m_refreshTimer->setInterval(kRefreshIntervalMs);
    m_refreshTimer->setSingleShot(false);
    connect(m_refreshTimer, &QTimer::timeout, this, &AuthService::onRefreshTimer);

    m_callbackTimeout->setInterval(kCallbackTimeoutMs);
    m_callbackTimeout->setSingleShot(true);
    connect(m_callbackTimeout, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForBrowser) {
            m_codeVerifier.clear();
            m_stateNonce.clear();
            setState(Unauthenticated);
            emit loginError(tr("Zaman aşımı — tarayıcıdan yanıt alınamadı"));
        }
    });

    // TLS certificate pinning — shared implementation for all domains
    security::installTlsPinning(m_nam);
}

AuthService::~AuthService()
{
    if (s_instance == this) s_instance = nullptr;
}

// -- State --

void AuthService::setState(AuthState newState)
{
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged();

    if (newState == Authenticated) {
        m_refreshTimer->start();
        CrashReporter::addBreadcrumb("auth", "Authenticated");
    } else if (newState == Unauthenticated) {
        m_refreshTimer->stop();
    }
}

// -- Startup check --

void AuthService::checkStoredToken()
{
#ifdef MAKINE_UI_ONLY
    // UI-only builds skip auth — CredentialStore unavailable
    setState(Authenticated);
    return;
#endif
    setState(Checking);
    QString refresh = loadRefreshToken();
    if (refresh.isEmpty()) {
        setState(Unauthenticated);
        return;
    }

    // Safety timeout: if refresh fails to respond within 5s, go to Unauthenticated
    QTimer::singleShot(5000, this, [this]() {
        if (m_state == Checking) {
            qCWarning(lcAuth) << "Token refresh timed out — falling back to login";
            setState(Unauthenticated);
        }
    });
    refreshAccessToken();
}

// -- Login flow --

void AuthService::startLogin()
{
    m_codeVerifier = generateCodeVerifier();
    m_stateNonce = generateState();
    QString challenge = generateCodeChallenge(m_codeVerifier);

    QUrl url(QString::fromLatin1(AUTH_BASE_URL));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("launcher"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("state"), m_stateNonce);
    url.setQuery(query);

    setState(WaitingForBrowser);
    m_callbackTimeout->start();

    if (!QDesktopServices::openUrl(url)) {
        emit loginError(tr("Tarayıcı açılamadı. URL'yi kopyalayın:\n%1").arg(url.toString()));
    }

    qCDebug(lcAuth) << "Login flow started, browser opened";
}

void AuthService::retryLogin()
{
    m_callbackTimeout->stop();
    m_codeVerifier.clear();
    m_stateNonce.clear();
    startLogin();
}

void AuthService::handleAuthCallback(const QString& code, const QString& state)
{
    m_callbackTimeout->stop();

    if (state != m_stateNonce) {
        qCWarning(lcAuth) << "State mismatch — possible CSRF attempt";
        emit loginError(tr("Güvenlik doğrulaması başarısız — tekrar deneyin"));
        setState(Unauthenticated);
        m_codeVerifier.clear();
        m_stateNonce.clear();
        return;
    }

    setState(Exchanging);
    exchangeCodeForTokens(code);
}

// -- Token exchange --

void AuthService::exchangeCodeForTokens(const QString& code)
{
    QJsonObject body;
    body[QStringLiteral("code")] = code;
    body[QStringLiteral("code_verifier")] = m_codeVerifier;
    body[QStringLiteral("client_id")] = QLatin1String(CLIENT_ID);

    m_codeVerifier.clear();
    m_stateNonce.clear();

    QNetworkRequest req(QUrl(QStringLiteral("%1/token-exchange").arg(QLatin1String(API_BASE_URL))));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    auto* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcAuth) << "Token exchange failed:" << reply->errorString();
            emit loginError(tr("Doğrulama başarısız — tekrar deneyin"));
            setState(Unauthenticated);
            return;
        }

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(reply->readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit loginError(tr("Sunucu yanıtı geçersiz"));
            setState(Unauthenticated);
            return;
        }

        auto obj = doc.object();
        m_accessToken = obj.value(QStringLiteral("access_token")).toString();
        QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString();

        if (m_accessToken.isEmpty() || refreshToken.isEmpty()) {
            emit loginError(tr("Token alınamadı"));
            setState(Unauthenticated);
            return;
        }

        storeRefreshToken(refreshToken);
        fetchUserProfile();
    });
}

// -- Token refresh --

void AuthService::refreshAccessToken()
{
    QString refresh = loadRefreshToken();
    if (refresh.isEmpty()) {
        setState(Unauthenticated);
        return;
    }

    if (m_state == Authenticated)
        setState(Refreshing);

    QJsonObject body;
    body[QStringLiteral("refresh_token")] = refresh;
    body[QStringLiteral("client_id")] = QLatin1String(CLIENT_ID);

    QNetworkRequest req(QUrl(QStringLiteral("%1/refresh").arg(QLatin1String(API_BASE_URL))));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    auto* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcAuth) << "Token refresh failed:" << reply->errorString();
            clearTokens();
            setState(Unauthenticated);
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto obj = doc.object();

        m_accessToken = obj.value(QStringLiteral("access_token")).toString();
        QString newRefresh = obj.value(QStringLiteral("refresh_token")).toString();

        if (!m_accessToken.isEmpty()) {
            if (!newRefresh.isEmpty())
                storeRefreshToken(newRefresh);
            if (m_state != Authenticated)
                fetchUserProfile();
            else
                setState(Authenticated);
        } else {
            clearTokens();
            setState(Unauthenticated);
        }
    });
}

void AuthService::onRefreshTimer()
{
    if (m_state == Authenticated)
        refreshAccessToken();
}

// -- User profile --

void AuthService::fetchUserProfile()
{
    QNetworkRequest req(QUrl(QStringLiteral("%1/me").arg(QLatin1String(API_BASE_URL))));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcAuth) << "Profile fetch failed:" << reply->errorString();
            setState(Authenticated);
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto obj = doc.object();
        QString email = obj.value(QStringLiteral("email")).toString();

        if (email.contains(QLatin1Char('@'))) {
            int at = email.indexOf(QLatin1Char('@'));
            m_userEmail = (at > 2)
                ? email.left(2) + QStringLiteral("***") + email.mid(at)
                : QStringLiteral("***") + email.mid(at);
        }
        emit userChanged();
        setState(Authenticated);
        qCDebug(lcAuth) << "Authenticated as" << m_userEmail;
    });
}

// -- Logout --

void AuthService::logout()
{
    if (!m_accessToken.isEmpty()) {
        QNetworkRequest req(QUrl(QStringLiteral("%1/logout").arg(QLatin1String(API_BASE_URL))));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        m_nam->post(req, QByteArray("{}"));
    }

    clearTokens();
    m_userEmail.clear();
    emit userChanged();
    setState(Unauthenticated);
    qCDebug(lcAuth) << "Logged out";
}

// -- Credential storage --

void AuthService::storeRefreshToken(const QString& token)
{
#ifndef MAKINE_UI_ONLY
    auto result = CredentialStore::save(CRED_KEY, token.toStdString());
    if (!result) {
        qCWarning(lcAuth) << "Failed to store refresh token";
    }
#else
    Q_UNUSED(token)
#endif
}

QString AuthService::loadRefreshToken() const
{
#ifndef MAKINE_UI_ONLY
    auto val = CredentialStore::load(CRED_KEY);
    return val ? QString::fromStdString(*val) : QString();
#else
    return {};
#endif
}

void AuthService::clearTokens()
{
    m_accessToken.clear();
    m_refreshTimer->stop();
#ifndef MAKINE_UI_ONLY
    (void)CredentialStore::remove(CRED_KEY);
#endif
}

// -- PKCE --

QString AuthService::generateCodeVerifier()
{
    const int len = 64;
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    QString verifier;
    verifier.reserve(len);
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < len; ++i)
        verifier.append(QLatin1Char(charset[rng->bounded(static_cast<int>(sizeof(charset) - 1))]));
    return verifier;
}

QString AuthService::generateCodeChallenge(const QString& verifier)
{
    QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString AuthService::generateState()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(bytes.data()),
                                           bytes.size() / static_cast<int>(sizeof(quint32)));
    return QString::fromLatin1(bytes.toHex());
}

} // namespace makine
