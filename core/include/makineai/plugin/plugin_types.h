#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MAKINEAI_PLUGIN_API_VERSION 1

typedef enum {
    MAKINEAI_OK = 0,
    MAKINEAI_ERR_INIT_FAILED,
    MAKINEAI_ERR_NOT_READY,
    MAKINEAI_ERR_INVALID_PARAM,
    MAKINEAI_ERR_NOT_FOUND,
    MAKINEAI_ERR_ACCESS_DENIED,
    MAKINEAI_ERR_UNSUPPORTED,
    MAKINEAI_ERR_TIMEOUT,
    MAKINEAI_ERR_ENGINE_ERROR,
} MakineAiError;

typedef struct {
    const char* id;
    const char* name;
    const char* version;
    uint32_t apiVersion;
} MakineAiPluginInfo;

typedef enum {
    MAKINEAI_PLUGIN_TRANSLATION = 0,
    MAKINEAI_PLUGIN_ACCESSIBILITY,
    MAKINEAI_PLUGIN_HOOK,
    MAKINEAI_PLUGIN_OTHER,
} MakineAiPluginCategory;

#ifdef __cplusplus
}
#endif
