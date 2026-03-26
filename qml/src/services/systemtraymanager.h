/**
 * @file systemtraymanager.h
 * @brief System tray icon using native Win32 APIs (no Qt6Widgets dependency)
 * @copyright (c) 2026 MakineCeviri Team
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
    void hide();

    int pendingUpdates() const { return m_pendingUpdates; }
    void setPendingUpdates(int count);

    bool backgroundCheckEnabled() const { return m_backgroundCheckEnabled; }
    void setBackgroundCheckEnabled(bool enabled);

signals:
    void showWindowRequested();
    void settingsRequested();
    void quitRequested();
    void pendingUpdatesChanged();
    void backgroundCheckEnabledChanged();
    void updateCheckRequested();
    void contextMenuRequested(int x, int y);

private:
    void updateTooltip();

#ifdef Q_OS_WIN
    static LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void handleTrayMessage(LPARAM lParam);
    void showContextMenu();
    HICON qIconToHicon(const QIcon &icon, int size);

    HWND m_msgWindow{nullptr};
    HWND m_anchorWindow{nullptr}; // Real HWND for SetForegroundWindow (HWND_MESSAGE cannot be foreground)
    NOTIFYICONDATAW m_nid{};
    HICON m_hIcon{nullptr};
    bool m_visible{false};

    enum MenuId { IdShow = 1, IdCheckUpdates, IdSettings, IdQuit };
#endif

    int m_pendingUpdates{0};
    bool m_backgroundCheckEnabled{false};
    QTimer m_updateCheckTimer;
};
