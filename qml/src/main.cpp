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

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet
#endif

// Resolve log file path using platform-appropriate location
static QString getLogFilePath() {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDir);
    return logDir + "/makineai_debug.log";
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


// Backend services
#include "services/gameservice.h"
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/processscanner.h"
#include "services/systemtraymanager.h"
#include "services/integrityservice.h"

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

    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("MakineAI");
    app.setApplicationVersion("0.1.0-alpha");
    app.setOrganizationName("MakineAI");
    app.setOrganizationDomain("makineai.com");
    app.setWindowIcon(QIcon(":/qt/qml/MakineAI/resources/images/logo.png"));
    QQuickStyle::setStyle("Basic");

    int interRegular = QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Bold.ttf");

    QString fontFamily = interRegular >= 0 ? "Inter" : "Segoe UI";
    QFont defaultFont(fontFamily, 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(defaultFont);

    // Reduce thread pool: default = CPU count, each thread = 1 MB stack
    QThreadPool::globalInstance()->setMaxThreadCount(2);

    QQmlApplicationEngine engine;

    SystemTrayManager trayManager;
    trayManager.setIcon(app.windowIcon());
    trayManager.show();
    engine.rootContext()->setContextProperty("SystemTrayManager", &trayManager);

    logToFile("=== MakineAI Starting ===");
    logToFile(QString("App version: %1").arg(app.applicationVersion()));
    logToFile(QString("Qt version: %1").arg(qVersion()));
    logToFile(QString("Log file: %1").arg(getLogFilePath()));
    logToFile(QString("QSG_RENDER_LOOP: %1").arg(qEnvironmentVariable("QSG_RENDER_LOOP")));
    {
        QSettings bs("MakineAI", "MakineAI");
        QString cfgBackend = bs.value("performance/graphicsBackend", "vulkan").toString();
        auto api = QQuickWindow::graphicsApi();
        QString apiName = api == QSGRendererInterface::Direct3D12 ? "D3D12" :
                          api == QSGRendererInterface::Vulkan     ? "Vulkan" :
                          api == QSGRendererInterface::Direct3D11 ? "D3D11" :
                          api == QSGRendererInterface::OpenGL     ? "OpenGL" :
                          "Auto (RHI default)";
        logToFile(QString("Graphics backend setting: %1 → API: %2").arg(cfgBackend, apiName));
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

    logToFile("Loading QML module MakineAI.Main...");
    engine.loadFromModule("MakineAI", "Main");
    logToFile("QML module loaded, checking root objects...");

    if (engine.rootObjects().isEmpty()) {
        logToFile("ERROR: No root objects loaded! Application will exit.");
        return -1;
    }

    // Release GPU resources when window is hidden/minimized (reclaimed on show)
    auto *window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
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
#endif
    }

    // Trim working set after startup settles (DLL init, type registration, QML
    // compilation pages are no longer needed). Hot pages fault back in microseconds.
#ifdef Q_OS_WIN
    QTimer::singleShot(5000, [&]() {
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

    logToFile(QString("Root objects count: %1").arg(engine.rootObjects().size()));
    logToFile("Entering event loop...");
    return app.exec();
}
