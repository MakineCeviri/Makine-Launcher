/**
 * @file crashreporter.cpp
 * @brief Sentry crash reporting implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "crashreporter.h"

#ifdef MAKINEAI_HAS_SENTRY
#include <sentry.h>
#include <QSysInfo>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

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
        makineai::CrashReporter::addBreadcrumb(category, utf8.constData(), "warning");
        break;
    case QtCriticalMsg:
        makineai::CrashReporter::addBreadcrumb(category, utf8.constData(), "error");
        makineai::CrashReporter::captureMessage(utf8.constData(), "error");
        break;
    case QtFatalMsg:
        makineai::CrashReporter::captureMessage(utf8.constData(), "fatal");
        break;
    }
}

// beforeSend callback — strip PII, add context
sentry_value_t beforeSend(sentry_value_t event, void* /*hint*/, void* /*closure*/)
{
    // Add app version tag
    sentry_value_t tags = sentry_value_get_by_key(event, "tags");
    if (sentry_value_is_null(tags)) {
        tags = sentry_value_new_object();
        sentry_value_set_by_key(event, "tags", tags);
    }

    return event;
}

} // namespace
#endif // MAKINEAI_HAS_SENTRY

namespace makineai {

void CrashReporter::initialize()
{
#ifdef MAKINEAI_HAS_SENTRY
    sentry_options_t* options = sentry_options_new();

    // DSN from CMake compile definition (SENTRY_DSN)
#ifdef MAKINEAI_DEV_TOOLS
    // Dev: allow env var override
    QByteArray envDsn = qgetenv("MAKINEAI_SENTRY_DSN");
    const char* dsn = envDsn.isEmpty() ? SENTRY_DSN : envDsn.constData();
#else
    const char* dsn = SENTRY_DSN;
#endif
    sentry_options_set_dsn(options, dsn);

    // Release tag for version tracking (matches deploy.py Sentry release name)
    sentry_options_set_release(options, MAKINEAI_SENTRY_RELEASE);

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

    // Max breadcrumbs
    sentry_options_set_max_breadcrumbs(options, 50);

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
#ifdef MAKINEAI_HAS_SENTRY
    addBreadcrumb("app", "Application shutting down", "info");
    sentry_close();
#endif
}

void CrashReporter::addBreadcrumb(const char* category, const char* message,
                                   const char* level)
{
#ifdef MAKINEAI_HAS_SENTRY
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", message);
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
#ifdef MAKINEAI_HAS_SENTRY
    sentry_set_tag(key, value.toUtf8().constData());
#else
    Q_UNUSED(key)
    Q_UNUSED(value)
#endif
}

void CrashReporter::setUser(const QString& id)
{
#ifdef MAKINEAI_HAS_SENTRY
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(id.toUtf8().constData()));
    sentry_set_user(user);
#else
    Q_UNUSED(id)
#endif
}

void CrashReporter::captureMessage(const char* message, const char* level)
{
#ifdef MAKINEAI_HAS_SENTRY
    sentry_level_t sentryLevel = SENTRY_LEVEL_INFO;
    if (qstrcmp(level, "warning") == 0)      sentryLevel = SENTRY_LEVEL_WARNING;
    else if (qstrcmp(level, "error") == 0)    sentryLevel = SENTRY_LEVEL_ERROR;
    else if (qstrcmp(level, "fatal") == 0)    sentryLevel = SENTRY_LEVEL_FATAL;
    else if (qstrcmp(level, "debug") == 0)    sentryLevel = SENTRY_LEVEL_DEBUG;

    sentry_capture_event(sentry_value_new_message_event(sentryLevel, "makineai", message));
#else
    Q_UNUSED(message)
    Q_UNUSED(level)
#endif
}

void CrashReporter::setGameContext(const QString& gameId, const QString& gameName)
{
#ifdef MAKINEAI_HAS_SENTRY
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
#ifdef MAKINEAI_HAS_SENTRY
    s_previousHandler = qInstallMessageHandler(sentryMessageHandler);
#endif
}

} // namespace makineai
