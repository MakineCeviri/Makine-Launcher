# Launcher Auth Gate — Design Spec

## Goal

Require users to authenticate via makineceviri.org before using the launcher. All content remains free — authentication exists solely for security and community protection.

## Architecture

OAuth-style redirect flow using a custom protocol handler (`makine://`). No in-app registration — web handles all account management.

**Stack:** Qt6/C++ (launcher), Cloudflare Worker + FastAPI (existing backend)

## Auth Flow (PKCE Authorization Code)

Tokens are NEVER passed in URLs. Instead, a one-time authorization code is exchanged for tokens over HTTPS.

```
App starts
  → Stored refresh token exists?
    → Yes → POST /auth/refresh
      → 200 → Store new tokens → Main screen
      → 401 → Clear tokens → Login screen
    → No → Login screen

Login screen:
  → User clicks "Giriş Yap"
  → Launcher generates:
    - code_verifier (random 43-128 char string)
    - code_challenge = SHA256(code_verifier), base64url-encoded
    - state = random 32-byte hex nonce
  → Opens browser: makineceviri.org/hesap?launcher=true&code_challenge=X&state=Y
  → User logs in on web (existing flow)
  → Web generates one-time authorization code, stores with code_challenge
  → Web redirects: makine://auth/callback?code=AUTH_CODE&state=Y
  → Launcher verifies state matches, then exchanges code for tokens:
    POST /auth/token-exchange
    { "code": AUTH_CODE, "code_verifier": VERIFIER }
  → Backend verifies SHA256(code_verifier) == stored code_challenge
  → Backend returns { access_token, refresh_token } over HTTPS
  → Launcher stores refresh token in Windows Credential Manager
  → Navigates to main screen

Logout:
  → POST /auth/logout (invalidate server session)
  → Clear Windows Credential Manager entry
  → Return to login screen
```

**Why PKCE?** Even if a malicious app hijacks `makine://`, it only gets a one-time code. Without the `code_verifier` (which never leaves the launcher process), it cannot exchange the code for tokens.

## Components

### 1. Custom Protocol Handler (`makine://`)

Windows registry entry that maps `makine://` URLs to the launcher executable.

**Registration:** On first run or install, write to `HKCU\Software\Classes\makine`:
```
makine\shell\open\command = "C:\...\Makine-Launcher.exe" "--auth-callback" "%1"
```

**Handling:** When launched with `--auth-callback makine://auth/callback?code=X&state=Y`, extract code and state from URL, pass to AuthService for token exchange.

**Single instance:** If launcher is already running, the new instance sends the auth code via `QLocalServer` IPC (with `UserAccessOption` for process isolation) and exits. The running instance receives the code and performs the token exchange.

**Timeout:** If no callback received within 2 minutes after opening browser, show "Zaman aşımı — tekrar deneyin" with retry button.

### 2. AuthService (C++ backend service)

New file: `qml/src/services/authservice.h` / `.cpp`

**Responsibilities:**
- Token lifecycle: store, retrieve, refresh, clear
- HTTP calls: `/auth/me`, `/auth/refresh`, `/auth/logout`
- Auth state: `Authenticated`, `Unauthenticated`, `Checking`
- User info cache: email (masked), user ID

**PKCE state:**
- `code_verifier` and `state` kept in memory during login flow
- Cleared after token exchange or timeout (2 min)

**Token storage — Windows Credential Manager:**
- Reuse existing `CredentialStore` class (if available) or Win32 `CredWriteW` / `CredReadW` / `CredDeleteW`
- Target name: `MakineLauncher/RefreshToken`
- Access token kept in memory only (never persisted)
- No tokens written to files, registry, or QSettings

**Auto-refresh:** Timer-based, refresh access token every 12 minutes (token expires at 15 min). On failure, transition to `Unauthenticated`.

**Auth states:**
- `Checking` — startup, verifying stored token
- `Unauthenticated` — no valid token, show login screen
- `WaitingForBrowser` — browser opened, waiting for callback
- `Exchanging` — received code, exchanging for tokens
- `Authenticated` — valid token, app usable
- `Refreshing` — auto-refreshing access token (background, stays authenticated)

**Properties (QML-accessible):**
```cpp
Q_PROPERTY(AuthState state READ state NOTIFY stateChanged)
Q_PROPERTY(QString userEmail READ userEmail NOTIFY userChanged)
Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY stateChanged)
```

