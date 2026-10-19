// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file abi.h
 * @brief Makine plugin ABI v2 — the only contract between a plugin DLL and Makine.
 *
 * A plugin is a Windows x64 DLL that exports one function, makine_plugin_entry.
 * Makine never loads it into the launcher: each plugin runs in its own
 * makine-plugin-host.exe, which loads the DLL and relays calls to and from the
 * launcher (docs/adr/0009-plugin-system-v2.md).
 *
 * Rules for both sides:
 *  - Data crosses as UTF-8 JSON text. Every pointer argument is valid only for the
 *    duration of the call that receives it; copy what you keep.
 *  - Results come back through a MakineWriteFn the caller supplies, so no module
 *    ever frees memory another module allocated. A callee may call write() any
 *    number of times; the chunks are concatenated.
 *  - Structs are only ever extended at the end. Read struct_size before touching a
 *    field added after ABI 2.
 *  - Threading: initialize, shutdown, invoke and on_event are called on one host
 *    thread, never concurrently. MakineHostApi functions are thread-safe and may be
 *    called from any plugin thread, including from inside invoke.
 *  - No C++ exceptions and no longjmp may cross these functions.
 *
 * Plain C so that any toolchain (MinGW, MSVC, Rust, Zig…) can implement it. C++
 * plugins should use makine/plugin/sdk.hpp instead of writing this by hand.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAKINE_PLUGIN_ABI_VERSION 2u
/* Oldest plugin ABI a host still loads. A host accepts every plugin from this
 * version up to its own and reads only the fields the plugin's struct_size covers,
 * so a plugin built against ABI 2 keeps working on later hosts. */
#define MAKINE_PLUGIN_ABI_MIN_VERSION 2u
#define MAKINE_PLUGIN_ENTRY_SYMBOL "makine_plugin_entry"

#if defined(_WIN32)
#  define MAKINE_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define MAKINE_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/* Status codes. int32_t rather than an enum: an enum's size is the compiler's choice. */
typedef int32_t MakineStatus;
#define MAKINE_OK                  0
#define MAKINE_E_FAILED            1  /* generic failure; result carries {"message": "..."} */
#define MAKINE_E_INVALID_ARGUMENT  2
#define MAKINE_E_NOT_FOUND         3  /* unknown method, event, key or game */
#define MAKINE_E_DENIED            4  /* the user or the launcher refused */
#define MAKINE_E_UNSUPPORTED       5  /* ABI version or feature not available */
#define MAKINE_E_BUSY              6
#define MAKINE_E_TIMEOUT           7

/* Log levels for MakineHostApi::log. */
#define MAKINE_LOG_DEBUG 0
#define MAKINE_LOG_INFO  1
#define MAKINE_LOG_WARN  2
#define MAKINE_LOG_ERROR 3

/* Receives result bytes (UTF-8 JSON). May be called several times; chunks concatenate. */
typedef void (*MakineWriteFn)(void* write_ctx, const char* data, size_t len);

/*
 * Services Makine offers a plugin. Given to initialize(); valid until shutdown()
 * returns. Pass `ctx` back as the first argument of every function.
 */
typedef struct MakineHostApi {
    uint32_t struct_size;   /* sizeof(MakineHostApi) as the host knows it */
    uint32_t abi_version;   /* MAKINE_PLUGIN_ABI_VERSION of the host */
    void*    ctx;

    /* Write a line to the plugin's log (shown in Makine's plugin page). */
    void (*log)(void* ctx, int32_t level, const char* message);

    /*
     * Ask Makine for something and wait for the answer (docs/plugins/README.md lists
     * the methods: settings.get, games.list, games.requestAttach, ui.notify, …).
     * params_json may be NULL for "{}". The answer is written through `write`.
     */
    MakineStatus (*call)(void* ctx, const char* method, const char* params_json,
                         MakineWriteFn write, void* write_ctx);

    /* Fire-and-forget event to Makine. Not named `emit`: Qt defines that as a macro. */
    void (*emit_event)(void* ctx, const char* event, const char* payload_json);
} MakineHostApi;

/* What a plugin offers Makine. Filled in by makine_plugin_entry. */
typedef struct MakinePluginApi {
    uint32_t struct_size;   /* set by the plugin: sizeof(MakinePluginApi) it was built with */
    uint32_t abi_version;   /* set by the plugin: MAKINE_PLUGIN_ABI_VERSION it was built with */

    /*
     * Called once before anything else. init_json carries pluginId, version,
     * launcherVersion, abi, locale, dataDir, pluginDir and the current settings. Return
     * MAKINE_OK to start; any other status stops the plugin and is shown to the user.
     */
    MakineStatus (*initialize)(const MakineHostApi* host, const char* init_json);

    /* Called once before the host exits. Stop threads and close windows here. */
    void (*shutdown)(void);

    /* Run a method — an action from the plugin page or a call from Makine. */
    MakineStatus (*invoke)(const char* method, const char* params_json,
                           MakineWriteFn write, void* write_ctx);

    /* Something happened (game.started, settings.changed, …). May be NULL. */
    void (*on_event)(const char* event, const char* payload_json);
} MakinePluginApi;

/* The MakinePluginApi of ABI 2: what a host requires at minimum. Fields added later
 * sit past this size. */
#define MAKINE_PLUGIN_API_MIN_SIZE (offsetof(MakinePluginApi, on_event) + sizeof(void (*)(void)))

/*
 * The one export. host_abi_version is the host's MAKINE_PLUGIN_ABI_VERSION. On entry
 * out->struct_size is the number of bytes the host allocated: write no more than
 * that, set struct_size to the number of bytes filled and return MAKINE_OK — or
 * MAKINE_E_UNSUPPORTED if this host is too old. Do no other work here; start in
 * initialize().
 */
typedef MakineStatus (*MakinePluginEntryFn)(uint32_t host_abi_version, MakinePluginApi* out);

#ifdef __cplusplus
}
#endif
