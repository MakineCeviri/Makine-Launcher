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

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet
#include <dwmapi.h>    // DwmExtendFrameIntoClientArea

// Acrylic backdrop blur (DWM composition)
namespace {

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attribute;
    PVOID Data;
    ULONG DataSize;
};

using SetWindowCompositionAttributeFunc = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

void enableAcrylicBlur(HWND hwnd) {
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DWORD buildNumber = 0;
    {
        OSVERSIONINFOW ovi{};
        ovi.dwOSVersionInfoSize = sizeof(ovi);
        auto RtlGetVersion = reinterpret_cast<LONG(WINAPI*)(OSVERSIONINFOW*)>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
        if (RtlGetVersion) { RtlGetVersion(&ovi); buildNumber = ovi.dwBuildNumber; }
    }

    if (buildNumber >= 22621) {
        DWORD backdropType = 3; // Acrylic
        DwmSetWindowAttribute(hwnd, 38, &backdropType, sizeof(backdropType));
        return;
    }

    auto SetWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttributeFunc>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!SetWindowCompositionAttribute) return;

    ACCENT_POLICY accent{};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.GradientColor = 0x600A0A0A;

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.Attribute = 19;
    data.Data = &accent;
    data.DataSize = sizeof(accent);
    SetWindowCompositionAttribute(hwnd, &data);
}

} // namespace

// Native Win32 splash window — shown immediately while QML loads
// 440×240, rounded corners, logo, gradient bars, loading dots
class SplashWindow {
public:
    SplashWindow() = default;
    ~SplashWindow() { close(); if (m_logoBitmap) DeleteObject(m_logoBitmap); }
    SplashWindow(const SplashWindow&) = delete;
    SplashWindow& operator=(const SplashWindow&) = delete;

    // Load logo from QImage (call before show)
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

