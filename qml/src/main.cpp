/**
 * @file main.cpp
 * @brief MakineAI QML Application Entry Point
 * @copyright (c) 2026 MakineAI Team
 */

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QDebug>
#include <QFile>
#include <QSharedMemory>
#include <QTextStream>
#include <QDateTime>

// File-based logging for debugging
void logToFile(const QString& msg) {
    QFile file("C:/cedra/makineai_debug.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " - " << msg << "\n";
        file.close();
    }
}

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Backend services
#include "services/gameservice.h"
#include "services/translationservice.h"
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/processscanner.h"
#include "services/systemtraymanager.h"

int main(int argc, char *argv[])
{
    // Single instance check using shared memory
    // Fix for stale shared memory after crash: try to clean up first
    QSharedMemory cleanupMemory("MakineAI_SingleInstance_Guard");
    if (cleanupMemory.attach()) {
        // Detach to clean up potentially stale segment
        cleanupMemory.detach();
    }

    QSharedMemory singleInstanceGuard("MakineAI_SingleInstance_Guard");

    // Try to create first (normal case - no other instance)
    if (!singleInstanceGuard.create(1)) {
        // Creation failed - another instance might be running
        // Try to attach to verify
        if (singleInstanceGuard.attach()) {
            // Another instance is actually running - show warning
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
        // Attach also failed - segment is stale, ignore and continue
        qWarning() << "Stale shared memory segment detected, continuing anyway";
    }

    QApplication app(argc, argv);

    // Allow app to keep running when window is hidden (system tray)
    app.setQuitOnLastWindowClosed(false);

    // Application metadata
    app.setApplicationName("MakineAI");
    app.setApplicationVersion("0.1.0-alpha");
    app.setOrganizationName("MakineAI");
    app.setOrganizationDomain("makineai.com");

    // Set application icon for taskbar and window
    app.setWindowIcon(QIcon(":/qt/qml/MakineAI/resources/images/logo.png"));

    // Use Basic style for full customization
    QQuickStyle::setStyle("Basic");

    // Load Inter font (Flutter style)
    int interRegular = QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Regular.ttf");
    int interMedium = QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Medium.ttf");
    int interSemiBold = QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-SemiBold.ttf");
    int interBold = QFontDatabase::addApplicationFont(":/MakineAI/fonts/Inter-Bold.ttf");

    // Set default font to Inter
    QString fontFamily = "Inter";
    if (interRegular < 0) {
        // Fallback to Segoe UI if Inter failed to load
        fontFamily = "Segoe UI";
        qWarning() << "Failed to load Inter font, using Segoe UI fallback";
    } else {
        qDebug() << "Inter font loaded successfully";
    }

    QFont defaultFont(fontFamily, 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(defaultFont);

    QQmlApplicationEngine engine;

    // System tray icon (C++ managed, exposed to QML)
    SystemTrayManager trayManager;
    trayManager.setIcon(app.windowIcon());
    trayManager.show();
    engine.rootContext()->setContextProperty("SystemTrayManager", &trayManager);

    logToFile("=== MakineAI Starting ===");
    logToFile(QString("App version: %1").arg(app.applicationVersion()));
    logToFile(QString("Qt version: %1").arg(qVersion()));

    // Handle QML warnings (single handler)
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                logToFile(QString("QML Warning: %1").arg(warning.toString()));
            }
        }
    );

    // Handle QML object creation failure (single handler)
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            logToFile("CRITICAL: QML Object creation failed!");
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    // Load QML module
    logToFile("Loading QML module MakineAI.Main...");
    engine.loadFromModule("MakineAI", "Main");
    logToFile("QML module loaded, checking root objects...");

    if (engine.rootObjects().isEmpty()) {
        logToFile("ERROR: No root objects loaded! Application will exit.");
        return -1;
    }

    logToFile(QString("Root objects count: %1").arg(engine.rootObjects().size()));
    logToFile("Entering event loop...");
    return app.exec();
}
