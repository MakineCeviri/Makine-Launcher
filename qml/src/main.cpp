/**
 * @file main.cpp
 * @brief Makine Launcher QML Application Entry Point
 * @copyright (c) 2026 MakineCeviri Team
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
#include <QJsonDocument>
#include <QTimer>
#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUrlQuery>
#include "services/profiler.h"
#include "services/rendergovernor.h"
#include "services/crashreporter.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcApp, "makine.app")

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet
#include <dwmapi.h>    // DwmExtendFrameIntoClientArea
#include <QAbstractNativeEventFilter>
#include <cmath>
#include <atomic>
#include <mutex>

// Eliminate DWM non-client border on frameless windows.
// Without this, Windows adds an asymmetric invisible border (~8px left, ~0px right)
// that shifts the contentItem and makes centered content appear off-center.
class FramelessFilter : public QAbstractNativeEventFilter {
public:
    static constexpr double kAspectRatio = 900.0 / 620.0; // minWidth / minHeight

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);

        if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
            *result = 0;
            return true;
        }

        // Native aspect ratio lock — zero QML bindings, zero stutter
        if (msg->message == WM_SIZING) {
            auto* rect = reinterpret_cast<RECT*>(msg->lParam);
            int w = rect->right - rect->left;
            int h = rect->bottom - rect->top;

            switch (msg->wParam) {
            case WMSZ_LEFT: case WMSZ_RIGHT:
            case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:
            case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOMRIGHT:
                rect->bottom = rect->top + static_cast<int>(w / kAspectRatio);
                break;
            case WMSZ_TOP: case WMSZ_BOTTOM:
                rect->right = rect->left + static_cast<int>(h * kAspectRatio);
                break;
            }
            *result = TRUE;
            return true;
        }

        return false;
    }
};

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
// 440x240, rounded corners, logo, gradient bars, loading dots
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
    // Brand gradient palette (Makine official colors)
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
        float peakAlpha = 0.12f + breath * 0.13f; // range: 0.12 -> 0.25

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
            // Version from CMake — MAKINE_APP_VERSION is narrow, convert to wide
            auto _verStr = QStringLiteral("v" MAKINE_APP_VERSION);
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
        wc.lpszClassName = L"MakineSplash";
        wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 15));
        wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
        RegisterClassW(&wc);

        constexpr int w = 440, h = 240;
        int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

        self->m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"MakineSplash", L"",
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
    QString logDir = makine::AppPaths::logsDir();
    QDir().mkpath(logDir);
    return makine::AppPaths::debugLog();
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

// Anti-RE protection (no-op in debug builds)
#include "services/appprotection.h"

// Backend services
#include "services/gameservice.h"
#include "services/supportedgamesmodel.h"
#include "services/catalogproxymodel.h"
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/pluginmanager.h"
#include "services/ocrcontroller.h"
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
#include "services/translationstatemanager.h"
#include "services/installflowservice.h"
#ifdef MAKINE_DEV_TOOLS
#include "services/frametimer.h"
#include "services/sceneprofiler.h"
#include "services/memoryprofiler.h"
#endif

// Bring service types into scope for helper function signatures.
// NOLINT: spdlog is not used in this file — no ADL collision risk.
using namespace makine;

// -----------------------------------------------------------------------------
// Forward declarations of helper functions
// -----------------------------------------------------------------------------

// Set Qt environment variables before QGuiApplication is constructed.
static void configureQtEnvironment();

// Single-instance guard. Returns true if this process acquired the slot.
// isPostUpdate: allows extra retries when the previous process is still exiting.
static bool acquireSingleInstance(bool isPostUpdate);

// Configure QGuiApplication properties, fonts, and pipeline-cache env vars.
static void configureApplication(QGuiApplication& app);

// Register QML types and set engine context properties needed before services.
static void configureEngine(QQmlApplicationEngine& engine);

// Phases 1-7: instantiate all backend services, register as context properties,
// and return pointers needed by later phases via out-parameters.
static void createServices(
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    bool isPostUpdate,
    const QElapsedTimer& startupTimer,
#ifdef Q_OS_WIN
    SplashWindow& splash,
#endif
    GameService*& outGameService,
    ManifestSyncService*& outManifestSync,
    ImageCacheManager*& outImageCache,
    SystemTrayManager*& outTrayManager,
    OperationJournal*& outJournal,
    ProcessScanner*& outProcessScanner);

// Wire inter-service signal/slot connections.
static void wireSignals(
    QGuiApplication& app,
    GameService* gameService,
    ManifestSyncService* manifestSync,
    SystemTrayManager* trayManager,
    ProcessScanner* processScanner);

// Log startup environment diagnostics (graphics backend, Qt version, etc.)
// and attach QML warning/error handlers to the engine.
static void logStartupDiagnostics(QGuiApplication& app, QQmlApplicationEngine& engine);

// Phase 10: configure the root QQuickWindow (sizing, style, pipeline cache,
// first-frame splash teardown, dev-tool attachment).
static void setupRootWindow(
    QObject* rootObject,
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    GameService* gameService,
    ImageCacheManager* imageCache,
    const QElapsedTimer& startupTimer
#ifdef Q_OS_WIN
    , SplashWindow& splash
#endif
);

// Schedule post-startup heap compact + working-set release.
static void scheduleMemoryTrim();

// Wire performance reporting for MAKINE_PERF_ACTIVE builds.
static void setupPerfReporting(QGuiApplication& app, int argc, char* argv[]);

// -----------------------------------------------------------------------------
// Helper function implementations
// -----------------------------------------------------------------------------

static void configureQtEnvironment()
{
    // === HIGH-DPI ROUNDING POLICY ===
    // PassThrough preserves fractional DPI scale (e.g. 125%, 150%) instead
    // of rounding to integer. This prevents layout jumps and font metric
    // shifts when the window crosses between monitors with different DPI
    // (e.g. 4K @ 150% ↔ 1080p @ 100%). Must be set before QGuiApplication.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // === RENDER LOOP ===
    // "threaded" = render on separate thread, overlaps CPU+GPU work.
    // "basic" = single-threaded, best idle efficiency but blocks main thread during render.
    qputenv("QSG_RENDER_LOOP", "threaded");

    // === VSYNC (60 FPS CAP) ===
    // swapInterval=1 locks to monitor refresh (60Hz=60fps, 144Hz capped by governor)
    {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setSwapInterval(1);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

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

    // Set org/app names early so QStandardPaths resolves to the correct directory
    // (MakineCeviri/Makine-Launcher) before any path queries.
    QCoreApplication::setOrganizationName("MakineCeviri");
    QCoreApplication::setApplicationName("Makine-Launcher");

    // === QML DISK CACHE ===
    // Pin cache to a stable path so it survives Qt minor version updates.
    // Without this, Qt writes to %TEMP% which may be cleaned between sessions,
    // forcing full QML recompile on every cold start (~200-400ms penalty).
    {
        const QString cacheBase = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        if (!cacheBase.isEmpty())
            qputenv("QML_DISK_CACHE_PATH", (cacheBase + "/qml_cache").toUtf8());
    }

    // Disable RHI debug/validation layers (saves ~10 MB + CPU)
    qputenv("QSG_RHI_DEBUG_LAYER", "0");

    // === MEMORY OPTIMIZATIONS ===
    // Texture atlas: 512x512 = 1 MB/atlas (default 2048x2048 = 16 MB)
    qputenv("QSG_ATLAS_WIDTH", "512");
    qputenv("QSG_ATLAS_HEIGHT", "512");

    // V4 JS engine: aggressive GC to free unused JS heap sooner
    qputenv("QV4_MM_AGGRESSIVE_GC", "1");

    // UI Scale: compute effective scale factor before QGuiApplication.
    //
    // Reference logical size: 1280×800.  On screens too small to fit the
    // reference at native DPI, "auto" mode shrinks to fit (floor 0.80).
    // compact = 0.92×, large = 1.12×, auto = adaptive (≤ 1.0).
    // The numeric factor is persisted so configureWindowOnReady() can
    // size the window to the exact matching physical resolution.
    {
        QSettings settings("MakineCeviri", "Makine-Launcher");
        QString uiScale = settings.value("appearance/uiScale", "auto").toString();

        double effectiveScale = 1.0;

        if (uiScale == "manual") {
            effectiveScale = 1.0;  // No scaling — user controls resolution freely
        } else if (uiScale == "compact") {
            effectiveScale = 0.92;
        } else if (uiScale == "large") {
            effectiveScale = 1.12;
        } else {
#ifdef Q_OS_WIN
            // auto: adaptive scale based on logical work area
            // PerMonitorV2: SystemParametersInfo returns logical pixels (DPI-adjusted)
            // No nativeDpr needed — Qt handles physical-to-logical mapping
            RECT wa{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);

            double logicalW = (wa.right - wa.left) * 0.92;
            double logicalH = (wa.bottom - wa.top) * 0.92;
            double fitW = logicalW / 1280.0;
            double fitH = logicalH / 800.0;
            double fitScale = qMin(fitW, fitH);

            if (fitScale < 1.0)
                effectiveScale = qMax(fitScale, 0.80);  // shrink for small screens
            else if (fitScale > 1.08)
                effectiveScale = 1.0 + (fitScale - 1.0) * 0.35;  // gentle grow for large screens
#endif
        }

        if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR") && effectiveScale != 1.0)
            qputenv("QT_SCALE_FACTOR", QByteArray::number(effectiveScale, 'f', 3));

        settings.setValue("appearance/_appliedUiScale", uiScale);
        settings.setValue("appearance/_appliedScaleFactor", effectiveScale);
    }
}

static bool acquireSingleInstance(bool isPostUpdate)
{
    // When launched with --post-update, the old process may still be exiting.
    // Retry a few times to allow the kernel to release the shared memory handle.
    const int maxRetries = isPostUpdate ? 10 : 1;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QSharedMemory cleanupMemory("MakineLauncher_SingleInstance_Guard");
        if (cleanupMemory.attach())
            cleanupMemory.detach();

        QSharedMemory testGuard("MakineLauncher_SingleInstance_Guard");
        if (testGuard.create(1))
            return true;

        if (attempt + 1 < maxRetries)
#ifdef Q_OS_WIN
            Sleep(500);  // Wait 500ms before retry
#else
            QThread::msleep(500);
#endif
    }

    return false;
}

static void configureApplication(QGuiApplication& app)
{
    // Set dynamically after SettingsManager init (based on minimizeToTray)
    app.setApplicationName("Makine-Launcher");
    app.setApplicationDisplayName(QStringLiteral("Makine \u00C7eviri - Makine Launcher"));
    app.setApplicationVersion(MAKINE_APP_VERSION);
    app.setOrganizationName("MakineCeviri");
    app.setOrganizationDomain("makineceviri.org");
    app.setWindowIcon(QIcon(":/qt/qml/MakineLauncher/resources/images/logo.png"));
    QQuickStyle::setStyle("Basic");

    int interRegular = QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Bold.ttf");

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

    // Thread pool: 4 threads balances async throughput vs memory (4 MB stack total).
    // Default CPU count (8) is excessive; 2 was too constrained during startup
    // when async Loaders + image downloads + CDN fetch all compete.
    QThreadPool::globalInstance()->setMaxThreadCount(4);
}

static void configureEngine(QQmlApplicationEngine& engine)
{
    // Dev tools availability (must be set BEFORE QML creation)
#ifdef MAKINE_DEV_TOOLS
    engine.rootContext()->setContextProperty("devToolsEnabled", true);
#else
    engine.rootContext()->setContextProperty("devToolsEnabled", false);
#endif


    // Register model types for QML.
    // Manual registration — works in both shared and static Qt builds.
    // (QML_ELEMENT/QML_SINGLETON relies on linker keeping registration code,
    //  which --gc-sections strips in static builds.)
    qmlRegisterUncreatableType<makine::SupportedGamesModel>("MakineLauncher", 1, 0,
        "SupportedGamesModel", "Use GameService.supportedGamesModel");
    qmlRegisterType<makine::CatalogProxyModel>("MakineLauncher", 1, 0, "CatalogProxyModel");
}

static void createServices(
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    bool isPostUpdate,
    const QElapsedTimer& startupTimer,
#ifdef Q_OS_WIN
    SplashWindow& splash,
#endif
    GameService*& outGameService,
    ManifestSyncService*& outManifestSync,
    ImageCacheManager*& outImageCache,
    SystemTrayManager*& outTrayManager,
    OperationJournal*& outJournal,
    ProcessScanner*& outProcessScanner)
{
    // ===== Phase 1: Directory structure + configuration =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 1: Directory structure + configuration");
#ifdef Q_OS_WIN
    splash.setStatus(L"Dizin yap\u0131s\u0131 haz\u0131rlan\u0131yor...");
#endif
    AppPaths::migrateFromLegacyRoot();   // MakineLauncher → MakineCeviri/Makine-Launcher
    AppPaths::ensureDirectories();
    AppPaths::migrateFromFlatLayout();
    AppPaths::cleanupLegacyDirs();       // Remove empty legacy dirs

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

    // When minimizeToTray is off, closing the last window should quit the app
    app.setQuitOnLastWindowClosed(!settingsManager->minimizeToTray());
    QObject::connect(settingsManager, &SettingsManager::minimizeToTrayChanged, &app,
                     [&app, settingsManager]() {
        app.setQuitOnLastWindowClosed(!settingsManager->minimizeToTray());
    });

    // ===== Phase 2: Image cache =====
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00F6rsel \u00F6nbelle\u011Fi ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* imageCache = new ImageCacheManager(&app);
    engine.rootContext()->setContextProperty("ImageCache", imageCache);
    outImageCache = imageCache;

    // ===== Phase 2.5: Manifest sync (loads cached index, starts background sync) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Katalog haz\u0131rlan\u0131yor...");
#endif
    auto* manifestSync = new ManifestSyncService(&app);
    engine.rootContext()->setContextProperty("ManifestSync", manifestSync);
    outManifestSync = manifestSync;
    // syncCatalog() called in Phase 7.5 — loads catalog index before QML creation

    auto* translationDownloader = new TranslationDownloader(&app);
    translationDownloader->setDataPath(makine::AppPaths::packagesDir());
    engine.rootContext()->setContextProperty("TranslationDownloader", translationDownloader);

    auto* translationStateManager = new makine::TranslationStateManager(&app);
    engine.rootContext()->setContextProperty("TranslationStateManager", translationStateManager);

    // UpdateService registered as singleton instance in Phase 7 (below)

    // ===== Phase 3: Game library (construction only — data loads after QML) =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 3: Game library construction");
#ifdef Q_OS_WIN
    splash.setStatus(L"Oyun k\u00FCt\u00FCphanesi haz\u0131rlan\u0131yor...");
#endif
    auto* gameService = new GameService(&app);
    gameService->setManifestSync(manifestSync);
    gameService->setImageCache(imageCache);
    engine.rootContext()->setContextProperty("GameService", gameService);
    outGameService = gameService;
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
        qCDebug(lcApp) << "OperationJournal: recovering from interrupted operation...";
        journal->recover();
    }
    outJournal = journal;
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
    outProcessScanner = processScanner;

    // ===== Phase 6: Security + integrity =====
    auto* integrityService = new IntegrityService(&app);
    engine.rootContext()->setContextProperty("IntegrityService", integrityService);

    auto* batchService = new BatchOperationService(&app);
    engine.rootContext()->setContextProperty("BatchOperationService", batchService);

    // ===== Phase 6b: Plugin system (dev builds only) =====
#ifdef MAKINE_DEV_TOOLS
    auto* pluginManager = new PluginManager(&app);
    pluginManager->discoverPlugins();
    pluginManager->loadEnabledPlugins();
    pluginManager->checkForUpdates();
    pluginManager->fetchCommunityPlugins();
    engine.rootContext()->setContextProperty("PluginManager", pluginManager);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, pluginManager, &PluginManager::shutdownAll);

    auto* ocrController = new OcrController(pluginManager, &app);
    engine.rootContext()->setContextProperty("OcrController", ocrController);
#else
    engine.rootContext()->setContextProperty("PluginManager", nullptr);
    engine.rootContext()->setContextProperty("OcrController", nullptr);
#endif

    // ===== Phase 7: Update service + system tray =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 7: Update service + system tray");
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00FCncelleme servisi haz\u0131rlan\u0131yor...");
#endif
    auto* updateService = UpdateService::create(nullptr, nullptr);
    updateService->setParent(&app);
    // Singleton instance: exposes BOTH the instance AND Q_ENUM(State) values to QML.
    // (setContextProperty only exposes the instance — enum constants resolve to undefined)
    qmlRegisterSingletonInstance("MakineLauncher", 1, 0, "UpdateService", updateService);

    // Startup update check — once, async, unless we just updated
    if (!isPostUpdate)
        updateService->check();

#ifdef Q_OS_WIN
    splash.setStatus(L"Sistem tepsisi yap\u0131land\u0131r\u0131l\u0131yor...");
#endif
    auto* trayManager = new SystemTrayManager(&app);
    trayManager->setIcon(app.windowIcon());
    trayManager->show();
    engine.rootContext()->setContextProperty("SystemTrayManager", trayManager);
    outTrayManager = trayManager;

    // Wire journal to CoreBridge
    CoreBridge::instance()->setJournal(journal);
    engine.rootContext()->setContextProperty("CoreBridge", CoreBridge::instance());

    // Install flow state machine (depends on GameService, TranslationDownloader, ManifestSync, CoreBridge)
    auto* installFlow = new makine::InstallFlowService(
        gameService, translationDownloader, manifestSync,
        CoreBridge::instance(), &app);
    engine.rootContext()->setContextProperty("InstallFlowService", installFlow);

    // B2-03: route TranslationDownloader signals natively so the install
    // pipeline cannot stall when the QML controller is reloaded or GC'd.
    QObject::connect(translationDownloader, &makine::TranslationDownloader::packageReady,
                     installFlow, [installFlow](const QString& appId, const QString& /*dirName*/) {
                         installFlow->onDownloadReady(appId);
                     });
    QObject::connect(translationDownloader, &makine::TranslationDownloader::downloadError,
                     installFlow, &makine::InstallFlowService::onDownloadFailed);
    QObject::connect(translationDownloader, &makine::TranslationDownloader::downloadCancelled,
                     installFlow, [installFlow](const QString& appId) {
                         installFlow->onDownloadFailed(appId, QString{});
                     });

    logToFile(QString("Services initialized in %1 ms").arg(startupTimer.elapsed()));
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif
}

