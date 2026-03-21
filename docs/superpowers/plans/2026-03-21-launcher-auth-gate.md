# Launcher Auth Gate — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Require users to authenticate via makineceviri.org before using the launcher, using PKCE authorization code flow with `makine://` custom protocol.

**Architecture:** AuthService (C++ singleton) manages PKCE, token exchange, credential storage, and auto-refresh. LoginScreen.qml gates the UI. QLocalServer handles IPC for auth callbacks when app is already running. Protocol handler registered at startup.

**Tech Stack:** Qt6/C++23, Windows Credential Manager (existing CredentialStore), QLocalServer, QNetworkAccessManager, SHA256 (QCryptographicHash)

**Spec:** `docs/superpowers/specs/2026-03-21-launcher-auth-gate-design.md`

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `qml/src/services/authservice.h` | Create | Auth state machine, PKCE, token management |
| `qml/src/services/authservice.cpp` | Create | Implementation |
| `qml/qml/screens/LoginScreen.qml` | Create | Login gate UI |
| `qml/src/main.cpp` | Modify | Protocol handler, QLocalServer IPC, AuthService init |
| `qml/qml/Main.qml` | Modify | Auth gate before main content |
| `qml/CMakeLists.txt` | Modify | Register new files |

---

### Task 1: AuthService Header

**Files:**
- Create: `qml/src/services/authservice.h`

- [ ] **Step 1: Create authservice.h**

```cpp
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
        Checking,           // Startup — verifying stored token
        Unauthenticated,    // No valid token — show login
        WaitingForBrowser,  // Browser opened, awaiting callback
        Exchanging,         // Received code, exchanging for tokens
        Authenticated,      // Valid token — app usable
        Refreshing          // Auto-refreshing (stays authenticated)
    };
    Q_ENUM(AuthState)

    explicit AuthService(QObject* parent = nullptr);
    ~AuthService() override;

    static AuthService* instance() { return s_instance; }

    // Properties
    AuthState state() const { return m_state; }
    QString userEmail() const { return m_userEmail; }
    bool isAuthenticated() const { return m_state == Authenticated || m_state == Refreshing; }

    // Actions
    Q_INVOKABLE void checkStoredToken();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void retryLogin();

    // Called from main.cpp when auth callback received (via protocol handler or IPC)
    void handleAuthCallback(const QString& code, const QString& state);

    // Access token for API calls (memory only, never persisted)
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

    // PKCE helpers
    QString generateCodeVerifier();
    QString generateCodeChallenge(const QString& verifier);
    QString generateState();

    static AuthService* s_instance;

    AuthState m_state{Checking};
    QString m_accessToken;
    QString m_userEmail;

    // PKCE state (memory only, cleared after exchange)
    QString m_codeVerifier;
    QString m_stateNonce;

    QNetworkAccessManager* m_nam{nullptr};
    QTimer* m_refreshTimer{nullptr};
    QTimer* m_callbackTimeout{nullptr};

    static constexpr int kRefreshIntervalMs = 12 * 60 * 1000;  // 12 min
    static constexpr int kCallbackTimeoutMs = 2 * 60 * 1000;   // 2 min
};

} // namespace makine
```

- [ ] **Step 2: Commit**

```bash
git add qml/src/services/authservice.h
git commit -m "feat(ui): add AuthService header with PKCE auth state machine"
```

---

### Task 2: AuthService Implementation

**Files:**
- Create: `qml/src/services/authservice.cpp`

- [ ] **Step 1: Create authservice.cpp**

