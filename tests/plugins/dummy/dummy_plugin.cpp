#include <makineai/plugin/plugin_api.h>

static bool s_ready = false;
static const char* s_lastError = "";

extern "C" __declspec(dllexport)
MakineAiPluginInfo makineai_get_info(void) {
    return {"com.makineceviri.dummy", "Dummy Test Plugin", "0.0.1", 1};
}

extern "C" __declspec(dllexport)
MakineAiError makineai_initialize(const char* /*dataPath*/) {
    s_ready = true;
    return MAKINEAI_OK;
}

extern "C" __declspec(dllexport)
void makineai_shutdown(void) {
    s_ready = false;
}

extern "C" __declspec(dllexport)
bool makineai_is_ready(void) {
    return s_ready;
}

extern "C" __declspec(dllexport)
const char* makineai_get_last_error(void) {
    return s_lastError;
}
