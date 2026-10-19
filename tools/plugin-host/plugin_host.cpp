// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// makine-plugin-host: runs one plugin DLL outside the launcher.
//
//   makine-plugin-host.exe <absolute path to plugin.dll>
//
// The launcher starts one host per enabled plugin and talks to it over the
// host's stdin/stdout: frames of a uint32 little-endian length followed by that
// many bytes of UTF-8 JSON-RPC 2.0. Pipes the launcher made for this child alone
// have no name another process could open, so the channel needs no handshake.
// Whatever the plugin writes to stderr reaches the launcher as log output.
//
//   launcher -> host: initialize, invoke, shutdown (requests), event (notification)
//   host -> launcher: call (request), log, emit (notifications)
//
// Design and rationale: docs/adr/0009-plugin-system-v2.md.

#include <makine/plugin/abi.h>

#include <nlohmann/json.hpp>

#include <windows.h>
#include <shellapi.h>

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <io.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace {

constexpr uint32_t kMaxFrame = 16u * 1024 * 1024;

// JSON-RPC codes for failures of the host itself; plugin statuses are 1..7.
constexpr int kErrInvalidRequest = -32600;
constexpr int kErrMethodNotFound = -32601;
constexpr int kErrInternal       = -32603;
constexpr int kErrLoadFailed     = -32000;

HANDLE g_in = INVALID_HANDLE_VALUE;
HANDLE g_out = INVALID_HANDLE_VALUE;
std::mutex g_writeMutex;

// Requests and notifications from the launcher, run in order on the main thread.
std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::deque<json> g_queue;
bool g_eof = false;

// Calls the plugin made to the launcher, waiting for their responses.
struct PendingCall {
    bool done = false;
    json reply;
};
std::mutex g_callMutex;
std::condition_variable g_callCv;
std::map<int64_t, PendingCall*> g_pending;
int64_t g_nextCallId = 1;
bool g_closed = false;

struct {
    HMODULE module = nullptr;
    MakinePluginApi api{};
    bool initialized = false;
} g_plugin;

const json& member(const json& obj, const char* key)
{
    static const json kNull;
    if (!obj.is_object()) return kNull;
    const auto it = obj.find(key);
    return it == obj.end() ? kNull : *it;
}

std::string text(const json& obj, const char* key)
{
    const json& v = member(obj, key);
    return v.is_string() ? v.get<std::string>() : std::string();
}

std::string dump(const json& j)
{
    // Plugins may hand over invalid UTF-8; replace it rather than throw.
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

bool readExact(void* buf, DWORD len)
{
    auto* p = static_cast<char*>(buf);
    while (len > 0) {
        DWORD got = 0;
        if (!ReadFile(g_in, p, len, &got, nullptr) || got == 0) return false;
        p += got;
        len -= got;
    }
    return true;
}

bool writeAll(const void* buf, DWORD len)
{
    auto* p = static_cast<const char*>(buf);
    while (len > 0) {
        DWORD put = 0;
        if (!WriteFile(g_out, p, len, &put, nullptr) || put == 0) return false;
        p += put;
        len -= put;
    }
    return true;
}

std::optional<json> readFrame()
{
    unsigned char h[4];
    if (!readExact(h, 4)) return std::nullopt;
    const uint32_t len = uint32_t(h[0]) | uint32_t(h[1]) << 8 | uint32_t(h[2]) << 16
                         | uint32_t(h[3]) << 24;
    if (len > kMaxFrame) return std::nullopt;
    std::string body(len, '\0');
    if (len && !readExact(body.data(), len)) return std::nullopt;
    json msg = json::parse(body, nullptr, false);
    if (msg.is_discarded() || !msg.is_object()) return std::nullopt;
    return msg;
}

bool writeFrame(const json& msg)
{
    const std::string body = dump(msg);
    if (body.size() > kMaxFrame) return false;
    const auto len = static_cast<uint32_t>(body.size());
    const unsigned char h[4] = {static_cast<unsigned char>(len), static_cast<unsigned char>(len >> 8),
                                static_cast<unsigned char>(len >> 16), static_cast<unsigned char>(len >> 24)};
    std::lock_guard lock(g_writeMutex);
    return writeAll(h, 4) && writeAll(body.data(), len);
}

void notify(const char* method, json params)
{
    writeFrame({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
}

void fail(const json& id, int code, const std::string& message, json data = nullptr)
{
    json error = {{"code", code}, {"message", message}};
    if (!data.is_null()) error["data"] = std::move(data);
    writeFrame({{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(error)}});
}

void reply(const json& id, json result)
{
    if (!writeFrame({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}))
        fail(id, MAKINE_E_FAILED, "result larger than 16 MiB");
}

// What a plugin wrote back: JSON when it parses, else the raw text as a string.
json parsedOutput(const std::string& out)
{
    if (out.empty()) return nullptr;
    json v = json::parse(out, nullptr, false);
    return v.is_discarded() ? json(out) : v;
}

void failWithOutput(const json& id, MakineStatus status, const std::string& out,
                    const char* fallback)
{
    const json v = parsedOutput(out);
    std::string message = text(v, "message");
    if (message.empty()) message = v.is_string() ? v.get<std::string>() : fallback;
    fail(id, status, message);
}

json paramsOrEmpty(const char* params)
{
    if (!params || !*params) return json::object();
    return json::parse(params, nullptr, false);
}

void appendTo(void* ctx, const char* data, size_t len)
{
    if (data && len) static_cast<std::string*>(ctx)->append(data, len);
}

// ── MakineHostApi ───────────────────────────────────────────────────────────

// The last error a plugin logged; a failed initialize has no other way to say why.
std::mutex g_errorMutex;
std::string g_lastError;

void hostLog(void*, int32_t level, const char* message)
{
    if (!message) return;
    if (level >= MAKINE_LOG_ERROR) {
        std::lock_guard lock(g_errorMutex);
        g_lastError = message;
    }
    notify("log", {{"level", level}, {"message", message}});
}

MakineStatus hostCall(void*, const char* method, const char* paramsJson,
                      MakineWriteFn write, void* writeCtx)
{
    if (!method || !*method) return MAKINE_E_INVALID_ARGUMENT;
    json params = paramsOrEmpty(paramsJson);
    if (params.is_discarded()) return MAKINE_E_INVALID_ARGUMENT;

    PendingCall pending;
    int64_t id = 0;
    {
        std::lock_guard lock(g_callMutex);
        if (g_closed) return MAKINE_E_FAILED;
        id = g_nextCallId++;
        g_pending[id] = &pending;
    }
    const bool sent = writeFrame({{"jsonrpc", "2.0"}, {"id", id}, {"method", "call"},
                                  {"params", {{"method", method}, {"params", std::move(params)}}}});
    // No timeout here: some calls wait for the user (a region pick, a consent
    // dialog), and the launcher bounds every call it answers.
    std::unique_lock lock(g_callMutex);
    if (sent) g_callCv.wait(lock, [&] { return pending.done || g_closed; });
    g_pending.erase(id);
    if (!pending.done) return MAKINE_E_FAILED;
    lock.unlock();

    const json& error = member(pending.reply, "error");
    std::string out;
    MakineStatus status = MAKINE_OK;
    if (error.is_object()) {
        const json& code = member(error, "code");
        const int c = code.is_number_integer() ? code.get<int>() : MAKINE_E_FAILED;
        status = (c >= MAKINE_E_FAILED && c <= MAKINE_E_TIMEOUT) ? c : MAKINE_E_FAILED;
        out = dump({{"message", text(error, "message")}});
    } else {
        out = dump(member(pending.reply, "result"));
    }
    if (write) write(writeCtx, out.data(), out.size());
    return status;
}

void hostEmit(void*, const char* event, const char* payloadJson)
{
    if (!event || !*event) return;
    json payload = payloadJson && *payloadJson ? json::parse(payloadJson, nullptr, false) : json();
    if (payload.is_discarded()) return;
    notify("emit", {{"event", event}, {"payload", std::move(payload)}});
}

MakineHostApi g_hostApi{sizeof(MakineHostApi), MAKINE_PLUGIN_ABI_VERSION, nullptr,
                        &hostLog, &hostCall, &hostEmit};

// ── Requests from the launcher ──────────────────────────────────────────────

void handleInitialize(const json& id, const json& params, const std::wstring& dllPath)
{
    if (g_plugin.module) return fail(id, kErrInvalidRequest, "already initialized");

    // The plugin's own folder serves both its imports and its later LoadLibrary calls.
    const std::wstring dir = dllPath.substr(0, dllPath.find_last_of(L"\\/"));
    AddDllDirectory(dir.c_str());
    HMODULE module = LoadLibraryExW(dllPath.c_str(), nullptr,
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module)
        return fail(id, kErrLoadFailed, "LoadLibrary failed",
                    {{"winError", static_cast<uint32_t>(GetLastError())}});

    const auto entry = reinterpret_cast<MakinePluginEntryFn>(
        GetProcAddress(module, MAKINE_PLUGIN_ENTRY_SYMBOL));
    if (!entry) return fail(id, kErrLoadFailed, "makine_plugin_entry is not exported");

    MakinePluginApi api{};
    api.struct_size = sizeof api;
    const MakineStatus status = entry(MAKINE_PLUGIN_ABI_VERSION, &api);
    if (status != MAKINE_OK)
        return fail(id, status, "the plugin does not support this Makine version");
    if (api.abi_version < MAKINE_PLUGIN_ABI_MIN_VERSION || api.abi_version > MAKINE_PLUGIN_ABI_VERSION
        || api.struct_size < MAKINE_PLUGIN_API_MIN_SIZE || api.struct_size > sizeof api
        || !api.initialize || !api.shutdown || !api.invoke)
        return fail(id, MAKINE_E_UNSUPPORTED, "plugin ABI " + std::to_string(api.abi_version)
                                                  + " does not match the host");

    g_plugin.module = module;
    g_plugin.api = api;
    const std::string init = dump(member(params, "init").is_object() ? member(params, "init")
                                                                     : json::object());
    const MakineStatus initStatus = api.initialize(&g_hostApi, init.c_str());
    if (initStatus != MAKINE_OK) {
        std::lock_guard lock(g_errorMutex);
        return fail(id, initStatus, g_lastError.empty() ? "plugin initialize failed" : g_lastError);
    }
    g_plugin.initialized = true;
    reply(id, {{"abi", api.abi_version}});
}

void handleInvoke(const json& id, const json& params)
{
    if (!g_plugin.initialized) return fail(id, kErrInvalidRequest, "not initialized");
    const std::string method = text(params, "method");
    const json& args = member(params, "params");
    const std::string argsJson = dump(args.is_null() ? json::object() : args);
    std::string out;
    const MakineStatus status = g_plugin.api.invoke(method.c_str(), argsJson.c_str(), &appendTo, &out);
    if (status == MAKINE_OK) reply(id, parsedOutput(out));
    else failWithOutput(id, status, out, "plugin call failed");
}

void handleEvent(const json& params)
{
    if (!g_plugin.initialized || !g_plugin.api.on_event) return;
    const std::string event = text(params, "event");
    const std::string payload = dump(member(params, "payload"));
    g_plugin.api.on_event(event.c_str(), payload.c_str());
}

void shutdownPlugin()
{
    if (!g_plugin.initialized) return;
    g_plugin.initialized = false;
    g_plugin.api.shutdown();
}

void readerLoop()
{
    while (auto msg = readFrame()) {
        if (msg->contains("method")) {
            std::lock_guard lock(g_queueMutex);
            g_queue.push_back(std::move(*msg));
            g_queueCv.notify_one();
            continue;
        }
        // A response to one of the plugin's calls.
        const json& id = member(*msg, "id");
        if (!id.is_number_integer()) continue;
        std::lock_guard lock(g_callMutex);
        const auto it = g_pending.find(id.get<int64_t>());
        if (it == g_pending.end()) continue;
        it->second->reply = std::move(*msg);
        it->second->done = true;
        g_callCv.notify_all();
    }
    {
        std::lock_guard lock(g_callMutex);
        g_closed = true;
    }
    g_callCv.notify_all();
    {
        std::lock_guard lock(g_queueMutex);
        g_eof = true;
    }
    g_queueCv.notify_one();
}

// Takes stdin/stdout for the protocol and points the process's standard handles
// at NUL, so a plugin's printf cannot corrupt the stream.
bool claimStdio()
{
    const HANDLE self = GetCurrentProcess();
    if (!DuplicateHandle(self, GetStdHandle(STD_INPUT_HANDLE), self, &g_in, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)
        || !DuplicateHandle(self, GetStdHandle(STD_OUTPUT_HANDLE), self, &g_out, 0, FALSE,
                            DUPLICATE_SAME_ACCESS))
        return false;

    const int nul = _wopen(L"NUL", _O_RDWR | _O_BINARY);
    if (nul >= 0) {
        _dup2(nul, 0);
        _dup2(nul, 1);
        _close(nul);
    }
    const HANDLE nulHandle = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                         OPEN_EXISTING, 0, nullptr);
    SetStdHandle(STD_INPUT_HANDLE, nulHandle);
    SetStdHandle(STD_OUTPUT_HANDLE, nulHandle);
    // msvcrt fully buffers stderr when it is a pipe; the host ends with
    // TerminateProcess, so a buffered plugin message would never arrive.
    setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // A missing dependency must fail LoadLibrary, not open a dialog nobody sees.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // No current-directory or PATH lookups for anything loaded from now on.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc != 2) return 2;
    const std::wstring dllPath = argv[1];
    LocalFree(argv);

    if (!claimStdio()) return 3;
    std::thread(readerLoop).detach();

    for (;;) {
        json msg;
        {
            std::unique_lock lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return !g_queue.empty() || g_eof; });
            if (g_queue.empty()) break;   // launcher gone
            msg = std::move(g_queue.front());
            g_queue.pop_front();
        }

        const json& id = member(msg, "id");
        const std::string method = text(msg, "method");
        const json& params = member(msg, "params");
        try {
            if (method == "initialize") {
                handleInitialize(id, params, dllPath);
            } else if (method == "invoke") {
                handleInvoke(id, params);
            } else if (method == "event") {
                handleEvent(params);
            } else if (method == "shutdown") {
                shutdownPlugin();
                reply(id, nullptr);
                // Abrupt on purpose: ExitProcess would run DLL detach code while
                // plugin threads may hold locks. shutdown() was the plugin's turn.
                TerminateProcess(GetCurrentProcess(), 0);
            } else if (!id.is_null()) {
                fail(id, kErrMethodNotFound, "unknown method: " + method);
            }
        } catch (const std::exception& e) {
            if (!id.is_null()) fail(id, kErrInternal, e.what());
        }
    }

    // The launcher closed the pipe without a shutdown request (it exited or
    // crashed). Give the plugin a bounded chance to clean up.
    std::thread([] {
        Sleep(5000);
        TerminateProcess(GetCurrentProcess(), 4);
    }).detach();
    shutdownPlugin();
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
