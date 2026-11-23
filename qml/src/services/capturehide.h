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

/// Excludes the window from capture (`excluded`) or lets capture see it again ("yayında
/// göster"). true when the window is now in that state. Idempotent; before Windows 10
/// 2004 the flag does not exist: false, and the window stays capturable (WDA_NONE).
inline bool setExcluded(std::uintptr_t nativeWindow, bool excluded, unsigned long* lastError = nullptr)
{
#ifdef _WIN32
    const auto hwnd = reinterpret_cast<HWND>(nativeWindow);
    const DWORD wanted = excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    DWORD current = 0;
    if (GetWindowDisplayAffinity(hwnd, &current) && current == wanted) return true;
    if (SetWindowDisplayAffinity(hwnd, wanted)) return true;
    if (lastError) *lastError = GetLastError();
    return false;
#else
    (void)nativeWindow;
    (void)excluded;
    (void)lastError;
    return false;
#endif
}

inline bool exclude(std::uintptr_t nativeWindow, unsigned long* lastError = nullptr)
{
    return setExcluded(nativeWindow, true, lastError);
}

} // namespace capturehide