static void wireSignals(
    QGuiApplication& app,
    GameService* gameService,
    ManifestSyncService* manifestSync,
    SystemTrayManager* trayManager,
    ProcessScanner* processScanner)
{
    // Rebuild process map when game library scan completes.
    // PackageManager is lazy-init (created during first scanAllLibraries),
    // so we inject it + rebuild on every scanCompleted.
    QObject::connect(gameService, &GameService::scanCompleted,
                     processScanner, [processScanner]() {
        processScanner->setPackageManager(makine::CoreBridge::instance()->packageManager());
        processScanner->rebuildProcessMap();
    });

    // Tray quit -> app quit directly (bypass QML round-trip)
    QObject::connect(trayManager, &SystemTrayManager::quitRequested,
                     &app, &QCoreApplication::quit);

    // Clean shutdown: remove tray icon and drain thread pool before exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [trayManager]() {
        trayManager->hide();
        QThreadPool::globalInstance()->clear();
        QThreadPool::globalInstance()->waitForDone(3000);
    });

    // Connect ManifestSync signals BEFORE syncing,
    // so catalogReady/packageDetailReady are never missed.
    QObject::connect(manifestSync, &makine::ManifestSyncService::catalogReady,
        gameService, []() {
            if (auto* bridge = makine::CoreBridge::instance())
                bridge->refreshPackageManifest();
        });
    QObject::connect(manifestSync, &makine::ManifestSyncService::packageDetailReady,
        gameService, [manifestSync](const QString& appId) {
            if (auto* bridge = makine::CoreBridge::instance()) {
                QVariantMap detail = manifestSync->getPackageDetail(appId);
                QJsonDocument doc(QJsonObject::fromVariantMap(detail));
                bridge->enrichPackageFromJson(appId, doc.toJson(QJsonDocument::Compact));
            }
        });
}

