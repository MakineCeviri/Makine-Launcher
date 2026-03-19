#pragma once

/**
 * profiler.h - Tracy profiler + PerfReporter integration wrapper.
 *
 * Three modes:
 *   1. TRACY_ENABLE defined: Tracy zones + PerfReporter recording (full profiling)
 *   2. MAKINE_PERF_REPORT defined: PerfReporter only (lightweight, no Tracy)
 *   3. Neither: all macros compile to nothing (zero overhead)
 *
 * PerfReporter is always active when any profiling is enabled.
 * It writes a JSON report on app exit that Claude Code can read.
 *
 * Build with: cmake --preset dev-profile (enables both Tracy + PerfReporter)
 *
 * Usage:
 *   #include "profiler.h"
 *   void myFunction() {
 *       MAKINE_ZONE_NAMED("MyFunction");  // Named zone (reported + Tracy)
 *       MAKINE_FRAME;                     // Frame marker (Tracy only)
 *       MAKINE_THREAD_NAME("Worker");     // Thread naming (both)
 *   }
 */

// PerfReporter is active when Tracy OR standalone perf mode is enabled
#if defined(TRACY_ENABLE) || defined(MAKINE_PERF_REPORT)
#define MAKINE_PERF_ACTIVE
#endif

#ifdef MAKINE_PERF_ACTIVE
#include "perfreporter.h"
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

// --- Helper: unique variable name per line ---
#define MAKINE_CONCAT_(a, b) a##b
#define MAKINE_CONCAT(a, b) MAKINE_CONCAT_(a, b)

// ============================================================================
// MAKINE_ZONE_NAMED(n) — Named zone with timing (reported in perf JSON)
// ============================================================================
#if defined(TRACY_ENABLE) && defined(MAKINE_PERF_ACTIVE)
    // Tracy + PerfReporter
    #define MAKINE_ZONE_NAMED(n) \
        ZoneScopedN(n); \
        makine::PerfZone MAKINE_CONCAT(_makine_pz_, __LINE__)(n)
#elif defined(MAKINE_PERF_ACTIVE)
    // PerfReporter only
    #define MAKINE_ZONE_NAMED(n) \
        makine::PerfZone MAKINE_CONCAT(_makine_pz_, __LINE__)(n)
#else
    #define MAKINE_ZONE_NAMED(n)
#endif

// ============================================================================
// MAKINE_ZONE — Auto-named zone (Tracy only, not reported in perf JSON)
// ============================================================================
#ifdef TRACY_ENABLE
    #define MAKINE_ZONE ZoneScoped
#else
    #define MAKINE_ZONE
#endif

// ============================================================================
// MAKINE_ZONE_COLOR(c) — Colored zone (Tracy only)
// ============================================================================
#ifdef TRACY_ENABLE
    #define MAKINE_ZONE_COLOR(c) ZoneScopedC(c)
#else
    #define MAKINE_ZONE_COLOR(c)
#endif

// ============================================================================
// MAKINE_FRAME — Frame boundary marker (Tracy only)
// ============================================================================
#ifdef TRACY_ENABLE
    #define MAKINE_FRAME FrameMark
#else
    #define MAKINE_FRAME
#endif

// ============================================================================
// MAKINE_MESSAGE(msg) — Text annotation (Tracy only)
// ============================================================================
#ifdef TRACY_ENABLE
    #define MAKINE_MESSAGE(msg) TracyMessageL(msg)
#else
    #define MAKINE_MESSAGE(msg)
#endif

// ============================================================================
// MAKINE_THREAD_NAME(n) — Thread naming (both Tracy + PerfReporter)
// ============================================================================
#if defined(TRACY_ENABLE) && defined(MAKINE_PERF_ACTIVE)
    #define MAKINE_THREAD_NAME(n) \
        tracy::SetThreadName(n); \
        makine::PerfReporter::instance().registerThread(n)
#elif defined(MAKINE_PERF_ACTIVE)
    #define MAKINE_THREAD_NAME(n) \
        makine::PerfReporter::instance().registerThread(n)
#else
    #define MAKINE_THREAD_NAME(n)
#endif
