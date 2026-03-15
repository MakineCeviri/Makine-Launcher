/**
 * MakineAI Plugin Template
 *
 * This is a minimal plugin skeleton. All plugins must export
 * the 5 required C ABI functions listed below.
 *
 * Build: cmake -B build -G Ninja && cmake --build build
 * Pack:  python makine-pack.py ./build/release/
 *
 * See: https://github.com/MakineCeviri/MakineAI-Launcher/blob/main/core/include/makineai/plugin/plugin_api.h
 */

#include <makineai/plugin/plugin_api.h>

static bool s_ready = false;
static char s_error[256] = "";

// ── Required Exports ──

extern "C" __declspec(dllexport)
MakineAiPluginInfo makineai_get_info(void)
{
    return {
        "com.github-username.my-plugin",  // id — must match manifest.json
        "My MakineAI Plugin",              // name
        "0.1.0",                           // version
        MAKINEAI_PLUGIN_API_VERSION        // apiVersion
    };
}

extern "C" __declspec(dllexport)
MakineAiError makineai_initialize(const char* dataPath)
{
    // dataPath = AppData/Local/MakineAI/plugin-data/<your-plugin-id>/
    // Use this directory to store caches, configs, etc.
    (void)dataPath;

    // TODO: Initialize your plugin here
    s_ready = true;
    return MAKINEAI_OK;
}

extern "C" __declspec(dllexport)
void makineai_shutdown(void)
{
    // TODO: Clean up resources here
    s_ready = false;
}

extern "C" __declspec(dllexport)
bool makineai_is_ready(void)
{
    return s_ready;
}

extern "C" __declspec(dllexport)
const char* makineai_get_last_error(void)
{
    return s_error;
}
