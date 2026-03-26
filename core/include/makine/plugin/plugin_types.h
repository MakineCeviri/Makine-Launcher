#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MAKINE_PLUGIN_API_VERSION 1

typedef enum {
    MAKINE_OK = 0,
    MAKINE_ERR_INIT_FAILED,
    MAKINE_ERR_NOT_READY,
    MAKINE_ERR_INVALID_PARAM,
    MAKINE_ERR_NOT_FOUND,
    MAKINE_ERR_ACCESS_DENIED,
    MAKINE_ERR_UNSUPPORTED,
    MAKINE_ERR_TIMEOUT,
    MAKINE_ERR_ENGINE_ERROR,
} MakineError;

typedef struct {
    const char* id;
    const char* name;
    const char* version;
    uint32_t apiVersion;
} MakinePluginInfo;

typedef enum {
    MAKINE_PLUGIN_TRANSLATION = 0,
    MAKINE_PLUGIN_ACCESSIBILITY,
    MAKINE_PLUGIN_HOOK,
    MAKINE_PLUGIN_OTHER,
} MakinePluginCategory;

#ifdef __cplusplus
}
#endif
