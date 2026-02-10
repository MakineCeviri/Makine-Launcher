/**
 * @file systemtraymanager.h
 * @brief System tray icon and menu management
 * @copyright (c) 2026 MakineAI Team
 *
 * Provides system tray icon with context menu, toast notifications,
 * and background service coordination. Supports minimize-to-tray on
 * window close and Windows startup integration.
 */

#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QTimer>
#include <memory>

class SystemTrayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pendingUpdates READ pendingUpdates WRITE setPendingUpdates NOTIFY pendingUpdatesChanged)
    Q_PROPERTY(bool backgroundCheckEnabled READ backgroundCheckEnabled WRITE setBackgroundCheckEnabled NOTIFY backgroundCheckEnabledChanged)

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager();

    void setIcon(const QIcon &icon);
    void show();

    int pendingUpdates() const { return m_pendingUpdates; }
    void setPendingUpdates(int count);

    bool backgroundCheckEnabled() const { return m_backgroundCheckEnabled; }
    void setBackgroundCheckEnabled(bool enabled);

    Q_INVOKABLE void showNotification(const QString &title, const QString &message,
                                      int durationMs = 5000);

    Q_INVOKABLE void enterServiceMode();
    Q_INVOKABLE void exitServiceMode();

    bool isServiceMode() const { return m_serviceMode; }

signals:
    void serviceModeChanged(bool serviceMode);
    void showWindowRequested();
    void settingsRequested();
    void quitRequested();
    void pendingUpdatesChanged();
    void backgroundCheckEnabledChanged();
    void updateCheckRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageClicked();

private:
    void buildMenu();
    void updateTooltip();

    QSystemTrayIcon *m_trayIcon;
    std::unique_ptr<QMenu> m_trayMenu;
    int m_pendingUpdates{0};
    bool m_backgroundCheckEnabled{false};
    bool m_serviceMode{false};
    QTimer m_updateCheckTimer;
};
