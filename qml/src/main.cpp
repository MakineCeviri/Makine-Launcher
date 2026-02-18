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

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet

// Native Win32 splash window — shown immediately while QML loads
// 440×240, rounded corners, static gradient bars, loading dots
class SplashWindow {
public:
    SplashWindow() = default;
    ~SplashWindow() { close(); }
    SplashWindow(const SplashWindow&) = delete;
    SplashWindow& operator=(const SplashWindow&) = delete;

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

    // Pump pending Win32 messages so splash animation stays alive
    // Call between heavy init steps to prevent freeze
    void pumpMessages() {
        if (!m_hwnd) return;
        MSG msg;
        while (PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Ensure splash is visible for at least minMs, keeping animation alive
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

    // Interpolate between two COLORREFs by fraction f (0..1)
    static COLORREF lerpColor(COLORREF a, COLORREF b, float f) {
        return RGB(
            GetRValue(a) + (int)((GetRValue(b) - GetRValue(a)) * f),
            GetGValue(a) + (int)((GetGValue(b) - GetGValue(a)) * f),
            GetBValue(a) + (int)((GetBValue(b) - GetBValue(a)) * f)
        );
    }

    // Smooth animated gradient bar — colors flow from center outward
    static void drawAnimatedGradientBar(HDC hdc, int x, int y, int barW, int barH, float phase) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = barW;
        bmi.bmiHeader.biHeight = -barH; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        BYTE* pixels = nullptr;
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                       reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!dib || !pixels) return;

        float halfW = barW / 2.0f;
        for (int px = 0; px < barW; ++px) {
            // Symmetric distance from center (0 at center, 1 at edges)
            float dist = (px < halfW)
                ? (halfW - px) / halfW
                : (px - halfW) / halfW;

            // Colors flow outward: center shows newest, edges show older
            float t = phase - dist * 0.65f;
            t = t - (float)(int)(t); // fast floor for positive
            if (t < 0.0f) t += 1.0f;

            // Map to color palette with smooth interpolation
            float colorPos = t * (kColorCount - 1);
            int idx = (int)colorPos;
            float frac = colorPos - idx;
            if (idx >= kColorCount - 1) { idx = kColorCount - 2; frac = 1.0f; }

            COLORREF c = lerpColor(kBrandColors[idx], kBrandColors[idx + 1], frac);
            BYTE r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);

            for (int py = 0; py < barH; ++py) {
                int off = (py * barW + px) * 4;
                pixels[off + 0] = b;  // BGRA byte order
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

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<SplashWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right, h = rc.bottom;

            // Double-buffer to prevent flicker
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            HBRUSH bg = CreateSolidBrush(RGB(10, 10, 15));
            FillRect(mem, &rc, bg);
            DeleteObject(bg);

            // Subtle center glow (concentric ellipses)
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

            // Animated smooth gradient bar — top (3px)
            float gp = self ? self->m_gradientPhase : 0.0f;
            drawAnimatedGradientBar(mem, 0, 0, w, 3, gp);

            SetBkMode(mem, TRANSPARENT);

            HFONT titleFont = CreateFontW(-30, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(mem, titleFont);
            SetTextColor(mem, RGB(245, 245, 250));
            RECT titleRc = {0, 55, w, 100};
            DrawTextW(mem, L"MakineAI", -1, &titleRc, DT_CENTER | DT_SINGLELINE);
            SelectObject(mem, oldFont);
            DeleteObject(titleFont);

            HFONT tagFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            oldFont = (HFONT)SelectObject(mem, tagFont);
            SetTextColor(mem, RGB(120, 120, 145));
            RECT tagRc = {0, 105, w, 130};
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

            // Version — 9px, bottom-right corner
            HFONT verFont = CreateFontW(-9, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            oldFont = (HFONT)SelectObject(mem, verFont);
            SetTextColor(mem, RGB(60, 60, 75));
            RECT verRc = {0, h - 24, w - 14, h - 8};
            DrawTextW(mem, L"v0.1.0pre-alpha", -1, &verRc, DT_RIGHT | DT_SINGLELINE);
            SelectObject(mem, oldFont);
            DeleteObject(verFont);

            // Animated smooth gradient bar — bottom (2px)
            drawAnimatedGradientBar(mem, 0, h - 2, w, 2, gp);

            // Blit to screen
            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == 1 && self) {
                // Static splash — only repaint for status text updates
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND m_hwnd{nullptr};
    DWORD m_showTime{0};
    float m_gradientPhase{0.25f}; // Static gradient position
    wchar_t m_status[64]{};
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
#include "services/notificationservice.h"
#include "services/operationjournal.h"
#include "services/imagecachemanager.h"
#include "services/corebridge.h"

int main(int argc, char *argv[])
{
    // === RENDER LOOP ===
    // "basic" = single-threaded, renders only when scene changes.
    // Best idle CPU/GPU: Vulkan + basic loop → near-zero when nothing animates.
    qputenv("QSG_RENDER_LOOP", "basic");

    // === GRAPHICS BACKEND ===
    // Vulkan: lowest idle GPU on Windows with basic render loop.
    // Override: QSG_RHI_BACKEND env var always takes precedence.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND")) {
        QSettings backendSettings("MakineAI", "MakineAI");
        QString backend = backendSettings.value("performance/graphicsBackend", "vulkan").toString();
#ifdef QT_STATIC
        // Static Qt builds: Vulkan RHI backend hangs during QML loading,
        // force D3D11 regardless of saved setting.
        if (backend == "vulkan")
            backend = "d3d11";
#endif
        // Migrate: "auto" from earlier versions → vulkan
        if (backend == "auto") {
            backend = "vulkan";
            backendSettings.setValue("performance/graphicsBackend", backend);
        }

        // Probe Vulkan availability: load vulkan-1.dll and check for physical devices
        auto isVulkanAvailable = []() -> bool {
            QLibrary vulkanLib("vulkan-1");
            if (!vulkanLib.load())
                return false;

            using PFN_vkCreateInstance = int (*)(const void *, const void *, void **);
            using PFN_vkDestroyInstance = void (*)(void *, const void *);
            using PFN_vkEnumeratePhysicalDevices = int (*)(void *, uint32_t *, void **);

            auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
                vulkanLib.resolve("vkCreateInstance"));
            auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
                vulkanLib.resolve("vkDestroyInstance"));
            auto vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
                vulkanLib.resolve("vkEnumeratePhysicalDevices"));

            if (!vkCreateInstance || !vkDestroyInstance || !vkEnumeratePhysicalDevices)
                return false;

            struct VkApplicationInfo {
                int sType = 0; // VK_STRUCTURE_TYPE_APPLICATION_INFO
                const void *pNext = nullptr;
                const char *pApplicationName = "MakineAI";
                uint32_t applicationVersion = 0;
                const char *pEngineName = nullptr;
                uint32_t engineVersion = 0;
                uint32_t apiVersion = (1 << 22) | (0 << 12); // VK_API_VERSION_1_0
            };
            struct VkInstanceCreateInfo {
                int sType = 1; // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
                const void *pNext = nullptr;
                uint32_t flags = 0;
                const VkApplicationInfo *pApplicationInfo = nullptr;
                uint32_t enabledLayerCount = 0;
                const char *const *ppEnabledLayerNames = nullptr;
                uint32_t enabledExtensionCount = 0;
                const char *const *ppEnabledExtensionNames = nullptr;
            };

            VkApplicationInfo appInfo{};
            VkInstanceCreateInfo createInfo{};
            createInfo.pApplicationInfo = &appInfo;

            void *instance = nullptr;
            if (vkCreateInstance(&createInfo, nullptr, &instance) != 0 || !instance)
                return false;

            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            vkDestroyInstance(instance, nullptr);
            return deviceCount > 0;
        };

        if (backend == "vulkan") {
            if (isVulkanAvailable())
                QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
            else
                QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11); // fallback
        } else if (backend == "d3d11") {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
        } else if (backend == "opengl") {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        }
        // "auto" = no explicit call, Qt chooses best available
    }

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
    splash.show();
#endif

    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("MakineAI");
    app.setApplicationVersion("0.1.0pre-alpha");
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

