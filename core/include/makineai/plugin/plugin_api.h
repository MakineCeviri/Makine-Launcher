#pragma once
#include "plugin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef MakineAiPluginInfo (*MakineAiFn_GetInfo)(void);
typedef MakineAiError      (*MakineAiFn_Initialize)(const char* dataPath);
typedef void               (*MakineAiFn_Shutdown)(void);
typedef bool               (*MakineAiFn_IsReady)(void);
typedef const char*        (*MakineAiFn_GetLastError)(void);

#ifdef __cplusplus
}
#endif
