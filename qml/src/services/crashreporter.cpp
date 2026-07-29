// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file crashreporter.cpp
 * @brief Sentry crash reporting implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "crashreporter.h"

#include <QString>
#include <QStringList>

#ifdef MAKINE_HAS_SENTRY
#include <sentry.h>
#include <QSysInfo>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <algorithm>   // std::min — not pulled in implicitly on MinGW
#include <string>

namespace {

// Previous Qt message handler (for chaining)
static QtMessageHandler s_previousHandler = nullptr;

// Qt message handler that routes to Sentry
void sentryMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    // Always chain to previous handler first
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);

    QByteArray utf8 = msg.toUtf8();
    const char* category = ctx.category ? ctx.category : "qt";

    switch (type) {
    case QtDebugMsg:
        // Skip debug messages — too noisy for Sentry
        break;
    case QtInfoMsg:
        // Skip info messages
        break;
    case QtWarningMsg:
        makine::CrashReporter::addBreadcrumb(category, utf8.constData(), "warning");
        break;
    case QtCriticalMsg:
        makine::CrashReporter::addBreadcrumb(category, utf8.constData(), "error");
        makine::CrashReporter::captureMessage(utf8.constData(), "error");
        break;
    case QtFatalMsg:
        makine::CrashReporter::captureMessage(utf8.constData(), "fatal");
        break;
    }
}

// Strip Windows username from file paths (SEC-14: PII stripping)
// Replaces EVERY occurrence, not just the first: stack frame values hold a
// single path, but captured messages (install/uninstall failures) can quote
// several — one partially-sanitized message would still leak the username.
static std::string sanitizePath(const char* raw) {
    if (!raw) return {};
    std::string path(raw);
    static const std::string kRedacted = "[redacted]";
    // Replace C:\Users\<username>\ with C:\Users\[redacted]\ (both slash styles)
    for (const auto& sep : {std::string("Users\\"), std::string("Users/")}) {
        std::string::size_type pos = 0;
        while ((pos = path.find(sep, pos)) != std::string::npos) {
            const auto nameStart = pos + sep.size();
            // Look for either separator, not just the one that opened the
            // match: paths reach us mixed ("C:\Users\Ahmet/AppData/…") and
            // searching only for the opening style would find nothing.
            const auto nameEnd = std::min(path.find('\\', nameStart),
                                          path.find('/', nameStart));
            // A message can end at the user name — "klasör: C:\Users\Ahmet"
            // carries no trailing separator, and bailing out here left the
            // name in place. Redact to the end of the string in that case.
            const auto nameLen = (nameEnd == std::string::npos)
                                     ? path.size() - nameStart
                                     : nameEnd - nameStart;
            if (nameLen == 0)
                break;
            if (path.compare(nameStart, nameLen, kRedacted) != 0)
                path.replace(nameStart, nameLen, kRedacted);
            pos = nameStart + kRedacted.size();
        }
    }
    return path;
}

// Sanitize stack frame values containing file paths
static void sanitizeFrame(sentry_value_t frame) {
    static const char* pathKeys[] = {"filename", "abs_path", "module", "package", nullptr};
    for (const char** key = pathKeys; *key; ++key) {
        sentry_value_t v = sentry_value_get_by_key(frame, *key);
        if (!sentry_value_is_null(v) &&
            sentry_value_get_type(v) == SENTRY_VALUE_TYPE_STRING) {
            auto sanitized = sanitizePath(sentry_value_as_string(v));
            if (!sanitized.empty()) {
                sentry_value_set_by_key(frame, *key,
                    sentry_value_new_string(sanitized.c_str()));
            }
        }
    }
}

