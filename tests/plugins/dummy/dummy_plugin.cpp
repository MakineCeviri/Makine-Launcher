#include <makine/plugin/plugin_api.h>

static bool s_ready = false;
static const char* s_lastError = "";

extern "C" __declspec(dllexport)
MakinePluginInfo makine_get_info(void) {
    return {"com.makineceviri.dummy", "Dummy Test Plugin", "0.0.1", 1};
}

extern "C" __declspec(dllexport)
MakineError makine_initialize(const char* /*dataPath*/) {
    s_ready = true;
    return MAKINE_OK;
}

extern "C" __declspec(dllexport)
void makine_shutdown(void) {
    s_ready = false;
}

extern "C" __declspec(dllexport)
bool makine_is_ready(void) {
    return s_ready;
}

extern "C" __declspec(dllexport)
const char* makine_get_last_error(void) {
    return s_lastError;
}
