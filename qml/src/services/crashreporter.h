// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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

    /**
     * @brief Report an operation failure as a tagged Sentry event.
     *
     * The launcher emits ~70 distinct failure messages across download,
     * extract, install, backup and sync, but they only ever reached the local
     * log. Every user report ("yama kurulamadı") therefore arrived without the
     * message the user actually saw, making remote diagnosis impossible.
     *
     * Severity is derived from the message, not the call site, across three
     * classes tagged as `failure.side`:
     *   - `unsupported` (info)    — the patch needs an install capability the
     *                               launcher does not implement. Nothing broke;
     *                               these are a demand ranking for what to build.
     *   - `user` (warning)        — the user can fix it on their own machine
     *                               (disk full, game running, permission, network).
     *   - `system` (error)        — everything else, i.e. our defects.
     *
     * Without the `unsupported` split those events landed as errors and were
     * 72% of the project: real defects stopped standing out, and the
     * "Widespread Failure" alert fired on patches that were never installable
     * by us in the first place.
     *
     * @param operation Short slug used as a tag: "download", "install",
     *                  "uninstall", "backup", "restore", "sync", "scan".
     * @param subject   What the failure is about — appId/gameId when known.
     * @param message   User-facing text; paths are redacted by captureMessage().
     */
    static void reportFailure(const char* operation, const QString& subject,
                              const QString& message);

    /// True when `message` describes something the user can resolve on their own.
    /// Exposed for callers that want to branch on it (e.g. skip a retry).
    static bool isUserActionable(const QString& message);

    /// True when `message` says the launcher cannot install this patch kind at
    /// all (forge injection, script recipe, Paradox mod, Workshop, bundled
    /// installer…). Such a run is a capability gap, not a failure — callers use
    /// this to avoid presenting it as an error the user should retry.
    static bool isUnsupportedCapability(const QString& message);

    /// Install Qt message handler that routes qWarning/qCritical/qFatal to Sentry
    static void installQtMessageHandler();
};

} // namespace makine