// beforeSend callback — strip PII, add context (SEC-14)
sentry_value_t beforeSend(sentry_value_t event, void* /*hint*/, void* /*closure*/)
{
    // Strip file paths from stack frames to remove Windows usernames
    sentry_value_t exception = sentry_value_get_by_key(event, "exception");
    if (!sentry_value_is_null(exception)) {
        sentry_value_t values = sentry_value_get_by_key(exception, "values");
        auto len = sentry_value_get_length(values);
        for (size_t i = 0; i < len; ++i) {
            sentry_value_t exc = sentry_value_get_by_index(values, i);
            sentry_value_t stacktrace = sentry_value_get_by_key(exc, "stacktrace");
            sentry_value_t frames = sentry_value_get_by_key(stacktrace, "frames");
            auto frameLen = sentry_value_get_length(frames);
            for (size_t j = 0; j < frameLen; ++j) {
                sanitizeFrame(sentry_value_get_by_index(frames, j));
            }
        }
    }

    // Add app version tag
    sentry_value_t tags = sentry_value_get_by_key(event, "tags");
    if (sentry_value_is_null(tags)) {
        tags = sentry_value_new_object();
        sentry_value_set_by_key(event, "tags", tags);
    }

    return event;
}

} // namespace
#endif // MAKINE_HAS_SENTRY

namespace makine {

void CrashReporter::initialize()
{
#ifdef MAKINE_HAS_SENTRY
    sentry_options_t* options = sentry_options_new();

    // DSN from CMake compile definition (SENTRY_DSN)
#ifdef MAKINE_DEV_TOOLS
    // Dev: allow env var override
    QByteArray envDsn = qgetenv("MAKINE_SENTRY_DSN");
    const char* dsn = envDsn.isEmpty() ? SENTRY_DSN : envDsn.constData();
#else
    const char* dsn = SENTRY_DSN;
#endif
    sentry_options_set_dsn(options, dsn);

    // Release tag for version tracking (matches deploy.py Sentry release name)
    sentry_options_set_release(options, MAKINE_SENTRY_RELEASE);

    // Environment
#ifdef NDEBUG
    sentry_options_set_environment(options, "production");
#else
    sentry_options_set_environment(options, "development");
#endif

    // Database path for breadcrumbs, envelope queue (AppData, not exe dir)
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                     + QStringLiteral("/sentry-db");
    QDir().mkpath(dbPath);
    QByteArray dbPathUtf8 = dbPath.toUtf8();
    sentry_options_set_database_path(options, dbPathUtf8.constData());

    // Crashpad handler path (only needed for crashpad backend, harmless for breakpad)
    QString handlerPath = QCoreApplication::applicationDirPath()
                          + QStringLiteral("/crashpad_handler.exe");
    if (QFile::exists(handlerPath)) {
        QByteArray handlerUtf8 = handlerPath.toUtf8();
        sentry_options_set_handler_path(options, handlerUtf8.constData());
    }

    // beforeSend callback
    sentry_options_set_before_send(options, beforeSend, nullptr);

    // Max breadcrumbs. Live events arrive with 41-45 crumbs, so 50 was about
    // to start dropping the oldest ones — and the oldest ones are the startup
    // phases, which is exactly the context a crash report needs.
    sentry_options_set_max_breadcrumbs(options, 100);

    int result = sentry_init(options);
    if (result != 0) {
        qWarning("CrashReporter: sentry_init failed with code %d", result);
        return;
    }

    // OS context
    setContext("os.name", QSysInfo::productType());
    setContext("os.version", QSysInfo::productVersion());
    setContext("os.build", QSysInfo::kernelVersion());
    setContext("arch", QSysInfo::currentCpuArchitecture());

    // Anonymous user ID (SHA-256 of machine unique ID)
    QByteArray machineId = QSysInfo::machineUniqueId();
    if (!machineId.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(machineId, QCryptographicHash::Sha256).toHex();
        setUser(QString::fromLatin1(hash.left(16)));
    }

    addBreadcrumb("app", "Sentry initialized", "info");
#endif
}

void CrashReporter::shutdown()
{
#ifdef MAKINE_HAS_SENTRY
    addBreadcrumb("app", "Application shutting down", "info");
    sentry_close();
#endif
}

void CrashReporter::addBreadcrumb(const char* category, const char* message,
                                   const char* level)
{
#ifdef MAKINE_HAS_SENTRY
    // Breadcrumbs are attached to every event, and the qCWarning lines routed
    // here quote absolute paths — live events carried "C:/Users/<name>/AppData/…"
    // through untouched while the message body next to them was correctly
    // redacted. beforeSend only walks exception stack frames and never sees
    // breadcrumbs, so this is the only place the redaction can happen.
    const std::string safe = sanitizePath(message);
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", safe.c_str());
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string(category));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(level));
    sentry_add_breadcrumb(crumb);
