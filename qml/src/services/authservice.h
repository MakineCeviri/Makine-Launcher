/**
 * @file authservice.h
 * @brief Authentication service — PKCE flow, token management, auth state
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcAuth)

namespace makine {

class AuthService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AuthState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY userChanged)
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY stateChanged)

public:
    enum AuthState {
        Checking,
        Unauthenticated,
        WaitingForBrowser,
        Exchanging,
        Authenticated,
        Refreshing
    };
    Q_ENUM(AuthState)

    explicit AuthService(QObject* parent = nullptr);
    ~AuthService() override;

    static AuthService* instance() { return s_instance; }

    AuthState state() const { return m_state; }
    QString userEmail() const { return m_userEmail; }
    bool isAuthenticated() const { return m_state == Authenticated || m_state == Refreshing; }

    Q_INVOKABLE void checkStoredToken();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void retryLogin();

    void handleAuthCallback(const QString& code, const QString& state);

    QString accessToken() const { return m_accessToken; }

signals:
    void stateChanged();
    void userChanged();
    void loginError(const QString& message);

private slots:
    void onRefreshTimer();

private:
    void setState(AuthState newState);
    void exchangeCodeForTokens(const QString& code);
    void refreshAccessToken();
    void fetchUserProfile();
    void storeRefreshToken(const QString& token);
    QString loadRefreshToken() const;
    void clearTokens();

    QString generateCodeVerifier();
    QString generateCodeChallenge(const QString& verifier);
    QString generateState();

    static AuthService* s_instance;

    AuthState m_state{Checking};
    QString m_accessToken;
    QString m_userEmail;

    QString m_codeVerifier;
    QString m_stateNonce;

    QNetworkAccessManager* m_nam{nullptr};
    QTimer* m_refreshTimer{nullptr};
    QTimer* m_callbackTimeout{nullptr};

    static constexpr int kRefreshIntervalMs = 12 * 60 * 1000;
    static constexpr int kCallbackTimeoutMs = 2 * 60 * 1000;
};

} // namespace makine