    // Reduce thread pool: default = CPU count, each thread = 1 MB stack
    QThreadPool::globalInstance()->setMaxThreadCount(2);

    QQmlApplicationEngine engine;

    // ===== Register backend singletons for QML access =====
    // Manual registration — works in both shared and static Qt builds.
    // (QML_ELEMENT/QML_SINGLETON relies on linker keeping registration code,
    //  which --gc-sections strips in static builds.)
    using namespace makineai;

    QElapsedTimer startupTimer;
    startupTimer.start();

    // ===== Phase 1: Directory bootstrap + lightweight services =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Dizinler haz\u0131rlan\u0131yor...");
#endif
    AppPaths::ensureDirectories();
    AppPaths::migrateFromFlatLayout();

#ifdef Q_OS_WIN
    splash.setStatus(L"Ayarlar y\u00FCkleniyor...");
#endif
    auto* settingsManager = new SettingsManager(&app);
    engine.rootContext()->setContextProperty("SettingsManager", settingsManager);

    auto* imageCache = new ImageCacheManager(&app);
    engine.rootContext()->setContextProperty("ImageCache", imageCache);

    // ===== Phase 2: GameService init (CoreBridge + cache loading) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Oyun k\u00FCt\u00FCphanesi y\u00FCkleniyor...");
#endif
    auto* gameService = new GameService(&app);
    engine.rootContext()->setContextProperty("GameService", gameService);
    gameService->initialize();
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    // ===== Phase 3: Register remaining services =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Servisler ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* journal = new OperationJournal(&app);
    if (journal->hasPendingOperation()) {
        qDebug() << "OperationJournal: recovering from interrupted operation...";
        journal->recover();
    }
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    auto* backupManager = new BackupManager(&app);
    backupManager->setJournal(journal);
    engine.rootContext()->setContextProperty("BackupManager", backupManager);

