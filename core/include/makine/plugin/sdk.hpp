// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file sdk.hpp
 * @brief Header-only C++ layer over the plugin ABI (abi.h).
 *
 *     class MyPlugin : public makine::plugin::Plugin {
 *     public:
 *         makine::plugin::Result invoke(std::string_view method, std::string_view params) override {
 *             if (method == "hello") return makine::plugin::Result::ok(R"({"text":"merhaba"})");
 *             return makine::plugin::Result::notFound(method);
 *         }
 *     };
 *     MAKINE_PLUGIN(MyPlugin)
 *
 * Payloads stay JSON text so the plugin picks its own JSON library. Exceptions
 * thrown by the plugin are caught here and turned into MAKINE_E_FAILED; they never
 * reach the host.
 */

#pragma once

#include "abi.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

namespace makine::plugin {

/// `s` as a quoted JSON string literal.
inline std::string jsonString(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    out += '"';
    return out;
}

/// Outcome of a call in either direction: a status and the JSON text that came with it.
struct Result {
    MakineStatus status = MAKINE_OK;
    std::string json;   // result on MAKINE_OK, {"message": "..."} otherwise

    bool succeeded() const { return status == MAKINE_OK; }

    static Result ok(std::string json = "null") { return {MAKINE_OK, std::move(json)}; }
    static Result error(MakineStatus status, std::string_view message)
    {
        return {status, "{\"message\":" + jsonString(message) + "}"};
    }
    static Result notFound(std::string_view method)
    {
        return error(MAKINE_E_NOT_FOUND, "unknown method: " + std::string(method));
    }
};

/// Makine's side of the ABI. Thread-safe; usable from any plugin thread.
class Host {
public:
    void log(int32_t level, std::string_view message) const
    {
        if (m_api) m_api->log(m_api->ctx, level, std::string(message).c_str());
    }
    void debug(std::string_view m) const { log(MAKINE_LOG_DEBUG, m); }
    void info(std::string_view m) const  { log(MAKINE_LOG_INFO, m); }
    void warn(std::string_view m) const  { log(MAKINE_LOG_WARN, m); }
    void error(std::string_view m) const { log(MAKINE_LOG_ERROR, m); }

    /// Blocking request to Makine; see docs/plugins for the methods.
    Result call(std::string_view method, std::string_view paramsJson = "{}") const
    {
        Result r;
        if (!m_api) return Result::error(MAKINE_E_FAILED, "host not ready");
        r.status = m_api->call(m_api->ctx, std::string(method).c_str(),
                               std::string(paramsJson).c_str(), &append, &r.json);
        return r;
    }

    void emitEvent(std::string_view event, std::string_view payloadJson = "null") const
    {
        if (m_api) m_api->emit_event(m_api->ctx, std::string(event).c_str(),
                                     std::string(payloadJson).c_str());
    }

    static void append(void* ctx, const char* data, size_t len)
    {
        if (data && len) static_cast<std::string*>(ctx)->append(data, len);
    }

private:
    template <class> friend struct Glue;
    const MakineHostApi* m_api = nullptr;
};

/// Base class for a plugin. One instance lives from initialize() to shutdown().
class Plugin {
public:
    virtual ~Plugin() = default;

    /// init is a JSON object: pluginId, version, launcherVersion, abi, locale, dataDir,
    /// pluginDir, settings (docs/plugins/README.md).
    virtual MakineStatus initialize(std::string_view /*initJson*/) { return MAKINE_OK; }
    virtual void shutdown() {}
    virtual Result invoke(std::string_view method, std::string_view paramsJson) = 0;
    virtual void onEvent(std::string_view /*event*/, std::string_view /*payloadJson*/) {}

    const Host& host() const { return m_host; }

private:
    template <class> friend struct Glue;
    Host m_host;
};

/// C entry points for plugin class T. Use through MAKINE_PLUGIN(T).
template <class T>
struct Glue {
    static T*& instance()
    {
        static T* p = nullptr;
        return p;
    }

    static MakineStatus initialize(const MakineHostApi* host, const char* init) noexcept
    {
        try {
            if (!instance()) instance() = new T();
            instance()->m_host.m_api = host;
            return instance()->initialize(init ? init : "{}");
        } catch (...) {
            return MAKINE_E_FAILED;
        }
    }

    static void shutdown() noexcept
    {
        try {
            if (instance()) instance()->shutdown();
        } catch (...) {
        }
        delete instance();
        instance() = nullptr;
    }

    static MakineStatus invoke(const char* method, const char* params,
                               MakineWriteFn write, void* writeCtx) noexcept
    {
        Result r;
        try {
            r = instance() ? instance()->invoke(method ? method : "", params ? params : "{}")
                           : Result::error(MAKINE_E_FAILED, "not initialized");
        } catch (const std::exception& e) {
            r = Result::error(MAKINE_E_FAILED, e.what());
        } catch (...) {
            r = Result::error(MAKINE_E_FAILED, "unknown exception");
        }
        if (write && !r.json.empty()) write(writeCtx, r.json.data(), r.json.size());
        return r.status;
    }

    static void onEvent(const char* event, const char* payload) noexcept
    {
        try {
            if (instance()) instance()->onEvent(event ? event : "", payload ? payload : "null");
        } catch (...) {
        }
    }

    static MakineStatus entry(uint32_t hostAbi, MakinePluginApi* out) noexcept
    {
        if (!out || hostAbi < MAKINE_PLUGIN_ABI_VERSION || out->struct_size < sizeof(MakinePluginApi))
            return MAKINE_E_UNSUPPORTED;
        const MakinePluginApi api{sizeof(MakinePluginApi), MAKINE_PLUGIN_ABI_VERSION,
                                  &initialize, &shutdown, &invoke, &onEvent};
        std::memcpy(out, &api, sizeof api);
        return MAKINE_OK;
    }
};

} // namespace makine::plugin

/// Exports makine_plugin_entry for plugin class `Class`.
#define MAKINE_PLUGIN(Class)                                                              \
    extern "C" MAKINE_PLUGIN_EXPORT MakineStatus makine_plugin_entry(                    \
        uint32_t hostAbi, MakinePluginApi* out)                                           \
    {                                                                                     \
        return ::makine::plugin::Glue<Class>::entry(hostAbi, out);                        \
    }
