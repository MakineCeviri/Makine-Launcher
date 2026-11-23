// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "capturehide.h"

#include <gtest/gtest.h>

#ifdef _WIN32
// A hidden window with the styles Qt gives the plugin overlay (frameless tool
// window, click-through layered, no activation): it must come out excluded.
TEST(CaptureHide, OverlayStyledWindowIsExcludedFromCapture)
{
    const HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE
                                          | WS_EX_TOPMOST,
                                      L"STATIC", L"overlay", WS_POPUP, 0, 0, 200, 60, nullptr, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(hwnd, nullptr);
    unsigned long error = 0;
    EXPECT_TRUE(capturehide::exclude(reinterpret_cast<std::uintptr_t>(hwnd), &error)) << "GetLastError " << error;
    DWORD affinity = 0;
    ASSERT_TRUE(GetWindowDisplayAffinity(hwnd, &affinity));
    EXPECT_EQ(affinity, static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE));
    // A second call is a no-op that still reports success
    EXPECT_TRUE(capturehide::exclude(reinterpret_cast<std::uintptr_t>(hwnd)));
    // "Yayında göster": capture sees it again, and the swap back still works
    EXPECT_TRUE(capturehide::setExcluded(reinterpret_cast<std::uintptr_t>(hwnd), false));
    ASSERT_TRUE(GetWindowDisplayAffinity(hwnd, &affinity));
    EXPECT_EQ(affinity, static_cast<DWORD>(WDA_NONE));
    EXPECT_TRUE(capturehide::setExcluded(reinterpret_cast<std::uintptr_t>(hwnd), true));
    ASSERT_TRUE(GetWindowDisplayAffinity(hwnd, &affinity));
    EXPECT_EQ(affinity, static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE));
    DestroyWindow(hwnd);
}
#endif