static void logStartupDiagnostics(QGuiApplication& app, QQmlApplicationEngine& engine)
{
    logToFile("=== Makine Launcher Starting ===");
    logToFile(QString("App version: %1").arg(app.applicationVersion()));
    logToFile(QString("Qt version: %1").arg(qVersion()));
    logToFile(QString("Log file: %1").arg(getLogFilePath()));
    logToFile(QString("QSG_RENDER_LOOP: %1").arg(qEnvironmentVariable("QSG_RENDER_LOOP")));
    {
        QSettings settings("MakineCeviri", "Makine-Launcher");
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
}

static void setupRootWindow(
    QObject* rootObject,
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    GameService* gameService,
    ImageCacheManager* imageCache,
    const QElapsedTimer& startupTimer
#ifdef Q_OS_WIN
    , SplashWindow& splash
#endif
)
{
    auto* window = qobject_cast<QQuickWindow*>(rootObject);
    if (!window) {
        // Fallback: no window — close splash and initialize game library immediately
#ifdef Q_OS_WIN
        splash.close();
#endif
        gameService->initialize();
        return;
    }

#ifdef Q_OS_WIN
    // DPI-aware window sizing + centered positioning via Win32
    //
    // Physical window size = reference (1280×800) × native DPI × effective scale.
    // The effective scale factor was computed in configureQtEnvironment() and
    // persisted to QSettings — it already accounts for auto-fit on small screens,
    // compact/large user preference, and the 0.80 floor.
    {
        // Window sizing is now handled in QML (Main.qml width/height/x/y).
        // Only apply DWM style here — no MoveWindow to avoid resize cascade.
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        configureWindowStyle(hwnd);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
#endif

    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);

    // Graphics configuration: pipeline cache via API
    // (supplements env vars set earlier in configureApplication)
    {
        QQuickGraphicsConfiguration gfxConfig;
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                            + "/pipeline_cache.bin";
        gfxConfig.setPipelineCacheSaveFile(cachePath);
        gfxConfig.setPipelineCacheLoadFile(cachePath);
        window->setGraphicsConfiguration(gfxConfig);
    }

#ifdef Q_OS_WIN
    // Release GPU resources + trim working set when hidden/minimized
    QObject::connect(window, &QWindow::visibilityChanged, [window](QWindow::Visibility v) {
        if (v == QWindow::Hidden || v == QWindow::Minimized) {
            window->releaseResources();
            EmptyWorkingSet(GetCurrentProcess());
        }
    });

    // FPS governor: active (60fps vsync) → idle (on-demand) → background (0fps)
    auto* renderGovernor = new RenderGovernor(window, &app);
    engine.rootContext()->setContextProperty("RenderGovernor", renderGovernor);

    logToFile(QString("Phase 10 (first-frame setup) at %1 ms").arg(startupTimer.elapsed()));

    // Close splash on FIRST rendered frame, then dispatch game library load.
    //
    // Why not processEvents here? Any processEvents() call triggers expired
    // QML timers (from createRootObject), which cascade into 4+ seconds of
    // synchronous model resets. Instead, we let QML start with empty data
    // (loading state), close splash on the first rendered frame, then
    // populate data progressively via the event loop.
    QMetaObject::Connection firstFrameConn;
    firstFrameConn = QObject::connect(window, &QQuickWindow::frameSwapped, &app,
        [firstFrameConn, &splash, &startupTimer, gameService]() mutable {
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

    // Request first frame BEFORE preloading Settings — ensures the render
    // pipeline starts immediately.
    window->requestUpdate();
    logToFile(QString("Phase 10 (callback registered) at %1 ms").arg(startupTimer.elapsed()));

    // Preload Settings page AFTER requestUpdate — first frame render has priority.
    // The async Loader runs on the thread pool, so this won't block the first frame,
    // but ordering ensures the render pipeline gets the earliest possible start.
    splash.setStatus(L"Sayfalar haz\u0131rlan\u0131yor...");
    splash.pumpMessages();
    {
        MAKINE_ZONE_NAMED("Preload::SettingsScreen");
        rootObject->setProperty("_settingsPreload", true);
    }
    logToFile(QString("Settings preloaded at %1 ms").arg(startupTimer.elapsed()));
#endif

    // Tracy: frame boundary marker (one FrameMark per rendered frame)
    QObject::connect(window, &QQuickWindow::afterRendering, window, []() {
        MAKINE_FRAME;
    }, Qt::DirectConnection);

#ifdef MAKINE_DEV_TOOLS
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
#ifdef MAKINE_PERF_ACTIVE
        auto& reporter = makine::PerfReporter::instance();
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
#else
    Q_UNUSED(imageCache)
    Q_UNUSED(engine)
#endif
}

static void scheduleMemoryTrim()
{
    // Trim working set after startup settles (DLL init, type registration, QML
    // compilation pages are no longer needed). Hot pages fault back in microseconds.
#ifdef Q_OS_WIN
    QTimer::singleShot(kStartupSettleMs, []() {
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
}

static void setupPerfReporting(QGuiApplication& app, int argc, char* argv[])
{
#ifdef MAKINE_PERF_ACTIVE
    makine::PerfReporter::instance().setMainThread();

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
        QString reportPath = makine::AppPaths::perfReportFile();
        makine::PerfReporter::instance().dumpReport(reportPath);
        logToFile(QString("Performance report saved to: %1").arg(reportPath));
    });
#else
    Q_UNUSED(app)
    Q_UNUSED(argc)
    Q_UNUSED(argv)
#endif
}

// -----------------------------------------------------------------------------
// main() — high-level orchestrator
// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    configureQtEnvironment();

    // Parse command-line flags before QGuiApplication
    bool isPostUpdate = false;
    bool startMinimized = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--post-update") == 0) {
            isPostUpdate = true;
        } else if (strcmp(argv[i], "--minimized") == 0) {
            startMinimized = true;
        }
    }

    // Single-instance guard (must run before QGuiApplication on Windows)
    if (!acquireSingleInstance(isPostUpdate)) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr,
            L"Makine Launcher zaten \u00e7al\u0131\u015f\u0131yor.\n\n"
            L"L\u00fctfen sistem tepsisindeki simgeyi kontrol edin "
            L"veya g\u00f6rev y\u00f6neticisinden kapat\u0131n.",
            L"Makine Launcher",
            MB_OK | MB_ICONWARNING);