#else
    Q_UNUSED(category)
    Q_UNUSED(message)
    Q_UNUSED(level)
#endif
}

void CrashReporter::setContext(const char* key, const QString& value)
{
#ifdef MAKINE_HAS_SENTRY
    sentry_set_tag(key, value.toUtf8().constData());
#else
    Q_UNUSED(key)
    Q_UNUSED(value)
#endif
}

void CrashReporter::setUser(const QString& id)
{
#ifdef MAKINE_HAS_SENTRY
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(id.toUtf8().constData()));
    sentry_set_user(user);
#else
    Q_UNUSED(id)
#endif
}

void CrashReporter::captureMessage(const char* message, const char* level)
{
#ifdef MAKINE_HAS_SENTRY
    sentry_level_t sentryLevel = SENTRY_LEVEL_INFO;
    if (qstrcmp(level, "warning") == 0)      sentryLevel = SENTRY_LEVEL_WARNING;
    else if (qstrcmp(level, "error") == 0)    sentryLevel = SENTRY_LEVEL_ERROR;
    else if (qstrcmp(level, "fatal") == 0)    sentryLevel = SENTRY_LEVEL_FATAL;
    else if (qstrcmp(level, "debug") == 0)    sentryLevel = SENTRY_LEVEL_DEBUG;

    // Captured messages routinely quote filesystem paths (install/uninstall
    // failures surface the extracted package folder), so they need the same
    // PII stripping the stack frames get in beforeSend — that hook only walks
    // exception frames and never touches the message body.
    const std::string safe = sanitizePath(message);
    sentry_capture_event(sentry_value_new_message_event(sentryLevel, "makine", safe.c_str()));
#else
    Q_UNUSED(message)
    Q_UNUSED(level)
#endif
}

