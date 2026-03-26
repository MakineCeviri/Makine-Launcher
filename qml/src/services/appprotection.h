/**
 * @file appprotection.h
 * @brief Anti-reverse-engineering and tamper detection API
 * @copyright (c) 2026 MakineCeviri Team
 *
 * All checks are compiled out in debug builds (NDEBUG not defined).
 */

#pragma once

namespace makine::protection {

/// Run all anti-debug / integrity checks once (call before QML load)
void initialize();

/// Start periodic re-checks via QTimer (call after event loop is running)
void schedulePeriodicChecks();

/// True if any check has flagged a compromise
bool isCompromised();

/// React to a detected violation — delayed exit to obscure the trigger site
void onViolation(int site);

} // namespace makine::protection

/// Sprinkle in critical functions — no-op in debug builds
#define INTEGRITY_GATE()                                         \
    do {                                                         \
        if (::makine::protection::isCompromised())             \
            ::makine::protection::onViolation(__LINE__);       \
    } while (0)
