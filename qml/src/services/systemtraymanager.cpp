#include "systemtraymanager.h"

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_trayMenu(new QMenu())
{
    // Context menu
    auto *showAction = m_trayMenu->addAction("Göster");
    connect(showAction, &QAction::triggered, this, &SystemTrayManager::showWindowRequested);

    m_trayMenu->addSeparator();

    auto *quitAction = m_trayMenu->addAction("Çıkış");
    connect(quitAction, &QAction::triggered, this, &SystemTrayManager::quitRequested);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("MakineAI");

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTrayManager::onTrayActivated);
}

SystemTrayManager::~SystemTrayManager()
{
    delete m_trayMenu;
}

void SystemTrayManager::setIcon(const QIcon &icon)
{
    m_trayIcon->setIcon(icon);
}

void SystemTrayManager::show()
{
    m_trayIcon->show();
}

void SystemTrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick) {
        emit showWindowRequested();
    }
}
