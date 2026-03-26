#pragma once

/**
 * @file crashreporter.h
 * @brief Sentry crash reporting integration
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Wraps sentry-native SDK for automatic crash reporting.
 * All methods are static and no-op when built without MAKINE_HAS_SENTRY.
 */

#include <QString>

namespace makine {

class CrashReporter
{
public:
    /// Initialize Sentry SDK. Call as early as possible, before QGuiApplication.
    static void initialize();

    /// Flush and shutdown Sentry SDK. Call at application exit.
    static void shutdown();

    /// Add a breadcrumb for diagnostic context
    static void addBreadcrumb(const char* category, const char* message,
                              const char* level = "info");

    /// Set a key-value tag on the Sentry scope
    static void setContext(const char* key, const QString& value);

    /// Set anonymous user identifier (SHA-256 of machine ID)
    static void setUser(const QString& id);

    /// Capture a message event (info, warning, error)
    static void captureMessage(const char* message, const char* level = "info");

    /// Set game context for current operation (breadcrumb + tags)
    static void setGameContext(const QString& gameId, const QString& gameName);

    /// Install Qt message handler that routes qWarning/qCritical/qFatal to Sentry
    static void installQtMessageHandler();
};

} // namespace makine
