// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file crashreporter.cpp
 * @brief Sentry crash reporting implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "crashreporter.h"

#include "pathredaction.h"

#include "failurereasons.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <appmodel.h>
#endif

#include <QString>
#include <QStringList>

#include "telemetryrules.h"

#include <QDateTime>
#include <QMutex>
#include <QSettings>
#include <QVariantMap>

#include <atomic>

static makine::CrashReporter::FailureSink s_failureSink = nullptr;

// The user's "Çökme ve Hata Raporlarını Gönder" setting. False until
// initialize() has brought Sentry up, so a session that opted out never even
// builds an event. Read on whichever thread reports, written on the GUI thread.
static std::atomic<bool> s_enabled{false};

// Sentry gate — see telemetryrules.h for why it exists. reportFailure runs on
// the scan worker thread too and the Qt handler on whichever thread logged, so
// the state is locked; recursive in case QSettings itself logs a critical.
static QRecursiveMutex s_gateMutex;
static int s_sessionEvents = 0;

static bool admitToSentry(const QString& key)
{
    // Checked before the quota: an event that is never sent must not use up
    // the day's slot for its key, or it stays suppressed once reports are back.
    if (!s_enabled)
        return false;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker lock(&s_gateMutex);
    if (s_sessionEvents >= makine::telemetryrules::kSentryEventsPerSession)
        return false;

    QSettings settings;
    QVariantMap sent = settings.value(QStringLiteral("telemetry/sentrySent")).toMap();
    makine::telemetryrules::pruneSent(sent, now);
    if (!makine::telemetryrules::sentryMaySend(sent.value(key).toLongLong(), now))
        return false;

    sent.insert(key, now);
    settings.setValue(QStringLiteral("telemetry/sentrySent"), sent);
    ++s_sessionEvents;
    return true;
}

#ifdef MAKINE_HAS_SENTRY
#include <sentry.h>
#include <QSysInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <algorithm>   // std::min — not pulled in implicitly on MinGW
#include <string>

namespace {

// Previous Qt message handler (for chaining)
static QtMessageHandler s_previousHandler = nullptr;

// sentry_init() has succeeded in this process. GUI thread only.
static bool s_initialized = false;

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
        if (admitToSentry(QStringLiteral("qt|%1|%2")
                              .arg(QString::fromLatin1(category), msg.left(120))))
            makine::CrashReporter::captureMessage(utf8.constData(), "error");
        break;
    case QtFatalMsg:
        makine::CrashReporter::captureMessage(utf8.constData(), "fatal");
        break;
    }
}

