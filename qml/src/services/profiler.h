#pragma once

/**
 * profiler.h - Tracy profiler integration wrapper.
 *
 * When MAKINEAI_PROFILER is enabled at build time, this provides Tracy macros.
 * Otherwise, all macros compile to nothing (zero overhead).
 *
 * Usage:
 *   #include "profiler.h"
 *   void myFunction() {
 *       MAKINE_ZONE;                     // Auto-named zone (function name)
 *       MAKINE_ZONE_NAMED("CustomName"); // Named zone
 *       MAKINE_FRAME;                    // Frame marker (call once per frame)
 *   }
 *
 * Build with: cmake -DMAKINEAI_PROFILER=ON --preset dev
 * Connect: Tracy profiler GUI (https://github.com/wolfpld/tracy/releases)
 */

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>

#define MAKINE_ZONE           ZoneScoped
#define MAKINE_ZONE_NAMED(n)  ZoneScopedN(n)
#define MAKINE_ZONE_COLOR(c)  ZoneScopedC(c)
#define MAKINE_FRAME          FrameMark
#define MAKINE_MESSAGE(msg)   TracyMessageL(msg)

#else

#define MAKINE_ZONE
#define MAKINE_ZONE_NAMED(n)
#define MAKINE_ZONE_COLOR(c)
#define MAKINE_FRAME
#define MAKINE_MESSAGE(msg)

#endif