```cpp
/**
 * @file authservice.cpp
 * @brief Authentication service implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "authservice.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QRandomGenerator>

#ifndef MAKINE_UI_ONLY
#include <makine/credential_store.hpp>
#endif

Q_LOGGING_CATEGORY(lcAuth, "makine.security")

namespace {
constexpr const char* AUTH_BASE_URL = "https://makineceviri.org/hesap";
constexpr const char* API_BASE_URL = "https://makineceviri.org/api/auth";
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
}

AuthService::~AuthService()
{
    if (s_instance == this) s_instance = nullptr;
}

// ── State management ──

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

// ── Startup check ──

void AuthService::checkStoredToken()
{
#ifdef MAKINE_UI_ONLY
    // UI-only builds (dev-ui) skip auth — CredentialStore unavailable
    setState(Authenticated);
    return;
#endif
    setState(Checking);
    QString refresh = loadRefreshToken();
    if (refresh.isEmpty()) {
        setState(Unauthenticated);
        return;
    }
    refreshAccessToken();
}

// ── Login flow ──

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

    // Verify state to prevent CSRF
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

// ── Token exchange ──

void AuthService::exchangeCodeForTokens(const QString& code)
{
    QJsonObject body;
    body[QStringLiteral("code")] = code;
    body[QStringLiteral("code_verifier")] = m_codeVerifier;

    // Clear PKCE state immediately (one-time use)
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

// ── Token refresh ──

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
                setState(Authenticated);  // Back from Refreshing
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

// ── User profile ──

void AuthService::fetchUserProfile()
{
    QNetworkRequest req(QUrl(QStringLiteral("%1/me").arg(QLatin1String(API_BASE_URL))));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcAuth) << "Profile fetch failed:" << reply->errorString();
            // Token might still be valid, proceed anyway
            setState(Authenticated);
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto obj = doc.object();
        QString email = obj.value(QStringLiteral("email")).toString();

        // Mask email for display: ab***@domain.com
        if (email.contains(QLatin1Char('@'))) {
            int at = email.indexOf(QLatin1Char('@'));
            if (at > 2)
                m_userEmail = email.left(2) + QStringLiteral("***") + email.mid(at);
            else
                m_userEmail = QStringLiteral("***") + email.mid(at);
        }
        emit userChanged();
        setState(Authenticated);
        qCDebug(lcAuth) << "Authenticated as" << m_userEmail;
    });
}

// ── Logout ──

void AuthService::logout()
{
    // Server-side invalidation
    if (!m_accessToken.isEmpty()) {
        QNetworkRequest req(QUrl(QStringLiteral("%1/logout").arg(QLatin1String(API_BASE_URL))));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        m_nam->post(req, QByteArray("{}"));  // Fire and forget
    }

    clearTokens();
    m_userEmail.clear();
    emit userChanged();
    setState(Unauthenticated);
    qCDebug(lcAuth) << "Logged out";
}

// ── Credential storage ──

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

// ── PKCE helpers ──

QString AuthService::generateCodeVerifier()
{
    // RFC 7636: 43-128 chars from [A-Z, a-z, 0-9, -, ., _, ~]
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
    // SHA256 hash, base64url-encoded (no padding)
    QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString AuthService::generateState()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(bytes.data()),
                                           bytes.size() / sizeof(quint32));
    return QString::fromLatin1(bytes.toHex());
}

} // namespace makine
```

- [ ] **Step 2: Build and verify**

```bash
just dev
```

- [ ] **Step 3: Commit**

```bash
git add qml/src/services/authservice.h qml/src/services/authservice.cpp
git commit -m "feat(ui): implement AuthService with PKCE, token exchange, and credential storage"
```

---

### Task 3: Protocol Handler + IPC

**Files:**
- Modify: `qml/src/main.cpp`

Wire protocol handler registration, `--auth-callback` handling, and QLocalServer IPC.

- [ ] **Step 1: Add includes and protocol registration**

Add to main.cpp includes:
```cpp
#include <QLocalServer>
#include <QLocalSocket>
#include <QUrlQuery>
#include "services/authservice.h"
```

Add protocol handler registration function (after `configureQtEnvironment`):
```cpp
static void registerProtocolHandler()
{
#ifdef Q_OS_WIN
    // Register makine:// protocol in current user registry.
    // Windows replaces %1 with the invoked URL at runtime.
    QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\makine"), QSettings::NativeFormat);
    reg.setValue(QStringLiteral("Default"), QStringLiteral("Makine Launcher Auth"));
    reg.setValue(QStringLiteral("URL Protocol"), QString());
    QString cmd = QLatin1Char('"') + exePath + QStringLiteral("\" \"--auth-callback\" \"%1\"");
    reg.setValue(QStringLiteral("shell/open/command/Default"), cmd);
    qCDebug(lcApp) << "Protocol handler registered: makine://";
#endif
}
```

- [ ] **Step 2: Add IPC and callback handling**

In `main()`, before `QGuiApplication` construction, add `--auth-callback` check.
Then AFTER `createServices()` returns, wire IPC server and handle pending callback.
This avoids scope issues with `createServices()`'s fixed signature.

