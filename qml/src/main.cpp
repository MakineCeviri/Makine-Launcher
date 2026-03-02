/**
 * @file main.cpp
 * @brief MakineAI QML Application Entry Point
 * @copyright (c) 2026 MakineAI Team
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QFile>
#include <QDir>
#include <QSharedMemory>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QSurfaceFormat>
#include <QQuickWindow>
#include <QQuickGraphicsConfiguration>
#include <QThreadPool>
#include <QLibrary>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QJsonObject>
#include "services/profiler.h"
#include "services/crashreporter.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet
#include <dwmapi.h>    // DwmExtendFrameIntoClientArea
#include <cmath>
#include <atomic>
#include <mutex>

// DWM window style — adapts to Windows version
namespace {

DWORD getWindowsBuildNumber() {
    OSVERSIONINFOW ovi{};
    ovi.dwOSVersionInfoSize = sizeof(ovi);
    auto RtlGetVersion = reinterpret_cast<LONG(WINAPI*)(OSVERSIONINFOW*)>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (RtlGetVersion) RtlGetVersion(&ovi);
    return ovi.dwBuildNumber;
}

bool isWindows11() { return getWindowsBuildNumber() >= 22000; }

// DWMWA constants not in older SDK headers
constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
constexpr DWORD DWMWA_BORDER_COLOR = 34;
constexpr DWORD DWMWA_CAPTION_COLOR = 35;
constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE = 38;

// DWM_WINDOW_CORNER_PREFERENCE values
constexpr DWORD DWMWCP_ROUND = 2;

// DWM_SYSTEMBACKDROP_TYPE values
constexpr DWORD DWMSBT_MAINWINDOW = 2; // Mica
constexpr DWORD DWMSBT_TABBEDWINDOW = 4; // Mica Alt

void configureWindowStyle(HWND hwnd) {
    DWORD build = getWindowsBuildNumber();

    // Dark mode frame — W10 1903+ (build >= 18362) and all W11
    if (build >= 18362) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                              &darkMode, sizeof(darkMode));
    }

    if (build >= 22000) {
        // ── Windows 11 ──
        // Rounded corners (DWM handles clipping automatically)
        DWORD cornerPref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &cornerPref, sizeof(cornerPref));

        // Mica backdrop — the signature W11 frosted material
        DWORD backdropType = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropType, sizeof(backdropType));

        // Extend DWM frame into entire client area (needed for Mica to render)
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        // Dark border color
        COLORREF borderColor = RGB(30, 30, 35);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR,
                              &borderColor, sizeof(borderColor));
    }
    // Windows 10 — no special treatment needed for frameless windows.
    // DWM provides standard sharp-cornered dark frame automatically
    // when DWMWA_USE_IMMERSIVE_DARK_MODE is set.
}

} // namespace

// Native Win32 splash window — shown immediately while QML loads
// 440×240, rounded corners, logo, gradient bars, loading dots
// Threaded splash: runs its own message loop on a dedicated thread so it stays
// responsive while the main thread blocks in QML create() (~4 seconds).
class SplashWindow {
public:
    SplashWindow() = default;
    ~SplashWindow() { close(); if (m_logoBitmap) DeleteObject(m_logoBitmap); }
    SplashWindow(const SplashWindow&) = delete;
    SplashWindow& operator=(const SplashWindow&) = delete;

    void setInterFont(bool loaded) { m_interLoaded = loaded; }

    // Load logo from QImage (call before show — main thread, before splash thread starts)
    void setLogo(const QImage& img) {
        if (img.isNull()) return;
        QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_logoWidth = converted.width();
        m_logoHeight = converted.height();

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = m_logoWidth;
        bmi.bmiHeader.biHeight = -m_logoHeight; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screenDC = GetDC(nullptr);
        BYTE* pixels = nullptr;
        m_logoBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS,
                                         reinterpret_cast<void**>(&pixels), nullptr, 0);
        ReleaseDC(nullptr, screenDC);

        if (m_logoBitmap && pixels)
            memcpy(pixels, converted.constBits(), m_logoWidth * m_logoHeight * 4);
    }

    // Spawn splash on its own thread (returns immediately)
    void show() {
        m_thread = CreateThread(nullptr, 0, splashThreadProc, this, 0, &m_threadId);
        // Wait until HWND is created so setStatus/close work
        while (!m_ready.load(std::memory_order_acquire))
            Sleep(1);
    }

    // Thread-safe: posts status text to splash thread
    void setStatus(const wchar_t* text) {
        std::lock_guard lk(m_statusMutex);
        wcsncpy(m_status, text, 63);
        m_status[63] = L'\0';
        if (m_hwnd)
            PostMessageW(m_hwnd, WM_APP + 1, 0, 0); // trigger repaint
    }

    // No-op — kept for API compatibility (splash pumps its own messages)
    void pumpMessages() {}

    void waitMinimumDisplay(DWORD minMs) {
        DWORD showTime = m_showTime.load(std::memory_order_acquire);
        if (!showTime) return;
        DWORD elapsed = GetTickCount() - showTime;
        if (elapsed < minMs)
            Sleep(minMs - elapsed);
    }

    // Thread-safe: signals splash thread to close and waits for it
    void close() {
        if (m_hwnd) {
            PostMessageW(m_hwnd, WM_APP + 2, 0, 0); // request close
        }
        if (m_thread) {
            WaitForSingleObject(m_thread, 3000);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }
        m_hwnd = nullptr;
    }

private:
    // Brand gradient palette (MakineAI official colors)
    static constexpr COLORREF kBrandColors[] = {
        RGB(252, 205, 102), RGB(247, 174, 118), RGB(238, 150, 143),
        RGB(204, 159, 216), RGB(144, 194, 230), RGB(119, 219, 200),
        RGB(128, 229, 157), RGB(200, 235, 124), RGB(212, 190, 119)
    };
    static constexpr int kColorCount = 9;

    static COLORREF lerpColor(COLORREF a, COLORREF b, float f) {
        return RGB(
            GetRValue(a) + (int)((GetRValue(b) - GetRValue(a)) * f),
            GetGValue(a) + (int)((GetGValue(b) - GetGValue(a)) * f),
            GetBValue(a) + (int)((GetBValue(b) - GetBValue(a)) * f)
        );
    }

    static void drawAnimatedGradientBar(HDC hdc, int x, int y, int barW, int barH, float phase) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = barW;
        bmi.bmiHeader.biHeight = -barH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        BYTE* pixels = nullptr;
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                       reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!dib || !pixels) return;

        float halfW = barW / 2.0f;
        for (int px = 0; px < barW; ++px) {
            float dist = (px < halfW)
                ? (halfW - px) / halfW
                : (px - halfW) / halfW;

            float t = phase - dist * 0.65f;
            t = t - (float)(int)(t);
            if (t < 0.0f) t += 1.0f;

            float colorPos = t * (kColorCount - 1);
            int idx = (int)colorPos;
            float frac = colorPos - idx;
            if (idx >= kColorCount - 1) { idx = kColorCount - 2; frac = 1.0f; }

            COLORREF c = lerpColor(kBrandColors[idx], kBrandColors[idx + 1], frac);
            BYTE r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);

            for (int py = 0; py < barH; ++py) {
                int off = (py * barW + px) * 4;
                pixels[off + 0] = b;
                pixels[off + 1] = g;
                pixels[off + 2] = r;
                pixels[off + 3] = 255;
            }
        }

        HDC dibDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(dibDC, dib);
        BitBlt(hdc, x, y, barW, barH, dibDC, 0, 0, SRCCOPY);
        SelectObject(dibDC, oldBmp);
        DeleteObject(dib);
        DeleteDC(dibDC);
    }

    // Soft breathing glow — brand colors blend and pulse gently around logo
    static void drawBreathingGlow(HDC hdc, int cx, int cy, int radius, float elapsed) {
        int size = radius * 2;
        int x0 = cx - radius, y0 = cy - radius;

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        BYTE* pixels = nullptr;
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                       reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!dib || !pixels) return;
        memset(pixels, 0, size * size * 4);

        // Slow color cycling — blend between two adjacent brand colors
        float colorT = elapsed * 0.3f; // full cycle ~3.3s per color pair
        colorT = colorT - (float)(int)colorT;
        int ci = (int)(colorT * kColorCount) % kColorCount;
        int ci2 = (ci + 1) % kColorCount;
        float cf = colorT * kColorCount - (float)(int)(colorT * kColorCount);
        COLORREF glowColor = lerpColor(kBrandColors[ci], kBrandColors[ci2], cf);

        // Breathing pulse — smooth sine wave
        float breath = 0.5f + 0.5f * sinf(elapsed * 1.8f); // ~0.55 Hz
        float peakAlpha = 0.12f + breath * 0.13f; // range: 0.12 → 0.25

        float invR = 1.0f / (float)radius;
        BYTE gr = GetRValue(glowColor), gg = GetGValue(glowColor), gb = GetBValue(glowColor);

        for (int py = 0; py < size; ++py) {
            float dy = (float)(py - radius) * invR;
            for (int px = 0; px < size; ++px) {
                float dx = (float)(px - radius) * invR;
                float dist2 = dx * dx + dy * dy;
                if (dist2 > 1.0f) continue;

                // Gaussian-like radial falloff
                float alpha = peakAlpha * expf(-dist2 * 3.0f);
                if (alpha < 0.003f) continue;

                int off = (py * size + px) * 4;
                pixels[off + 0] = (BYTE)(gb * alpha);
                pixels[off + 1] = (BYTE)(gg * alpha);
                pixels[off + 2] = (BYTE)(gr * alpha);
                pixels[off + 3] = (BYTE)(alpha * 255.0f);
            }
        }

        HDC dibDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(dibDC, dib);
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        alphaBlend(hdc, x0, y0, size, size, dibDC, 0, 0, size, size, bf);
        SelectObject(dibDC, oldBmp);
        DeleteObject(dib);
        DeleteDC(dibDC);
    }

    // AlphaBlend via dynamic loading (avoids link-time msimg32 dependency)
    static BOOL alphaBlend(HDC dest, int dx, int dy, int dw, int dh,
                           HDC src, int sx, int sy, int sw, int sh,
                           BLENDFUNCTION bf) {
        using Fn = BOOL(WINAPI*)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
        static Fn fn = reinterpret_cast<Fn>(
            GetProcAddress(LoadLibraryExW(L"msimg32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), "AlphaBlend"));
        if (fn) return fn(dest, dx, dy, dw, dh, src, sx, sy, sw, sh, bf);
        return BitBlt(dest, dx, dy, dw, dh, src, sx, sy, SRCCOPY);
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<SplashWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right, h = rc.bottom;

            // Double-buffer
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            // Soft dark background — subtle vertical gradient
            for (int y = 0; y < h; ++y) {
                float t = (float)y / (float)h;
                int r = (int)(12 + t * 6);
                int g = (int)(12 + t * 5);
                int b = (int)(18 + t * 8);
                HBRUSH rowBr = CreateSolidBrush(RGB(r, g, b));
                RECT rowRc = {0, y, w, y + 1};
                FillRect(mem, &rowRc, rowBr);
                DeleteObject(rowBr);
            }

            int cx = w / 2, cy = h / 2 - 20;

            // Animated gradient phase from elapsed time
            DWORD showTime = self ? self->m_showTime.load(std::memory_order_relaxed) : 0;
            float elapsed = showTime ? (GetTickCount() - showTime) / 1000.0f : 0.0f;
            float gp = 0.25f + elapsed * 0.15f; // slow sweep

            // Soft breathing glow around logo
            drawBreathingGlow(mem, cx, cy, 100, elapsed);

            // Top gradient bar (3px)
            drawAnimatedGradientBar(mem, 0, 0, w, 3, gp);

            SetBkMode(mem, TRANSPARENT);

            // Logo
            if (self && self->m_logoBitmap) {
                HDC logoDC = CreateCompatibleDC(mem);
                HBITMAP oldLogoBmp = (HBITMAP)SelectObject(logoDC, self->m_logoBitmap);

                BLENDFUNCTION bf{};
                bf.BlendOp = AC_SRC_OVER;
                bf.SourceConstantAlpha = 255;
                bf.AlphaFormat = AC_SRC_ALPHA;

                int logoX = (w - self->m_logoWidth) / 2;
                int logoY = cy - self->m_logoHeight / 2;
                alphaBlend(mem, logoX, logoY, self->m_logoWidth, self->m_logoHeight,
                           logoDC, 0, 0, self->m_logoWidth, self->m_logoHeight, bf);

                SelectObject(logoDC, oldLogoBmp);
                DeleteDC(logoDC);
            }

            // Status text
            if (self) {
                wchar_t statusBuf[64]{};
                {
                    std::lock_guard lk(self->m_statusMutex);
                    wcsncpy(statusBuf, self->m_status, 63);
                }
                if (statusBuf[0]) {
                    HFONT statusFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, (self && self->m_interLoaded) ? L"Inter" : L"Segoe UI");
                    HFONT oldSFont = (HFONT)SelectObject(mem, statusFont);
                    SetTextColor(mem, RGB(100, 100, 120));
                    RECT statusRc = {0, h - 62, w, h - 46};
                    DrawTextW(mem, statusBuf, -1, &statusRc, DT_CENTER | DT_SINGLELINE);
                    SelectObject(mem, oldSFont);
                    DeleteObject(statusFont);
                }
            }

            // Version
            HFONT verFont = CreateFontW(-9, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, (self && self->m_interLoaded) ? L"Inter" : L"Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(mem, verFont);
            SetTextColor(mem, RGB(60, 60, 75));
            RECT verRc = {0, h - 24, w - 14, h - 8};
            // Version from CMake — MAKINEAI_APP_VERSION is narrow, convert to wide
            auto _verStr = QStringLiteral("v" MAKINEAI_APP_VERSION);
            DrawTextW(mem, (LPCWSTR)_verStr.utf16(), -1, &verRc, DT_RIGHT | DT_SINGLELINE);
            SelectObject(mem, oldFont);
            DeleteObject(verFont);

            // Bottom gradient bar (3px)
            drawAnimatedGradientBar(mem, 0, h - 3, w, 3, gp);

            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == 1)
                InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_APP + 1: // status update from main thread
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_APP + 2: // close request from main thread
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // Splash thread entry point
    static DWORD WINAPI splashThreadProc(LPVOID param) {
        auto* self = static_cast<SplashWindow*>(param);

        // Register window class on this thread
        WNDCLASSW wc{};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"MakineAISplash";
        wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 15));
        wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
        RegisterClassW(&wc);

        constexpr int w = 440, h = 240;
        int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

        self->m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"MakineAISplash", L"",
            WS_POPUP,
            sx, sy, w, h,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        if (self->m_hwnd) {
            HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 12, 12);
            SetWindowRgn(self->m_hwnd, rgn, TRUE);

            SetWindowLongPtrW(self->m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            ShowWindow(self->m_hwnd, SW_SHOWNOACTIVATE);
            UpdateWindow(self->m_hwnd);
            self->m_showTime.store(GetTickCount(), std::memory_order_release);

            // 30 fps animation timer
            SetTimer(self->m_hwnd, 1, 33, nullptr);
        }

        // Signal main thread that HWND is ready
        self->m_ready.store(true, std::memory_order_release);

        // Own message loop — runs independently of main thread
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return 0;
    }

    HWND m_hwnd{nullptr};
    HANDLE m_thread{nullptr};
    DWORD m_threadId{0};
    std::atomic<DWORD> m_showTime{0};
    std::atomic<bool> m_ready{false};
    mutable std::mutex m_statusMutex;
    wchar_t m_status[64]{};
    HBITMAP m_logoBitmap{nullptr};
    int m_logoWidth{0};
    int m_logoHeight{0};
    bool m_interLoaded{false};
};
#endif

namespace {
constexpr int kStartupSettleMs = 5000;
}

// Resolve log file path using organized directory layout
#include "services/apppaths.h"
static QString getLogFilePath() {
    QString logDir = makineai::AppPaths::logsDir();
    QDir().mkpath(logDir);
    return makineai::AppPaths::debugLog();
}

// File-based logging for debugging
void logToFile(const QString& msg) {
    static const QString logPath = getLogFilePath();
    QFile file(logPath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " - " << msg << "\n";
        file.close();
    }
}

#include <QTimer>


// Anti-RE protection (no-op in debug builds)
#include "services/appprotection.h"

// Backend services
#include "services/gameservice.h"
#include "services/supportedgamesmodel.h"
#include "services/catalogproxymodel.h"
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/processscanner.h"
#include "services/systemtraymanager.h"
#include "services/integrityservice.h"
#include "services/batchoperationservice.h"
#include "services/updateservice.h"
#include "services/operationjournal.h"
#include "services/imagecachemanager.h"
#include "services/manifestsyncservice.h"
#include "services/translationdownloader.h"
#include "services/corebridge.h"
#ifdef MAKINEAI_DEV_TOOLS
#include "services/frametimer.h"
#include "services/sceneprofiler.h"
#include "services/memoryprofiler.h"
#endif

int main(int argc, char *argv[])
{
    // === RENDER LOOP ===
    // "threaded" = render on separate thread, overlaps CPU+GPU work.
    // "basic" = single-threaded, best idle efficiency but blocks main thread during render.
    qputenv("QSG_RENDER_LOOP", "threaded");

    // === GRAPHICS BACKEND ===
    // Qt RHI default: D3D11 on Windows, OpenGL on Linux, Metal on macOS.
    // No explicit setGraphicsApi() — let Qt pick the best available backend.
    // Override: QSG_RHI_BACKEND env var always takes precedence.
    // No special alpha buffer needed — QML renders the background directly.

    // === RELEASE SECURITY ===
#ifdef NDEBUG
    qputenv("QT_QML_NO_DEBUGGER", "1");   // Disable QML debugger in release
    qputenv("QML_DISABLE_DISK_CACHE", "0");
#endif

    // Disable RHI debug/validation layers (saves ~10 MB + CPU)
    qputenv("QSG_RHI_DEBUG_LAYER", "0");

    // === MEMORY OPTIMIZATIONS ===
    // Texture atlas: 512×512 = 1 MB/atlas (default 2048×2048 = 16 MB)
    qputenv("QSG_ATLAS_WIDTH", "512");
    qputenv("QSG_ATLAS_HEIGHT", "512");

    // V4 JS engine: aggressive GC to free unused JS heap sooner
    qputenv("QV4_MM_AGGRESSIVE_GC", "1");

    // Single instance check
    // When launched with --post-update, the old process may still be exiting.
    // Retry a few times to allow the kernel to release the shared memory handle.
    bool isPostUpdate = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--post-update") == 0) {
            isPostUpdate = true;
            break;
        }
    }

    const int maxRetries = isPostUpdate ? 10 : 1;
    bool instanceAcquired = false;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QSharedMemory cleanupMemory("MakineAI_SingleInstance_Guard");
        if (cleanupMemory.attach())
            cleanupMemory.detach();

        QSharedMemory testGuard("MakineAI_SingleInstance_Guard");
        if (testGuard.create(1)) {
            instanceAcquired = true;
            break;
        }

        if (attempt + 1 < maxRetries)
            Sleep(500);  // Wait 500ms before retry
    }

    // Persistent guard — lives for the lifetime of the process
    QSharedMemory singleInstanceGuard("MakineAI_SingleInstance_Guard");
    if (!instanceAcquired) {
        // Last attempt failed — show error
        singleInstanceGuard.attach(); // ensure cleanup on exit
#ifdef Q_OS_WIN
        MessageBoxW(nullptr,
            L"MakineAI zaten çalışıyor.\n\n"
            L"Lütfen sistem tepsisindeki simgeyi kontrol edin "
            L"veya görev yöneticisinden kapatın.",
            L"MakineAI",
            MB_OK | MB_ICONWARNING);
#endif
        return 0;
    }
    singleInstanceGuard.create(1);

    QGuiApplication app(argc, argv);

    // === Phase 0: Crash reporting (as early as possible after QApp) ===
    makineai::CrashReporter::initialize();
    makineai::CrashReporter::installQtMessageHandler();

    // Anti-RE: run all checks before anything else (no-op in debug builds)
    makineai::protection::initialize();

#ifdef Q_OS_WIN
    SplashWindow splash;
    // Load Inter font for splash (before splash thread starts)
    bool interLoaded = false;
    {
        QFile fontFile(":/qt/qml/MakineAI/resources/fonts/Inter-Medium.ttf");
        if (fontFile.open(QIODevice::ReadOnly)) {
            QByteArray fontData = fontFile.readAll();
            DWORD numFonts = 0;
            HANDLE hFont = AddFontMemResourceEx(fontData.data(), fontData.size(), nullptr, &numFonts);
            interLoaded = (hFont != nullptr && numFonts > 0);
        }
    }
    splash.setInterFont(interLoaded);
    // Load logo from Qt resources and scale for splash
    {
        QImage logoImg(":/qt/qml/MakineAI/resources/images/logo.png");
        if (!logoImg.isNull())
            splash.setLogo(logoImg.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    splash.show();
#endif

    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("MakineAI");
    app.setApplicationDisplayName(QStringLiteral("Makine \u00C7eviri - MakineAI"));
    app.setApplicationVersion(MAKINEAI_APP_VERSION);
    app.setOrganizationName("MakineAI");
    app.setOrganizationDomain("makineai.com");
    app.setWindowIcon(QIcon(":/qt/qml/MakineAI/resources/images/logo.png"));
    QQuickStyle::setStyle("Basic");

    int interRegular = QFontDatabase::addApplicationFont(":/qt/qml/MakineAI/resources/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineAI/resources/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineAI/resources/fonts/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineAI/resources/fonts/Inter-Bold.ttf");

    QString fontFamily = interRegular >= 0 ? "Inter" : "Segoe UI";
    QFont defaultFont(fontFamily, 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(defaultFont);

    // Pipeline cache: save compiled shaders so subsequent launches skip compilation stutter
    {
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        qputenv("QSG_RHI_PIPELINE_CACHE_SAVE", (cacheDir + "/pipeline_cache.bin").toUtf8());
        qputenv("QSG_RHI_PIPELINE_CACHE_LOAD", (cacheDir + "/pipeline_cache.bin").toUtf8());
    }

    // Reduce thread pool: default = CPU count, each thread = 1 MB stack
    QThreadPool::globalInstance()->setMaxThreadCount(2);

    QQmlApplicationEngine engine;

    // Dev tools availability (must be set BEFORE QML creation)
#ifdef MAKINEAI_DEV_TOOLS
    engine.rootContext()->setContextProperty("devToolsEnabled", true);
#else
    engine.rootContext()->setContextProperty("devToolsEnabled", false);
#endif

    // ===== Register backend singletons for QML access =====
    // Manual registration — works in both shared and static Qt builds.
    // (QML_ELEMENT/QML_SINGLETON relies on linker keeping registration code,
    //  which --gc-sections strips in static builds.)
    using namespace makineai;

    QElapsedTimer startupTimer;
    startupTimer.start();

    // ===== Phase 1: Directory structure + configuration =====
    makineai::CrashReporter::addBreadcrumb("startup", "Phase 1: Directory structure + configuration");
#ifdef Q_OS_WIN
    splash.setStatus(L"Dizin yap\u0131s\u0131 haz\u0131rlan\u0131yor...");
#endif
    AppPaths::ensureDirectories();
    AppPaths::migrateFromFlatLayout();

    // Post-update cleanup (delegate to UpdateService)
    if (isPostUpdate) {
        UpdateService::handlePostUpdate();
        logToFile("Post-update cleanup completed");
    }

#ifdef Q_OS_WIN
    splash.setStatus(L"Yap\u0131land\u0131rma y\u00FCkleniyor...");
#endif
    auto* settingsManager = new SettingsManager(&app);
    engine.rootContext()->setContextProperty("SettingsManager", settingsManager);

    // ===== Phase 2: Image cache =====
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00F6rsel \u00F6nbelle\u011Fi ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* imageCache = new ImageCacheManager(&app);
    engine.rootContext()->setContextProperty("ImageCache", imageCache);

    // ===== Phase 2.5: Manifest sync (loads cached index, starts background sync) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Katalog haz\u0131rlan\u0131yor...");
#endif
    auto* manifestSync = new ManifestSyncService(&app);
    engine.rootContext()->setContextProperty("ManifestSync", manifestSync);
    // syncCatalog() called in Phase 7.5 — loads catalog index before QML creation

    auto* translationDownloader = new TranslationDownloader(&app);
    translationDownloader->setManifestSync(manifestSync);
    translationDownloader->setDataPath(makineai::AppPaths::packagesDir());
    engine.rootContext()->setContextProperty("TranslationDownloader", translationDownloader);

    // Register model types for QML
    qmlRegisterUncreatableType<makineai::SupportedGamesModel>("MakineAI", 1, 0,
        "SupportedGamesModel", "Use GameService.supportedGamesModel");
    qmlRegisterType<makineai::CatalogProxyModel>("MakineAI", 1, 0, "CatalogProxyModel");

    // UpdateService registered as singleton instance in Phase 7 (below)

    // ===== Phase 3: Game library (construction only — data loads after QML) =====
    makineai::CrashReporter::addBreadcrumb("startup", "Phase 3: Game library construction");
#ifdef Q_OS_WIN
    splash.setStatus(L"Oyun k\u00FCt\u00FCphanesi haz\u0131rlan\u0131yor...");
#endif
    auto* gameService = new GameService(&app);
    gameService->setManifestSync(manifestSync);
    engine.rootContext()->setContextProperty("GameService", gameService);
    // initialize() deferred to after first frame render (see Phase 10)
    logToFile(QString("Phase 3 (GameService created) at %1 ms").arg(startupTimer.elapsed()));
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    // ===== Phase 4: Operation journal + recovery =====
#ifdef Q_OS_WIN
    splash.setStatus(L"\u0130\u015Flem g\u00FCnl\u00FC\u011F\u00FC kontrol ediliyor...");
#endif
    auto* journal = new OperationJournal(&app);
    if (journal->hasPendingOperation()) {
        qDebug() << "OperationJournal: recovering from interrupted operation...";
        journal->recover();
    }
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    // ===== Phase 5: Backup + process monitoring =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Yedekleme sistemi ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* backupManager = new BackupManager(&app);
    backupManager->setJournal(journal);
    engine.rootContext()->setContextProperty("BackupManager", backupManager);

#ifdef Q_OS_WIN
    splash.setStatus(L"S\u00FCrec izleyici ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* processScanner = new ProcessScanner(&app);
    processScanner->setGameService(gameService);
    engine.rootContext()->setContextProperty("ProcessScanner", processScanner);

    // Rebuild process map when game library scan completes
    // PackageManager is lazy-init (created during first scanAllLibraries),
    // so we inject it + rebuild on every scanCompleted
    QObject::connect(gameService, &GameService::scanCompleted,
                     processScanner, [processScanner]() {
        processScanner->setPackageManager(CoreBridge::instance()->packageManager());
        processScanner->rebuildProcessMap();
    });

    // ===== Phase 6: Security + integrity =====
#ifdef Q_OS_WIN
    splash.setStatus(L"B\u00FCt\u00FCnl\u00FCk do\u011Frulamas\u0131 yap\u0131l\u0131yor...");
#endif
    auto* integrityService = new IntegrityService(&app);
    engine.rootContext()->setContextProperty("IntegrityService", integrityService);

    auto* batchService = new BatchOperationService(&app);
    engine.rootContext()->setContextProperty("BatchOperationService", batchService);

    // ===== Phase 7: Update service + system tray =====
    makineai::CrashReporter::addBreadcrumb("startup", "Phase 7: Update service + system tray");
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00FCncelleme servisi haz\u0131rlan\u0131yor...");
#endif
    auto* updateService = UpdateService::create(nullptr, nullptr);
    updateService->setParent(&app);
    // Singleton instance: exposes BOTH the instance AND Q_ENUM(State) values to QML.
    // (setContextProperty only exposes the instance — enum constants resolve to undefined)
    qmlRegisterSingletonInstance("MakineAI", 1, 0, "UpdateService", updateService);

    // Startup update check — once, async, unless we just updated
    if (!isPostUpdate)
        updateService->check();

#ifdef Q_OS_WIN
    splash.setStatus(L"Sistem tepsisi yap\u0131land\u0131r\u0131l\u0131yor...");
#endif
    SystemTrayManager trayManager;
    trayManager.setIcon(app.windowIcon());
    trayManager.show();
    engine.rootContext()->setContextProperty("SystemTrayManager", &trayManager);

    // Tray quit → app quit directly (bypass QML round-trip)
    QObject::connect(&trayManager, &SystemTrayManager::quitRequested,
                     &app, &QCoreApplication::quit);

    // Wire journal to CoreBridge
    CoreBridge::instance()->setJournal(journal);
    engine.rootContext()->setContextProperty("CoreBridge", CoreBridge::instance());

    logToFile(QString("Services initialized in %1 ms").arg(startupTimer.elapsed()));
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    logToFile("=== MakineAI Starting ===");
    logToFile(QString("App version: %1").arg(app.applicationVersion()));
    logToFile(QString("Qt version: %1").arg(qVersion()));
    logToFile(QString("Log file: %1").arg(getLogFilePath()));
    logToFile(QString("QSG_RENDER_LOOP: %1").arg(qEnvironmentVariable("QSG_RENDER_LOOP")));
    {
        QSettings settings("MakineAI", "MakineAI");
        auto api = QQuickWindow::graphicsApi();
        QString apiName = api == QSGRendererInterface::Direct3D12 ? "D3D12" :
                          api == QSGRendererInterface::Vulkan     ? "Vulkan" :
                          api == QSGRendererInterface::Direct3D11 ? "D3D11" :
                          api == QSGRendererInterface::OpenGL     ? "OpenGL" :
                          "Auto (RHI default)";
        logToFile(QString("Graphics backend: %1").arg(apiName));
    }
    logToFile(QString("Swap interval: %1").arg(QSurfaceFormat::defaultFormat().swapInterval()));

    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                logToFile(QString("QML Warning: %1").arg(warning.toString()));
            }
        }
    );

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            logToFile("CRITICAL: QML Object creation failed!");
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    // ===== Phase 7.5: Wire signals + sync catalog index =====
    // Connect ManifestSync signals BEFORE syncing,
    // so catalogReady/packageDetailReady are never missed.
    QObject::connect(manifestSync, &makineai::ManifestSyncService::catalogReady,
        gameService, [gameService]() {
            if (auto* bridge = makineai::CoreBridge::instance())
                bridge->refreshPackageManifest();
        });
    QObject::connect(manifestSync, &makineai::ManifestSyncService::packageDetailReady,
        gameService, [manifestSync](const QString& appId) {
            if (auto* bridge = makineai::CoreBridge::instance()) {
                QVariantMap detail = manifestSync->getPackageDetail(appId);
                QJsonDocument doc(QJsonObject::fromVariantMap(detail));
                bridge->enrichPackageFromJson(appId, doc.toJson(QJsonDocument::Compact));
            }
        });

    // Sync catalog index (fast, ~10ms) — provides metadata for QML creation.
    // GameService::initialize() deferred to after first frame render to avoid
    // expired QML timer cascades during processEvents.
    manifestSync->syncCatalog();
    logToFile(QString("ManifestSync::syncCatalog() completed at %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 8: QML engine loading (heaviest single operation) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Aray\u00FCz derleniyor...");
#endif
    logToFile(QString("Phase 8 (QML load start) at %1 ms").arg(startupTimer.elapsed()));

    // Use QQmlComponent for incremental loading — keeps splash alive
    QQmlComponent mainComponent(&engine);

    {
        MAKINE_ZONE_NAMED("QML::loadFromModule");
        mainComponent.loadFromModule("MakineAI", "Main");

        // Pump events while QML compiles (first launch is slow)
#ifdef Q_OS_WIN
        while (mainComponent.isLoading()) {
            splash.pumpMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
        }
#endif
    }

    if (mainComponent.isError()) {
        for (const auto& error : mainComponent.errors())
            logToFile(QString("QML Error: %1").arg(error.toString()));
#ifdef Q_OS_WIN
        splash.close();
#endif
        makineai::CrashReporter::shutdown();
        return -1;
    }

    logToFile(QString("Phase 8 (QML compiled) at %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 9: Create root window =====
    // Note: QQmlIncubator::Asynchronous does NOT work for root objects
    // (Qt forces synchronous creation without an incubation controller).
    // The 4+ seconds here is Qt framework overhead (module loading, type registration,
    // shader compilation). Optimize by reducing QML module surface, not creation strategy.
    QObject* rootObject = nullptr;
    {
        MAKINE_ZONE_NAMED("QML::createRootObject");
#ifdef Q_OS_WIN
        splash.setStatus(L"Pencere olu\u015Fturuluyor...");
        splash.pumpMessages();
#endif
        rootObject = mainComponent.create();
    }
    if (!rootObject) {
        logToFile("ERROR: Failed to create root object!");
#ifdef Q_OS_WIN
        splash.close();
#endif
        makineai::CrashReporter::shutdown();
        return -1;
    }
    engine.setObjectOwnership(rootObject, QQmlEngine::JavaScriptOwnership);

    logToFile(QString("QML loaded + created in %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 10: Pre-render + finalize =====
    makineai::CrashReporter::addBreadcrumb("startup", "Phase 10: Pre-render + finalize");
#ifdef Q_OS_WIN
    splash.setStatus(L"Son haz\u0131rl\u0131klar yap\u0131l\u0131yor...");
    splash.pumpMessages();
#endif

    // Release GPU resources when window is hidden/minimized (reclaimed on show)
    auto *window = qobject_cast<QQuickWindow*>(rootObject);
#ifdef Q_OS_WIN
    QMetaObject::Connection firstFrameConn;
#endif
    if (window) {
        // Responsive sizing + centered positioning via Win32
        {
            RECT workArea{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
            int waW = workArea.right - workArea.left;
            int waH = workArea.bottom - workArea.top;

            // Proportional: 1280×1080 on 3840×2160 = 1/3 width, 1/2 height
            int w = waW / 3;
            int h = waH / 2;

            // Screen center point
            int cx = workArea.left + waW / 2;
            int cy = workArea.top + waH / 2;

            // Window top-left so that window center == screen center
            int x = cx - w / 2;
            int y = cy - h / 2;

            HWND hwnd = reinterpret_cast<HWND>(window->winId());
            MoveWindow(hwnd, x, y, w, h, TRUE);

            // Apply OS-appropriate window style (Mica on W11, dark frame on W10)
            configureWindowStyle(hwnd);
        }

        window->setPersistentGraphics(false);
        window->setPersistentSceneGraph(false);

        // Graphics configuration: pipeline cache
        {
            QQuickGraphicsConfiguration gfxConfig;

            // Pipeline cache via API (supplements env vars set earlier)
            QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                + "/pipeline_cache.bin";
            gfxConfig.setPipelineCacheSaveFile(cachePath);
            gfxConfig.setPipelineCacheLoadFile(cachePath);

            window->setGraphicsConfiguration(gfxConfig);
        }

        // Preload Settings page behind splash (~1.2s, hidden from user)
        splash.setStatus(L"Sayfalar haz\u0131rlan\u0131yor...");
        splash.pumpMessages();
        {
            MAKINE_ZONE_NAMED("Preload::SettingsScreen");
            rootObject->setProperty("_settingsPreload", true);
        }
        logToFile(QString("Settings preloaded at %1 ms").arg(startupTimer.elapsed()));

#ifdef Q_OS_WIN
        // Release GPU resources + trim working set when hidden/minimized
        QObject::connect(window, &QWindow::visibilityChanged, [window](QWindow::Visibility v) {
            if (v == QWindow::Hidden || v == QWindow::Minimized) {
                window->releaseResources();
                EmptyWorkingSet(GetCurrentProcess());
            }
        });

        logToFile(QString("Phase 10 (first-frame setup) at %1 ms").arg(startupTimer.elapsed()));

        // Close splash on FIRST rendered frame, then dispatch game library load.
        //
        // Why not processEvents here? Any processEvents() call triggers expired
        // QML timers (from createRootObject), which cascade into 4+ seconds of
        // synchronous model resets. Instead, we let QML start with empty data
        // (loading state), close splash on the first rendered frame, then
        // populate data progressively via the event loop.
        firstFrameConn = QObject::connect(window, &QQuickWindow::frameSwapped, &app,
            [&firstFrameConn, &splash, &startupTimer, gameService]() {
                // Guard: render thread may queue multiple frameSwapped before
                // the first callback runs (threaded loop pipelining).
                static bool done = false;
                if (done) return;
                done = true;

                MAKINE_ZONE_NAMED("Startup::firstFrame");
                QObject::disconnect(firstFrameConn);
                splash.waitMinimumDisplay(200);
                logToFile(QString("First frame rendered, closing splash at %1 ms")
                              .arg(startupTimer.elapsed()));
                splash.close();
                // Dispatch game library loading — results arrive on subsequent
                // event loop iterations, populating QML progressively.
                gameService->initialize();
            }, Qt::QueuedConnection);

        window->requestUpdate();
        logToFile(QString("Phase 10 (callback registered) at %1 ms").arg(startupTimer.elapsed()));
#endif

        // Tracy: frame boundary marker (one FrameMark per rendered frame)
        QObject::connect(window, &QQuickWindow::afterRendering, window, []() {
            MAKINE_FRAME;
        }, Qt::DirectConnection);

#ifdef MAKINEAI_DEV_TOOLS
        // Dev-only frame timer: high-precision render pipeline metrics
        auto* frameTimer = new FrameTimer(&app);
        engine.rootContext()->setContextProperty("FrameTimer", frameTimer);
        frameTimer->attachToWindow(window);

        // Dump frame stats on exit (integrates with PerfReporter)
        QObject::connect(&app, &QCoreApplication::aboutToQuit, frameTimer, &FrameTimer::dumpStats);

        // Reset frame timer after startup settles — clears the initial jank spike
        // so ongoing metrics reflect actual runtime performance, not first-frame shaders
        QTimer::singleShot(kStartupSettleMs, frameTimer, &FrameTimer::reset);

        // Scene profiler: transition, interaction, dialog, animation tracking
        auto* sceneProfiler = new SceneProfiler(&app);
        engine.rootContext()->setContextProperty("SceneProfiler", sceneProfiler);

        // Memory profiler: working set, image cache monitoring
        auto* memoryProfiler = new MemoryProfiler(&app);
        memoryProfiler->setImageCacheManager(imageCache);
        engine.rootContext()->setContextProperty("MemoryProfiler", memoryProfiler);

        // Dump profiler reports on exit
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [sceneProfiler, memoryProfiler, imageCache]() {
#ifdef MAKINEAI_PERF_ACTIVE
            auto& reporter = PerfReporter::instance();
            reporter.addCustomSection(QStringLiteral("scene"), sceneProfiler->sceneReport());
            reporter.addCustomSection(QStringLiteral("memory"), memoryProfiler->memoryReport());

            // Image cache stats
            QJsonObject imgObj;
            imgObj[QStringLiteral("cache_count")] = imageCache->cachedImageCount();
            imgObj[QStringLiteral("cache_bytes")] = imageCache->cachedImageBytes();
            QVariantMap stats = imageCache->imageStats();
            imgObj[QStringLiteral("total_downloads")] = stats.value(QStringLiteral("downloads")).toInt();
            imgObj[QStringLiteral("cache_hits")] = stats.value(QStringLiteral("cacheHits")).toInt();
            imgObj[QStringLiteral("hit_rate")] = stats.value(QStringLiteral("hitRate")).toDouble();
            imgObj[QStringLiteral("queue_peak")] = stats.value(QStringLiteral("queuePeak")).toInt();
            reporter.addCustomSection(QStringLiteral("images"), imgObj);
#endif
        });
#endif
    }

    // Fallback: close splash if window creation failed (normal path uses frameSwapped)
#ifdef Q_OS_WIN
    if (!window) {
        splash.close();
        gameService->initialize();
    }
#endif

    // Trim working set after startup settles (DLL init, type registration, QML
    // compilation pages are no longer needed). Hot pages fault back in microseconds.
#ifdef Q_OS_WIN
    QTimer::singleShot(kStartupSettleMs, [&]() {
        // Compact heaps: merge free blocks, release unused pages to OS
        HANDLE heaps[32];
        DWORD count = GetProcessHeaps(32, heaps);
        for (DWORD i = 0; i < count; ++i) {
            HeapCompact(heaps[i], 0);
        }

        // Release working set: cold pages (DLL init, QML compile) go to standby list
        EmptyWorkingSet(GetCurrentProcess());
        logToFile("Heap compacted + working set trimmed");
    });
#endif

    MAKINE_THREAD_NAME("Main/UI");

#ifdef MAKINEAI_PERF_ACTIVE
    makineai::PerfReporter::instance().setMainThread();

    // --profile-duration=N: auto-quit after N seconds (automated profiling)
    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);
        if (arg.startsWith("--profile-duration=")) {
            int secs = arg.mid(19).toInt();
            if (secs > 0) {
                logToFile(QString("Profile mode: auto-quit in %1 seconds").arg(secs));
                QTimer::singleShot(secs * 1000, &app, [&app]() {
                    logToFile("Profile duration reached, exiting...");
                    // Close all windows first (bypasses setQuitOnLastWindowClosed)
                    for (auto* w : QGuiApplication::topLevelWindows())
                        w->close();
                    app.exit(0);
                });
            }
        }
    }

    // Dump performance report on exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        QString reportPath = makineai::AppPaths::perfReportFile();
        makineai::PerfReporter::instance().dumpReport(reportPath);
        logToFile(QString("Performance report saved to: %1").arg(reportPath));
    });
#endif

    logToFile(QString("Total startup: %1 ms").arg(startupTimer.elapsed()));
    logToFile("Entering event loop...");

    // Anti-RE: periodic re-checks once event loop is running
    makineai::protection::schedulePeriodicChecks();

    int exitCode = app.exec();
    makineai::CrashReporter::shutdown();
    return exitCode;
}