    void show() {
        static bool classRegistered = false;
        if (!classRegistered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = wndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"MakineAISplash";
            wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 15));
            wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
            RegisterClassW(&wc);
            classRegistered = true;
        }

        constexpr int w = 440, h = 240;
        int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

        m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"MakineAISplash", L"",
            WS_POPUP,
            sx, sy, w, h,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        if (m_hwnd) {
            HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 12, 12);
            SetWindowRgn(m_hwnd, rgn, TRUE);

            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
            UpdateWindow(m_hwnd);
            m_showTime = GetTickCount();

            // Repaint timer for status text updates (~5fps)
            SetTimer(m_hwnd, 1, 200, nullptr);
        }
    }

    void pumpMessages() {
        if (!m_hwnd) return;
        MSG msg;
        while (PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void waitMinimumDisplay(DWORD minMs) {
        if (!m_hwnd) return;
        while ((GetTickCount() - m_showTime) < minMs) {
            pumpMessages();
            Sleep(1);
        }
    }

    void setStatus(const wchar_t* text) {
        wcsncpy(m_status, text, 63);
        m_status[63] = L'\0';
        pumpMessages();
    }

    void close() {
        if (m_hwnd) {
            KillTimer(m_hwnd, 1);
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
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

    static void drawLoadingDots(HDC hdc, int cx, int cy) {
        HPEN nullPen = CreatePen(PS_NULL, 0, 0);
        HPEN oldPen = (HPEN)SelectObject(hdc, nullPen);
        for (int i = 0; i < 3; ++i) {
            int dx = (i - 1) * 16;
            COLORREF c = RGB(120, 120, 145);
            int r = 3;
            HBRUSH br = CreateSolidBrush(c);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
            Ellipse(hdc, cx + dx - r, cy - r, cx + dx + r, cy + r);
            SelectObject(hdc, oldBr);
            DeleteObject(br);
        }
        SelectObject(hdc, oldPen);
        DeleteObject(nullPen);
    }

    // AlphaBlend via dynamic loading (avoids link-time msimg32 dependency)
    static BOOL alphaBlend(HDC dest, int dx, int dy, int dw, int dh,
                           HDC src, int sx, int sy, int sw, int sh,
                           BLENDFUNCTION bf) {
        using Fn = BOOL(WINAPI*)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
        static Fn fn = reinterpret_cast<Fn>(
            GetProcAddress(LoadLibraryW(L"msimg32.dll"), "AlphaBlend"));
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

            HBRUSH bg = CreateSolidBrush(RGB(10, 10, 15));
            FillRect(mem, &rc, bg);
            DeleteObject(bg);

            // Subtle center glow
            struct { int rx, ry; COLORREF c; } glows[] = {
                {140, 70, RGB(15, 15, 24)},
                {100, 50, RGB(18, 18, 28)},
                {60,  30, RGB(22, 22, 34)},
            };
            int cx = w / 2, cy = h / 2 - 16;
            HPEN nullPen = CreatePen(PS_NULL, 0, 0);
            HPEN oldPen = (HPEN)SelectObject(mem, nullPen);
            for (auto& g : glows) {
                HBRUSH gbr = CreateSolidBrush(g.c);
                HBRUSH oldBr = (HBRUSH)SelectObject(mem, gbr);
                Ellipse(mem, cx - g.rx, cy - g.ry, cx + g.rx, cy + g.ry);
                SelectObject(mem, oldBr);
                DeleteObject(gbr);
            }
            SelectObject(mem, oldPen);
            DeleteObject(nullPen);

            // Top gradient bar (3px)
            float gp = self ? self->m_gradientPhase : 0.0f;
            drawAnimatedGradientBar(mem, 0, 0, w, 3, gp);

            SetBkMode(mem, TRANSPARENT);

            // Logo (replaces "MakineAI" title text)
            if (self && self->m_logoBitmap) {
                HDC logoDC = CreateCompatibleDC(mem);
                HBITMAP oldLogoBmp = (HBITMAP)SelectObject(logoDC, self->m_logoBitmap);

                BLENDFUNCTION bf{};
                bf.BlendOp = AC_SRC_OVER;
                bf.SourceConstantAlpha = 255;
                bf.AlphaFormat = AC_SRC_ALPHA;

                int logoX = (w - self->m_logoWidth) / 2;
                int logoY = 25;
                alphaBlend(mem, logoX, logoY, self->m_logoWidth, self->m_logoHeight,
                           logoDC, 0, 0, self->m_logoWidth, self->m_logoHeight, bf);

                SelectObject(logoDC, oldLogoBmp);
                DeleteDC(logoDC);
            }

            // Tagline — below logo
            HFONT tagFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(mem, tagFont);
            SetTextColor(mem, RGB(120, 120, 145));
            RECT tagRc = {0, 115, w, 140};
            DrawTextW(mem, L"Oyunlar\u0131n\u0131 T\u00FCrk\u00E7e Oynaman\u0131n En Kolay Yolu",
                      -1, &tagRc, DT_CENTER | DT_SINGLELINE);
            SelectObject(mem, oldFont);
            DeleteObject(tagFont);

            // Status text — 11px, above dots
            if (self && self->m_status[0]) {
                HFONT statusFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
                HFONT oldSFont = (HFONT)SelectObject(mem, statusFont);
                SetTextColor(mem, RGB(100, 100, 120));
                RECT statusRc = {0, h - 78, w, h - 62};
                DrawTextW(mem, self->m_status, -1, &statusRc, DT_CENTER | DT_SINGLELINE);
                SelectObject(mem, oldSFont);
                DeleteObject(statusFont);
            }

            drawLoadingDots(mem, w / 2, h - 55);

            // Version — 9px, bottom-right
            HFONT verFont = CreateFontW(-9, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            oldFont = (HFONT)SelectObject(mem, verFont);
            SetTextColor(mem, RGB(60, 60, 75));
            RECT verRc = {0, h - 24, w - 14, h - 8};
            DrawTextW(mem, L"v0.1.0-alpha", -1, &verRc, DT_RIGHT | DT_SINGLELINE);
            SelectObject(mem, oldFont);
            DeleteObject(verFont);

            // Bottom gradient bar (2px)
            drawAnimatedGradientBar(mem, 0, h - 2, w, 2, gp);

            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == 1 && self) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND m_hwnd{nullptr};
    DWORD m_showTime{0};
    float m_gradientPhase{0.25f};
    wchar_t m_status[64]{};
    HBITMAP m_logoBitmap{nullptr};
    int m_logoWidth{0};
    int m_logoHeight{0};
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
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/processscanner.h"
#include "services/systemtraymanager.h"
#include "services/integrityservice.h"
#include "services/batchoperationservice.h"
#include "services/updatechecker.h"
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
    QSharedMemory cleanupMemory("MakineAI_SingleInstance_Guard");
    if (cleanupMemory.attach()) {
        cleanupMemory.detach();
    }

    QSharedMemory singleInstanceGuard("MakineAI_SingleInstance_Guard");
    if (!singleInstanceGuard.create(1)) {
        if (singleInstanceGuard.attach()) {
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
    }

    QGuiApplication app(argc, argv);

    // Anti-RE: run all checks before anything else (no-op in debug builds)
    makineai::protection::initialize();

#ifdef Q_OS_WIN
    SplashWindow splash;
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
    app.setApplicationVersion("0.1.0-alpha");
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
#ifdef Q_OS_WIN
    splash.setStatus(L"Dizin yap\u0131s\u0131 haz\u0131rlan\u0131yor...");
#endif
    AppPaths::ensureDirectories();
    AppPaths::migrateFromFlatLayout();

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
    // Sync starts after QML creation (non-blocking)

    auto* translationDownloader = new TranslationDownloader(&app);
    translationDownloader->setManifestSync(manifestSync);
    translationDownloader->setDataPath(makineai::AppPaths::packagesDir());
    engine.rootContext()->setContextProperty("TranslationDownloader", translationDownloader);

    // ===== Phase 3: Game library (construction only — data loads after QML) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Oyun k\u00FCt\u00FCphanesi haz\u0131rlan\u0131yor...");
#endif
    auto* gameService = new GameService(&app);
    gameService->setManifestSync(manifestSync);
    engine.rootContext()->setContextProperty("GameService", gameService);
    // initialize() deferred to after QML creation for faster first frame
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
    engine.rootContext()->setContextProperty("ProcessScanner", processScanner);

    // ===== Phase 6: Security + integrity =====
#ifdef Q_OS_WIN
    splash.setStatus(L"B\u00FCt\u00FCnl\u00FCk do\u011Frulamas\u0131 yap\u0131l\u0131yor...");
#endif
    auto* integrityService = new IntegrityService(&app);
    engine.rootContext()->setContextProperty("IntegrityService", integrityService);

    auto* batchService = new BatchOperationService(&app);
    engine.rootContext()->setContextProperty("BatchOperationService", batchService);

    // ===== Phase 7: Update checker + system tray =====
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00FCncelleme servisi haz\u0131rlan\u0131yor...");
#endif
    auto* updateChecker = new UpdateChecker(&app);
    engine.rootContext()->setContextProperty("UpdateChecker", updateChecker);

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

    // ===== Phase 8: QML engine loading (heaviest single operation) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Aray\u00FCz derleniyor...");
#endif
    logToFile(QString("Phase 8 (QML load start) at %1 ms").arg(startupTimer.elapsed()));

    // Use QQmlComponent for incremental loading — keeps splash alive
    QQmlComponent mainComponent(&engine);
    mainComponent.loadFromModule("MakineAI", "Main");

    // Pump events while QML compiles (first launch is slow)
#ifdef Q_OS_WIN
    while (mainComponent.isLoading()) {
        splash.pumpMessages();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
    }
#endif

    if (mainComponent.isError()) {
        for (const auto& error : mainComponent.errors())
            logToFile(QString("QML Error: %1").arg(error.toString()));
#ifdef Q_OS_WIN
        splash.close();
#endif
        return -1;
    }

    logToFile(QString("Phase 8 (QML compiled) at %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 9: Create root window =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Pencere olu\u015Fturuluyor...");
    splash.pumpMessages();
#endif
    QObject* rootObject = mainComponent.create();
    if (!rootObject) {
        logToFile("ERROR: Failed to create root object!");
#ifdef Q_OS_WIN
        splash.close();
#endif
        return -1;
    }
    engine.setObjectOwnership(rootObject, QQmlEngine::JavaScriptOwnership);

    logToFile(QString("QML loaded + created in %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 9.5: Deferred data loading =====
    // Schedule GameService init for AFTER splash closes and first frames render.
    // Delay ensures pre-render loop only warms up shaders with empty catalog state
    // (BusyIndicator shown), then data loads AFTER window is visible.
    QTimer::singleShot(300, gameService, [gameService, manifestSync, &startupTimer]() {
        gameService->initialize();
        logToFile(QString("GameService initialized at %1 ms").arg(startupTimer.elapsed()));

        // Start remote catalog sync (index.json only — lightweight)
        manifestSync->syncCatalog();
    });

    // Catalog index ready → reload PackageCatalog from cached index
    QObject::connect(manifestSync, &makineai::ManifestSyncService::catalogReady,
        gameService, [gameService]() {
            if (auto* bridge = makineai::CoreBridge::instance())
                bridge->refreshPackageManifest();
        });

    // Per-game detail ready → enrich PackageCatalog entry
    QObject::connect(manifestSync, &makineai::ManifestSyncService::packageDetailReady,
        gameService, [manifestSync](const QString& appId) {
            if (auto* bridge = makineai::CoreBridge::instance()) {
                QVariantMap detail = manifestSync->getPackageDetail(appId);
                QJsonDocument doc(QJsonObject::fromVariantMap(detail));
                bridge->enrichPackageFromJson(appId, doc.toJson(QJsonDocument::Compact));
            }
        });

    // ===== Phase 10: Pre-render + finalize =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Son haz\u0131rl\u0131klar yap\u0131l\u0131yor...");
    splash.pumpMessages();
#endif

    // Release GPU resources when window is hidden/minimized (reclaimed on show)
    auto *window = qobject_cast<QQuickWindow*>(rootObject);
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

            // Let QML handle the background color (Theme.bgPrimary)
            // Acrylic blur disabled — was causing white/transparent background issues.
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

#ifdef Q_OS_WIN
        // Release GPU resources + trim working set when hidden/minimized
        QObject::connect(window, &QWindow::visibilityChanged, [window](QWindow::Visibility v) {
            if (v == QWindow::Hidden || v == QWindow::Minimized) {
                window->releaseResources();
                EmptyWorkingSet(GetCurrentProcess());
            }
        });

        logToFile(QString("Phase 10 (pre-render start) at %1 ms").arg(startupTimer.elapsed()));

        // Pre-render: warm up shaders with the empty loading state (BusyIndicator).
        // Only process rendering events — avoid firing deferred timers that trigger
        // heavy data loading (GameService init, catalog sync, 265-game QML rebind).
        window->requestUpdate();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        splash.pumpMessages();

        logToFile(QString("Phase 10 (pre-render done) at %1 ms").arg(startupTimer.elapsed()));
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

    // Close splash — window is fully ready
#ifdef Q_OS_WIN
    splash.waitMinimumDisplay(200);
    logToFile(QString("Splash closed at %1 ms").arg(startupTimer.elapsed()));
    splash.close();
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

    return app.exec();
}