```cpp
// ── In main(), BEFORE QGuiApplication construction ──

// Check for auth callback before creating app
QString authCallbackUrl;
for (int i = 1; i < argc; ++i) {
    if (QString::fromLocal8Bit(argv[i]) == "--auth-callback" && i + 1 < argc) {
        authCallbackUrl = QString::fromLocal8Bit(argv[i + 1]);
        break;
    }
}

// If auth callback, try to send to running instance via IPC
if (!authCallbackUrl.isEmpty()) {
    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("MakineLauncher_AuthIPC"));
    if (socket.waitForConnected(1000)) {
        socket.write(authCallbackUrl.toUtf8());
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;  // Exit — running instance handles callback
    }
    // If no running instance, continue normal startup and handle callback after init
}
```

```cpp
// ── Inside createServices(), add AuthService creation ──

auto* authService = new AuthService(&app);
engine.rootContext()->setContextProperty("AuthService", authService);
```

```cpp
// ── In main(), AFTER createServices() returns ──

// IPC server for auth callbacks (uses disconnected for complete data)
auto* authService = AuthService::instance();
auto* ipcServer = new QLocalServer(&app);
ipcServer->setSocketOptions(QLocalServer::UserAccessOption);
ipcServer->listen(QStringLiteral("MakineLauncher_AuthIPC"));
QObject::connect(ipcServer, &QLocalServer::newConnection, [authService, ipcServer]() {
    auto* socket = ipcServer->nextPendingConnection();
    // Read on disconnect — guarantees all data is received
    QObject::connect(socket, &QLocalSocket::disconnected, [authService, socket]() {
        QUrl url(QString::fromUtf8(socket->readAll()));
        QUrlQuery query(url);
        QString code = query.queryItemValue(QStringLiteral("code"));
        QString state = query.queryItemValue(QStringLiteral("state"));
        if (!code.isEmpty())
            authService->handleAuthCallback(code, state);
        socket->deleteLater();
    });
});

// Handle callback if THIS instance was launched with --auth-callback
if (!authCallbackUrl.isEmpty()) {
    QTimer::singleShot(500, [authService, authCallbackUrl]() {
        QUrl url(authCallbackUrl);
        QUrlQuery query(url);
        authService->handleAuthCallback(
            query.queryItemValue(QStringLiteral("code")),
            query.queryItemValue(QStringLiteral("state")));
    });
}

// Register protocol handler
registerProtocolHandler();
```

- [ ] **Step 3: Build and verify**

```bash
just dev
```

- [ ] **Step 4: Commit**

```bash
git add qml/src/main.cpp
git commit -m "feat(ui): add makine:// protocol handler and QLocalServer IPC for auth callbacks"
```

---

### Task 4: LoginScreen QML

**Files:**
- Create: `qml/qml/screens/LoginScreen.qml`

