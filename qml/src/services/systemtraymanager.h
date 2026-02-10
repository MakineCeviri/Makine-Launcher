/**
 * @file systemtraymanager.h
 * @brief System tray icon using native Win32 APIs (no Qt6Widgets dependency)
 * @copyright (c) 2026 MakineAI Team
 *
 * Uses Shell_NotifyIconW + native popup menu to avoid linking Qt6::Widgets.
 * Saves ~7 MB by allowing QGuiApplication instead of QApplication.
 */

#pragma once

#include <QObject>
#include <QIcon>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

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

private:
    void updateTooltip();

#ifdef Q_OS_WIN
    static LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void handleTrayMessage(LPARAM lParam);
    void showContextMenu();
    HICON qIconToHicon(const QIcon &icon, int size);

    HWND m_msgWindow{nullptr};
    NOTIFYICONDATAW m_nid{};
    HMENU m_contextMenu{nullptr};
    HICON m_hIcon{nullptr};
    bool m_visible{false};

    enum MenuId { IdShow = 1, IdSettings, IdQuit };
#endif

    int m_pendingUpdates{0};
    bool m_backgroundCheckEnabled{false};
    bool m_serviceMode{false};
    QTimer m_updateCheckTimer;
};