    auto* processScanner = new ProcessScanner(&app);
    engine.rootContext()->setContextProperty("ProcessScanner", processScanner);

    auto* integrityService = new IntegrityService(&app);
    engine.rootContext()->setContextProperty("IntegrityService", integrityService);

    auto* batchService = new BatchOperationService(&app);
    engine.rootContext()->setContextProperty("BatchOperationService", batchService);

    auto* notificationService = new NotificationService(&app);
    engine.rootContext()->setContextProperty("NotificationService", notificationService);

    auto* updateChecker = new UpdateChecker(&app);
    engine.rootContext()->setContextProperty("UpdateChecker", updateChecker);

    SystemTrayManager trayManager;
    trayManager.setIcon(app.windowIcon());
    trayManager.show();
    engine.rootContext()->setContextProperty("SystemTrayManager", &trayManager);

    // Wire journal to CoreBridge
    CoreBridge::instance()->setJournal(journal);

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
        QString configuredBackend = settings.value("performance/graphicsBackend", "vulkan").toString();
        auto api = QQuickWindow::graphicsApi();
        QString apiName = api == QSGRendererInterface::Direct3D12 ? "D3D12" :
                          api == QSGRendererInterface::Vulkan     ? "Vulkan" :
                          api == QSGRendererInterface::Direct3D11 ? "D3D11" :
                          api == QSGRendererInterface::OpenGL     ? "OpenGL" :
                          "Auto (RHI default)";
        logToFile(QString("Graphics backend setting: %1 → API: %2").arg(configuredBackend, apiName));
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

    // ===== Phase 5: QML engine loading (heaviest single operation) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Aray\u00FCz derleniyor...");
#endif
    logToFile("Loading QML module MakineAI.Main...");

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

    // Create the root object
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

    // ===== Phase 6: Pre-render first frame, then show window =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Neredeyse haz\u0131r...");
    splash.pumpMessages();
#endif

    // Release GPU resources when window is hidden/minimized (reclaimed on show)
    auto *window = qobject_cast<QQuickWindow*>(rootObject);
    if (window) {
        window->setPersistentGraphics(false);
        window->setPersistentSceneGraph(false);

        // Enable VK_EXT_memory_budget when using Vulkan backend
        if (QQuickWindow::graphicsApi() == QSGRendererInterface::Vulkan) {
            QQuickGraphicsConfiguration gfxConfig;
            gfxConfig.setDeviceExtensions({"VK_EXT_memory_budget"});
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

        // Pre-render: process a few frames so shaders compile before user sees window
        // This prevents the "white flash" on first frame
        for (int i = 0; i < 3; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
            splash.pumpMessages();
        }
#endif
    }

    // Close splash — window is fully ready
#ifdef Q_OS_WIN
    splash.waitMinimumDisplay(800);
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

    logToFile(QString("Total startup: %1 ms").arg(startupTimer.elapsed()));
    logToFile("Entering event loop...");

    // Anti-RE: periodic re-checks once event loop is running
    makineai::protection::schedulePeriodicChecks();

    return app.exec();
}