// Strip Windows username from file paths (SEC-14: PII stripping).
// The transform itself lives in pathredaction.h so it can be unit-tested —
// from here it is only reachable by actually sending an event to Sentry.
static std::string sanitizePath(const char* raw) {
    return makine::redaction::redactUserPaths(raw);
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

    // The SDK attaches the process's module list to every event, and a module
    // path is a user path whenever the launcher or an injected DLL lives under
    // the profile: 0.1.6's ZIP run from the Desktop sent
    // "C:\Users\<name>\Desktop\...\Makine-Launcher.exe", MSIX runs sent
    // OneDrive's shell extension from "C:\Users\<name>\AppData\...". Only the
    // directory is redacted; symbolication matches modules by debug/code id.
    //
    // The SDK hands out its cached list frozen (sentry-native 0.9.1,
    // sentry_get_modules_list), where every set_by_key fails silently, so the
    // list is rebuilt from copies of the module finder's keys.
    sentry_value_t debugMeta = sentry_value_get_by_key(event, "debug_meta");
    sentry_value_t images = sentry_value_get_by_key(debugMeta, "images");
    const auto imageLen = sentry_value_get_length(images);
    if (imageLen > 0) {
        sentry_value_t redacted = sentry_value_new_list();
        for (size_t i = 0; i < imageLen; ++i) {
            sentry_value_t image = sentry_value_get_by_index(images, i);
            sentry_value_t copy = sentry_value_new_object();
            for (const char* key : {"type", "image_addr", "image_size", "code_id", "debug_id"}) {
                sentry_value_t v = sentry_value_get_by_key(image, key);
                if (!sentry_value_is_null(v)) {
                    sentry_value_incref(v);
                    sentry_value_set_by_key(copy, key, v);
                }
            }
            for (const char* key : {"code_file", "debug_file"}) {
                sentry_value_t v = sentry_value_get_by_key(image, key);
                if (sentry_value_get_type(v) == SENTRY_VALUE_TYPE_STRING)
                    sentry_value_set_by_key(copy, key, sentry_value_new_string(
                        sanitizePath(sentry_value_as_string(v)).c_str()));
            }
            sentry_value_append(redacted, copy);
        }
        sentry_value_set_by_key(debugMeta, "images", redacted);
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

    // Environment. The dev presets are Release builds (NDEBUG) with DEV_TOOLS,
    // so keying on NDEBUG alone filed every dev run and self-test under
    // "production" — the environment the alert rules and crash-free rates use.
#if defined(NDEBUG) && !defined(MAKINE_DEV_TOOLS)
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

    // Uploads wait for consent, and consent follows the user's setting
    // (setEnabled). That is what makes switching reports off total: the SDK
    // drops every envelope at the transport — messages, the crash the breakpad
    // handler writes, session updates, whatever is still queued — not only
    // the events that pass through our own wrappers.
    //
    // The SDK keeps that consent on disk and reads it inside sentry_init(),
    // the same call that sends the previous run's queued envelopes; a consent
    // given after it is too late for them. On the first start with this
    // setting there is no file yet, so that replay (a crash minidump among it)
    // would be dropped. Writing it first keeps it (sentry-native 0.9.1,
    // load_user_consent). Should the format ever change, only that replay is
    // lost: sentry_user_consent_give() below still covers the session.
    QFile consentFile(dbPath + QStringLiteral("/user-consent"));
    if (consentFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        consentFile.write("1\n");
    consentFile.close();
    sentry_options_set_require_user_consent(options, 1);

    int result = sentry_init(options);
    if (result != 0) {
        qWarning("CrashReporter: sentry_init failed with code %d", result);
        return;
    }
    sentry_user_consent_give();
    s_initialized = true;
    s_enabled = true;

    // OS context
    setContext("os.name", QSysInfo::productType());
    setContext("os.version", QSysInfo::productVersion());
    setContext("os.build", QSysInfo::kernelVersion());
    setContext("arch", QSysInfo::currentCpuArchitecture());

    // Anonymous user ID (SHA-256 of machine unique ID)
    // Shared with the counting channel so both can be joined per install.
    const QString installId = telemetryrules::installId(QSysInfo::machineUniqueId());
    if (!installId.isEmpty()) {
        setUser(installId);

    // Session fact, so a sticky tag is the right shape here (unlike fail.*).
    // MSIX path virtualization was a confirmed root cause class — being able to
    // filter app.packaged:true separates "our installer is wrong" from "the
    // container moved the files".
    //
    // The same check exists in updateservice.cpp (anonymous namespace) and
    // localpackagemanager.cpp; all three read a process-lifetime constant from
    // the OS and none can see the others.
#ifdef Q_OS_WIN
    UINT32 nameLength = 0;
    const bool packaged = ::GetCurrentPackageFullName(&nameLength, nullptr)
                          != APPMODEL_ERROR_NO_PACKAGE;
    setContext("app.packaged", packaged ? QStringLiteral("msix") : QStringLiteral("plain"));
#endif
    }

    addBreadcrumb("app", "Sentry initialized", "info");
#endif
}

void CrashReporter::setEnabled(bool enabled)
{
#ifdef MAKINE_HAS_SENTRY
    // The session started with reports off, so Sentry was never brought up.
    if (enabled && !s_initialized) {
        initialize();
        return;
    }
    if (enabled)
        sentry_user_consent_give();
    else
        sentry_user_consent_revoke();
#endif
    s_enabled = enabled;
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
    if (!s_enabled)
        return;
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

void CrashReporter::captureMessage(const char* message, const char* level,
                                    const QHash<QString, QString>& eventTags)
{
#ifdef MAKINE_HAS_SENTRY
    if (!s_enabled)
        return;
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
    sentry_value_t event = sentry_value_new_message_event(sentryLevel, "makine", safe.c_str());

    // Per-event tags. sentry_set_tag() would be simpler and wrong: it is
    // session-global, so a fail.reason set here would ride along on every
    // later event until something overwrote it.
    if (!eventTags.isEmpty()) {
        sentry_value_t tags = sentry_value_new_object();
        for (auto it = eventTags.cbegin(); it != eventTags.cend(); ++it) {
            if (it.value().isEmpty()) continue;          // an empty tag is noise
            sentry_value_set_by_key(tags, it.key().toUtf8().constData(),
                                    sentry_value_new_string(
                                        sanitizePath(it.value().toUtf8().constData()).c_str()));
        }
        sentry_value_set_by_key(event, "tags", tags);
    }
    sentry_capture_event(event);
#else
    Q_UNUSED(message)
    Q_UNUSED(level)
    Q_UNUSED(eventTags)
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
        // connectivity. "timed out" and "Host … not found" are how Qt words
        // QNetworkReply errors: "Katalog indirilemedi: Operation timed out"
        // reached 11 users in two weeks filed as our defect (system), next to
        // the real ones. An outage of our own CDN is the uptime monitor's job.
        QStringLiteral("internet"), QStringLiteral("bağlantı"), QStringLiteral("ağ "),
        QStringLiteral("network"), QStringLiteral("timeout"), QStringLiteral("zaman aşımı"),
        QStringLiteral("timed out"),
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
    // "Host cdn.makineceviri.org not found": the machine's DNS, not our package.
    if (cause.contains(QStringLiteral("Host ")) && cause.contains(QStringLiteral(" not found")))
        return true;
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

void CrashReporter::setFailureSink(FailureSink sink)
{
    s_failureSink = sink;
}

void CrashReporter::reportFailure(const char* operation, const QString& subject,
                                   const QString& message,
                                   const QHash<QString, QString>& eventTags)
{
    // Order matters: a capability gap is checked first because its guidance
    // text ("… kurulum aracını çalıştırın", "yönetici izni isterse …") overlaps
    // the user-actionable keywords and would otherwise be filed as `user`.
    const bool unsupported = isUnsupportedCapability(message);
    const bool userSide    = !unsupported && isUserActionable(message);

    const QString side = unsupported ? QStringLiteral("unsupported")
                       : userSide    ? QStringLiteral("user")
                                     : QStringLiteral("system");
    const char* level  = userSide    ? "warning"
                                     : "error";
    const QString reason = failurereasons::failureReason(message);

    // Every occurrence is counted; Sentry only gets the ones worth its quota.
    if (s_failureSink)
        s_failureSink(operation, subject, side, reason);

    // Capability gaps were 69% of what Sentry accepted and are fully answered
    // by the counts (which game, how often). Anything else goes out once per
    // (operation, reason, subject) per day — except the self-test, which is
    // the proof that the path works and must never be suppressed.
    // A cancel is the user's choice; the count is all there is to know.
    if (unsupported || reason == QLatin1String("cancelled"))
        return;
    if (qstrcmp(operation, "selftest") != 0
        && !admitToSentry(telemetryrules::sentryKey(QString::fromLatin1(operation),
                                                    reason, subject)))
        return;

    // "Which operation fails most", "which game fails most" and — the
    // dimension the first tags never carried — WHY: without fail.reason a
    // single defect spread over 67 games read as 67 unrelated issues (see
    // failurereasons.h). All of them ride on this event only; see
    // telemetryrules::failureTags for what the session-wide version broke.
    const QHash<QString, QString> tags = telemetryrules::failureTags(
        QString::fromLatin1(operation), subject, side, reason,
        failurereasons::failedStepAction(message), eventTags);

    const QByteArray payload =
        QStringLiteral("%1 failed [%2]: %3")
            .arg(QString::fromLatin1(operation),
                 subject.isEmpty() ? QStringLiteral("-") : subject,
                 message)
            .toUtf8();
    captureMessage(payload.constData(), level, tags);
}

void CrashReporter::setGameContext(const QString& gameId, const QString& gameName)
{
#ifdef MAKINE_HAS_SENTRY
    // A breadcrumb only. As session tags game.id/game.name stayed on every
    // later event — a crash an hour later on another screen still named the
    // last game that failed. Failure events carry their game themselves.
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
