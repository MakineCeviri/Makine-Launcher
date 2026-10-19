// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Test plugin for the plugin host round trip (tests/ui/test_pluginprocess.cpp).
// Each method exercises one path of the ABI; nothing here is a real feature.

#include <makine/plugin/sdk.hpp>

#include <cstdio>
#include <stdexcept>
#include <string>

#include <windows.h>

using makine::plugin::Result;

class EchoPlugin : public makine::plugin::Plugin {
public:
    MakineStatus initialize(std::string_view init) override
    {
        if (init.find("\"failInit\":true") != std::string_view::npos) {
            host().error("echo: init refused");   // becomes the reason the user sees
            return MAKINE_E_DENIED;
        }
        // stdout belongs to the protocol; the host must have pointed it at NUL.
        std::printf("stdout must not reach the protocol\n");
        std::fflush(stdout);
        std::fprintf(stderr, "echo: stderr line\n");
        host().info("echo ready");
        return MAKINE_OK;
    }

    Result invoke(std::string_view method, std::string_view params) override
    {
        if (method == "echo") return Result::ok(std::string(params));
        if (method == "fail") return Result::error(MAKINE_E_DENIED, "nope");
        if (method == "throw") throw std::runtime_error("boom");
        if (method == "callHost") {
            Result r = host().call("test.method", params);
            return r.succeeded() ? Result::ok(r.json) : r;
        }
        if (method == "emit") {
            host().emitEvent("test.event", params);
            return Result::ok();
        }
        if (method == "env") {
            // What the host's application manifest gives every plugin.
            const auto dpi = GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
            return Result::ok("{\"acp\":" + std::to_string(GetACP()) + ",\"dpiAwareness\":"
                              + std::to_string(static_cast<int>(dpi)) + "}");
        }
        if (method == "lastEvent") return Result::ok(m_lastEvent.empty() ? "null" : m_lastEvent);
        if (method == "crash") __builtin_trap();
        if (method == "hang") Sleep(INFINITE);
        return Result::notFound(method);
    }

    void onEvent(std::string_view event, std::string_view payload) override
    {
        m_lastEvent = "{\"event\":" + makine::plugin::jsonString(event) + ",\"payload\":"
                      + std::string(payload) + "}";
    }

private:
    std::string m_lastEvent;
};

MAKINE_PLUGIN(EchoPlugin)