#endif
        return 0;
    }

    // Persistent guard — lives for the lifetime of the process
    QSharedMemory singleInstanceGuard("MakineLauncher_SingleInstance_Guard");
    singleInstanceGuard.create(1);

    QGuiApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Eliminate asymmetric DWM border on frameless window
    static FramelessFilter framelessFilter;
    app.installNativeEventFilter(&framelessFilter);
#endif

    // === Phase 0: Crash reporting (as early as possible after QApp) ===
    makine::CrashReporter::initialize();
    makine::CrashReporter::installQtMessageHandler();

    // Anti-RE: run all checks before anything else (no-op in debug builds)
    makine::protection::initialize();

#ifdef Q_OS_WIN
    SplashWindow splash;
    // Load Inter font for splash (before splash thread starts)
    bool interLoaded = false;
    {
        QFile fontFile(":/qt/qml/MakineLauncher/resources/fonts/Inter-Medium.ttf");
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
        QImage logoImg(":/qt/qml/MakineLauncher/resources/images/logo.png");
        if (!logoImg.isNull())
            splash.setLogo(logoImg.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    splash.show();
#endif

    configureApplication(app);

    QQmlApplicationEngine engine;
    configureEngine(engine);

    QElapsedTimer startupTimer;
    startupTimer.start();

    // Phases 1-7: create all backend services
    GameService* gameService = nullptr;
    ManifestSyncService* manifestSync = nullptr;
    ImageCacheManager* imageCache = nullptr;
    SystemTrayManager* trayManager = nullptr;
    OperationJournal* journal = nullptr;
    ProcessScanner* processScanner = nullptr;

    createServices(app, engine, isPostUpdate, startupTimer,
#ifdef Q_OS_WIN
        splash,
#endif
        gameService, manifestSync, imageCache, trayManager, journal, processScanner);

    // Wire inter-service signal/slot connections
    wireSignals(app, gameService, manifestSync, trayManager, processScanner);

    // Attach QML warning handlers and log environment info
    logStartupDiagnostics(app, engine);

    // ===== Phase 7.5: Sync catalog index =====
    // Signals wired above — safe to sync now so catalogReady is never missed.
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
        mainComponent.loadFromModule("MakineLauncher", "Main");

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
        makine::CrashReporter::shutdown();
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
        makine::CrashReporter::shutdown();
        return -1;
    }
    engine.setObjectOwnership(rootObject, QQmlEngine::JavaScriptOwnership);

    logToFile(QString("QML loaded + created in %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 10: Pre-render + finalize =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 10: Pre-render + finalize");
#ifdef Q_OS_WIN
    splash.setStatus(L"Son haz\u0131rl\u0131klar yap\u0131l\u0131yor...");
    splash.pumpMessages();
#endif

    setupRootWindow(rootObject, app, engine, gameService, imageCache, startupTimer
#ifdef Q_OS_WIN
        , splash
#endif
    );

    // Hide window if launched with --minimized (Windows autostart → system tray)
    if (startMinimized) {
        if (auto* window = qobject_cast<QQuickWindow*>(rootObject))
            window->setVisible(false);
    }

    scheduleMemoryTrim();

    MAKINE_THREAD_NAME("Main/UI");

    setupPerfReporting(app, argc, argv);

    logToFile(QString("Total startup: %1 ms").arg(startupTimer.elapsed()));
    logToFile("Entering event loop...");

    // Anti-RE: periodic re-checks once event loop is running
    makine::protection::schedulePeriodicChecks();

    int exitCode = app.exec();
    makine::CrashReporter::shutdown();
    return exitCode;
}