### 3. Login Gate (QML)

Modify `Main.qml` startup flow:

```
ApplicationWindow
  → AuthService.state == Checking → Splash/loading
  → AuthService.state == Unauthenticated → LoginScreen
  → AuthService.state == Authenticated → Normal app (HomeScreen)
```

**LoginScreen.qml:** Minimal screen with:
- Makine logo
- "Devam etmek için giriş yapın" message
- "Giriş Yap" button → opens browser
- "Hesabınız yok mu? Kayıt olun" link → opens browser to register page
- Waiting indicator after browser opens ("Tarayıcıdan giriş bekleniyor...")

### 4. Web Changes (makineceviri.org)

Modify `hesap` page to detect `?launcher=true&code_challenge=X&state=Y`:

- Store `code_challenge` and `state` in session
- After successful login:
  - Generate one-time authorization code (random, 64 chars)
  - Store in DB/KV: `{ code, code_challenge, user_id, expires: 60s }`
  - Redirect to: `makine://auth/callback?code=AUTH_CODE&state=Y`
  - Show fallback: "Launcher'a yönlendiriliyorsunuz... Açılmadıysa [tekrar deneyin]"

### 5. Backend: Token Exchange Endpoint

New endpoint on cedra-auth:

```
POST /auth/token-exchange
{ "code": "AUTH_CODE", "code_verifier": "VERIFIER" }

Backend:
  1. Look up code in DB/KV
  2. Verify SHA256(code_verifier) == stored code_challenge
  3. Verify code not expired (60s) and not already used
  4. Delete code (one-time use)
  5. Create session + return { access_token, refresh_token }
  6. Rate limit: 5 req/min per IP
```

## Data Privacy

### What We Store

| Data | Location | Lifetime |
|------|----------|----------|
| Refresh token | Windows Credential Manager (encrypted by OS) | Until logout or 30-day expiry |
| Access token | Process memory only | Until app closes or 15-min expiry |
| User email (masked) | Process memory only | Until app closes |

### What We Never Do

- Never write tokens to files, logs, QSettings, or registry values
- Never log user email, tokens, or credentials (even in debug mode)
- Never send tokens to third parties
- Never store passwords (web handles authentication)
- Never cache user data between sessions (re-fetch on each launch)

### Security Measures

- All HTTP calls use HTTPS only
- Access token sent as `Authorization: Bearer` header
- Refresh token rotation on every refresh (old token invalidated)
- Rate limiting enforced by Cloudflare Worker (existing)
- Account lockout after 5 failed attempts (existing backend feature)
- Custom protocol callback validates token format before processing
- IPC for single-instance token delivery uses `QLocalServer` with access control

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `qml/src/services/authservice.h` | Create | Auth state, token management, API |
| `qml/src/services/authservice.cpp` | Create | Implementation |
| `qml/qml/screens/LoginScreen.qml` | Create | Login gate UI |
| `qml/qml/Main.qml` | Modify | Auth gate before main content |
| `qml/src/main.cpp` | Modify | Protocol handler, single instance IPC |
| `qml/CMakeLists.txt` | Modify | Add new files |

Web side (separate repo):
| File | Action | Purpose |
|------|--------|---------|
| `src/pages/hesap.astro` | Modify | `?launcher=true` redirect support |

## Error Handling

| Scenario | Behavior |
|----------|----------|
| No internet | Show "Bağlantı kurulamadı" + retry button |
| Token expired + refresh fails | Return to login screen |
| Backend down | Show "Sunucu bakımda" + retry with backoff |
| Invalid callback URL | Ignore, stay on login screen |
| State mismatch (CSRF attempt) | Ignore callback, log warning, stay on login screen |
| Code exchange fails | Show "Doğrulama başarısız" + retry button |
| Browser doesn't open | Show manual URL to copy |
| Callback timeout (2 min) | Show "Zaman aşımı" + retry button |
| Protocol handler not registered | Re-register on app start, show error if fails |
| Protocol hijacking attempt | PKCE prevents token theft — code useless without verifier |

## Out of Scope

- In-app registration (web only)
- Premium/tier system (all free)
- Social login (Discord, Google — future consideration)
- Offline mode (requires auth on every launch)
- Account management in launcher (web only)
