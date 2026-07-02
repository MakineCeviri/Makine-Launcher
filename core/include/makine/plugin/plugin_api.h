// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once
#include "plugin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef MakinePluginInfo (*MakineFn_GetInfo)(void);
typedef MakineError      (*MakineFn_Initialize)(const char* dataPath);
typedef void               (*MakineFn_Shutdown)(void);
typedef bool               (*MakineFn_IsReady)(void);
typedef const char*        (*MakineFn_GetLastError)(void);

#ifdef __cplusplus
}
#endif
