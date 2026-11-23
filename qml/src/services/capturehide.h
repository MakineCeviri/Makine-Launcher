// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Keeps a window out of screen capture (desktop duplication, BitBlt, OBS,
// screenshots): the plugin overlay, so a plugin reading the screen never
// reads its own subtitles. Header-only so test_capturehide checks it on a
// real window with the overlay's styles.

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace capturehide {

/// true when the window is (now) excluded. Idempotent; false off Windows and before
/// Windows 10 2004, where the flag does not exist (lastError holds GetLastError()).
inline bool exclude(std::uintptr_t nativeWindow, unsigned long* lastError = nullptr)
{
#ifdef _WIN32
    const auto hwnd = reinterpret_cast<HWND>(nativeWindow);
    DWORD current = 0;
    if (GetWindowDisplayAffinity(hwnd, &current) && current == WDA_EXCLUDEFROMCAPTURE) return true;
    if (SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) return true;
    if (lastError) *lastError = GetLastError();
    return false;
#else
    (void)nativeWindow;
    (void)lastError;
    return false;
#endif
}

} // namespace capturehide