- [ ] **Step 1: Create LoginScreen.qml**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * LoginScreen.qml — Auth gate shown when user is not authenticated
 */
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: Math.min(400, parent.width - 60)

        // Logo
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/qt/qml/MakineLauncher/assets/logo.png"
            sourceSize: Qt.size(80, 80)
            fillMode: Image.PreserveAspectFit
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Makine \u00C7eviri"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        // Subtitle
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Devam etmek i\u00E7in giri\u015F yap\u0131n")
            font.pixelSize: 14
            color: Theme.textSecondary
        }

        Item { Layout.preferredHeight: 8 }

        // Login button
        Button {
            id: loginBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 240
            Layout.preferredHeight: 44
            text: AuthService.state === AuthService.WaitingForBrowser
                  ? qsTr("Taray\u0131c\u0131dan giri\u015F bekleniyor...")
                  : qsTr("Giri\u015F Yap")
            enabled: AuthService.state === AuthService.Unauthenticated

            background: Rectangle {
                radius: 8
                color: loginBtn.enabled
                       ? (loginBtn.hovered ? Theme.accentHover : Theme.accent)
                       : Theme.bgTertiary
            }
            contentItem: Text {
                text: loginBtn.text
                color: "white"
                font.pixelSize: 14
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: AuthService.startLogin()
        }

        // Waiting spinner
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: AuthService.state === AuthService.WaitingForBrowser
                     || AuthService.state === AuthService.Exchanging
                     || AuthService.state === AuthService.Checking
            visible: running
        }

        // Error message
        Text {
            id: errorText
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: parent.width
            visible: text.length > 0
            color: Theme.error
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        // Retry button (shown after timeout/error)
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: errorText.visible
            text: qsTr("Tekrar Dene")
            flat: true
            onClicked: {
                errorText.text = ""
                AuthService.retryLogin()
            }
            contentItem: Text {
                text: parent.text
                color: Theme.accent
                font.pixelSize: 13
                font.underline: true
            }
            background: Item {}
        }

        // Register link
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Hesab\u0131n\u0131z yok mu? <a href='https://makineceviri.org/hesap'>Kay\u0131t olun</a>")
            color: Theme.textSecondary
            font.pixelSize: 12
            textFormat: Text.RichText
            onLinkActivated: link => Qt.openUrlExternally(link)

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // Connect error signal
    Connections {
        target: AuthService
        function onLoginError(message) {
            errorText.text = message
        }
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add qml/qml/screens/LoginScreen.qml
git commit -m "feat(ui): add LoginScreen with PKCE login flow and error handling"
```

---

### Task 5: Main.qml Auth Gate

**Files:**
- Modify: `qml/qml/Main.qml`

- [ ] **Step 1: Add auth gate loader**

In Main.qml, after the `_onboardingActive` property and before existing content, add an auth-aware Loader:

```qml
// Auth gate — blocks all content until authenticated
property bool _authReady: typeof AuthService !== "undefined"
                          && AuthService.isAuthenticated

Component.onCompleted: {
    if (typeof AuthService !== "undefined")
        AuthService.checkStoredToken()
    if (typeof SettingsManager !== "undefined")
        window._onboardingActive = !SettingsManager.onboardingCompleted
}
```

Wrap the existing content area (everything after title bar) in a conditional:
```qml
// Login screen — shown when not authenticated
Loader {
    anchors.fill: parent
    active: !window._authReady
    visible: active
    z: 100  // Above all other content
    source: "qrc:/qt/qml/MakineLauncher/qml/screens/LoginScreen.qml"
}
```

The existing main content (navbar, screens, etc.) remains but is hidden behind the login screen via z-ordering. When `_authReady` becomes true, the Loader deactivates and main content is visible.

- [ ] **Step 2: Add user email display to settings or title bar (optional)**

In the settings area or NavBar, show the masked email:
```qml
Text {
    visible: AuthService.isAuthenticated
    text: AuthService.userEmail
    color: Theme.textSecondary
    font.pixelSize: 11
}
```

- [ ] **Step 3: Build and verify**

```bash
just dev
```

- [ ] **Step 4: Commit**

```bash
git add qml/qml/Main.qml
git commit -m "feat(ui): add auth gate to Main.qml — login required before app access"
```

---

### Task 6: CMakeLists.txt Registration

**Files:**
- Modify: `qml/CMakeLists.txt`

- [ ] **Step 1: Add new source files**

Add `authservice.cpp` to `BACKEND_SOURCES` (around line 120):
```cmake
src/services/authservice.cpp
```

Add `authservice.h` to `BACKEND_HEADERS` (around line 154):
```cmake
src/services/authservice.h
```

Add `LoginScreen.qml` to `QML_FILES` under `# Screens` (around line 197):
```cmake
qml/screens/LoginScreen.qml
```

- [ ] **Step 2: Build full project**

```bash
just dev
```

- [ ] **Step 3: Commit**

```bash
git add qml/CMakeLists.txt
git commit -m "build: register AuthService and LoginScreen in CMakeLists"
```

---

## Summary

| Task | Component | Files |
|------|-----------|-------|
| 1 | AuthService header | authservice.h |
| 2 | AuthService implementation | authservice.cpp |
| 3 | Protocol handler + IPC | main.cpp |
| 4 | Login screen UI | LoginScreen.qml |
| 5 | Main.qml auth gate | Main.qml |
| 6 | Build registration | CMakeLists.txt |

**Build:** `just dev` (needs vcpkg for CredentialStore in core)

**Testing:** Manual — launch app, verify login screen appears, click "Giriş Yap", complete web flow, verify return to app.

**Out of scope (separate plans):**
- Backend: `POST /auth/token-exchange` endpoint (cedra-auth, FastAPI)
- Web: `hesap` page `?launcher=true` redirect support (Makine-WEB, Astro)
