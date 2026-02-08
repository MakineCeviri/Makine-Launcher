/**
 * @file systemtraymanager.cpp
 * @brief System tray icon and menu management implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "systemtraymanager.h"
#include <QCoreApplication>

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
{
    buildMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTrayManager::onTrayActivated);
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked,
            this, &SystemTrayManager::onMessageClicked);

    // Background update check timer (disabled by default)
    m_updateCheckTimer.setInterval(6 * 60 * 60 * 1000); // 6 hours
    connect(&m_updateCheckTimer, &QTimer::timeout,
            this, &SystemTrayManager::updateCheckRequested);
}

SystemTrayManager::~SystemTrayManager() = default;

void SystemTrayManager::buildMenu()
{
    m_trayMenu = std::make_unique<QMenu>();

    // Title (disabled, acts as header)
    auto *titleAction = m_trayMenu->addAction("MakineAI v" + QCoreApplication::applicationVersion());
    titleAction->setEnabled(false);

    m_trayMenu->addSeparator();

    // Show window
    auto *showAction = m_trayMenu->addAction(tr("MakineAI'ı Aç"));
    connect(showAction, &QAction::triggered, this, &SystemTrayManager::showWindowRequested);

    // Settings
    auto *settingsAction = m_trayMenu->addAction(tr("Ayarlar"));
    connect(settingsAction, &QAction::triggered, this, &SystemTrayManager::settingsRequested);

    m_trayMenu->addSeparator();

    // Quit
    auto *quitAction = m_trayMenu->addAction(tr("Tamamen Kapat"));
    connect(quitAction, &QAction::triggered, this, &SystemTrayManager::quitRequested);

    m_trayIcon->setContextMenu(m_trayMenu.get());
    updateTooltip();
}

void SystemTrayManager::setIcon(const QIcon &icon)
{
    m_trayIcon->setIcon(icon);
}

void SystemTrayManager::show()
{
    m_trayIcon->show();
}

void SystemTrayManager::setPendingUpdates(int count)
{
    if (m_pendingUpdates == count) return;
    m_pendingUpdates = count;
    updateTooltip();
    emit pendingUpdatesChanged();
}

void SystemTrayManager::setBackgroundCheckEnabled(bool enabled)
{
    if (m_backgroundCheckEnabled == enabled) return;
    m_backgroundCheckEnabled = enabled;

    if (enabled) {
        m_updateCheckTimer.start();
    } else {
        m_updateCheckTimer.stop();
    }

    emit backgroundCheckEnabledChanged();
}

void SystemTrayManager::showNotification(const QString &title, const QString &message,
                                          int durationMs)
{
    if (!QSystemTrayIcon::supportsMessages()) return;
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, durationMs);
}

void SystemTrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick) {
        emit showWindowRequested();
    }
}

void SystemTrayManager::onMessageClicked()
{
    emit showWindowRequested();
}

void SystemTrayManager::updateTooltip()
{
    QString tooltip = "MakineAI";
    if (m_pendingUpdates > 0) {
        tooltip += tr(" — %1 güncelleme mevcut").arg(m_pendingUpdates);
    }
    m_trayIcon->setToolTip(tooltip);
}