bool CrashReporter::isUserActionable(const QString& message)
{
    // Conditions the user resolves on their own machine. Classified from the
    // message because the same failure surfaces through several code paths, and
    // the wording is what actually distinguishes "your disk is full" from "our
    // package is broken". Turkish keywords match the user-facing strings; the
    // English ones cover messages coming up from the core layer.
    static const QStringList kUserPatterns = {
        // disk / space
        QStringLiteral("disk"), QStringLiteral("yer açın"), QStringLiteral("yetersiz"),
        QStringLiteral("boş alan"), QStringLiteral("space"),
        // permissions / locks
        QStringLiteral("izin"), QStringLiteral("yönetici"), QStringLiteral("erişim"),
        QStringLiteral("salt okunur"), QStringLiteral("permission"), QStringLiteral("denied"),
        // game or store busy
        QStringLiteral("çalışıyor"), QStringLiteral("kapatın"), QStringLiteral("kilitli"),
        // connectivity
        QStringLiteral("internet"), QStringLiteral("bağlantı"), QStringLiteral("ağ "),
        QStringLiteral("network"), QStringLiteral("timeout"), QStringLiteral("zaman aşımı"),
        // user-initiated
        QStringLiteral("iptal"), QStringLiteral("cancel"),
    };
    // Classify on the failure text only. Install failures append a remedy
    // paragraph — "Çözüm: oyunu ve Steam'i kapatın, … yönetici olarak
    // çalıştırın, antivirüste … izinli yapın" — whose wording matches the
    // patterns above. Every message carrying that paragraph was therefore
    // filed as `failure.side:user` no matter what actually broke, and that is
    // the exact tag the "Widespread Failure" alert filters on: a real defect
    // hitting many users was classified as their environment and never fired.
    // ELDEN RING's failure (a mangled Turkish path in our own code) arrived
    // tagged `user` for this reason.
    //
    // left() returns the whole string when indexOf() finds nothing, so
    // messages without the paragraph are classified exactly as before.
    const QString cause = message.left(message.indexOf(QStringLiteral("\n\nÇözüm:")));

    for (const QString& p : kUserPatterns) {
        if (cause.contains(p, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool CrashReporter::isUnsupportedCapability(const QString& message)
{
    // The launcher refuses, by design, to overlay-copy a payload whose install
    // method it cannot actually perform — that silent "success" was the
    // "yama kuruldu ama oyun İngilizce" bug. The refusal is correct behaviour,
    // so it must not be filed next to genuine defects.
    //
    // Matched on the guidance wording emitted by LocalPackageManager's
    // no-handler branches (see installPackage / installWithOptions). Keep in
    // sync with those strings; test_crash_reporter.cpp pins them.
    static const QStringList kUnsupportedPatterns = {
        QStringLiteral("otomatik kurulamıyor"),   // external/script/forge/generic
        QStringLiteral("otomatik kuramıyor"),     // unityPatch wording
        QStringLiteral("Paradox modudur"),
        QStringLiteral("Steam Workshop üzerinden"),
        QStringLiteral("kendi kurulum sihirbazıyla"),
        QStringLiteral("desteklemediği bir kurulum adımı"),
    };

    for (const QString& p : kUnsupportedPatterns) {
        if (message.contains(p, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void CrashReporter::reportFailure(const char* operation, const QString& subject,
                                   const QString& message)
{
    // Order matters: a capability gap is checked first because its guidance
    // text ("… kurulum aracını çalıştırın", "yönetici izni isterse …") overlaps
    // the user-actionable keywords and would otherwise be filed as `user`.
    const bool unsupported = isUnsupportedCapability(message);
    const bool userSide    = !unsupported && isUserActionable(message);

    const QString side = unsupported ? QStringLiteral("unsupported")
                       : userSide    ? QStringLiteral("user")
                                     : QStringLiteral("system");
    const char* level  = unsupported ? "info"
                       : userSide    ? "warning"
                                     : "error";

    // Tag before capturing so the event carries them: "which operation fails
    // most" and "which game fails most" are the two questions this exists to
    // answer without asking users for logs.
    setContext("operation", QString::fromLatin1(operation));
    if (!subject.isEmpty())
        setContext("subject", subject);
    setContext("failure.side", side);

    // Kept as an event rather than dropped: the per-game counts are how we rank
    // which install handler to write next (sentry_triage.py reads them as
    // "Eksik handler talebi"). Only the severity changes.
    const QByteArray payload =
        QStringLiteral("%1 failed [%2]: %3")
            .arg(QString::fromLatin1(operation),
                 subject.isEmpty() ? QStringLiteral("-") : subject,
                 message)
            .toUtf8();
    captureMessage(payload.constData(), level);
}

void CrashReporter::setGameContext(const QString& gameId, const QString& gameName)
{
#ifdef MAKINE_HAS_SENTRY
    setContext("game.id", gameId);
    setContext("game.name", gameName);

    QByteArray msg = QStringLiteral("Game context: %1 (%2)").arg(gameName, gameId).toUtf8();
    addBreadcrumb("game", msg.constData(), "info");
#else
    Q_UNUSED(gameId)
    Q_UNUSED(gameName)
#endif
}

void CrashReporter::installQtMessageHandler()
{
#ifdef MAKINE_HAS_SENTRY
    s_previousHandler = qInstallMessageHandler(sentryMessageHandler);
#endif
}

} // namespace makine
