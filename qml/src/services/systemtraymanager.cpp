/**
 * @file systemtraymanager.cpp
 * @brief Native Win32 system tray implementation (no Qt6Widgets)
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "systemtraymanager.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>

#ifdef Q_OS_WIN

namespace {
constexpr int kUpdateCheckIntervalMs = 6 * 60 * 60 * 1000; // 6 hours
}

static const UINT WM_TRAYICON = WM_APP + 1;
static const wchar_t TrayWindowClassName[] = L"MakineLauncher_TrayMsgWindow";

// Accessed only from GUI thread (Win32 message pump) — no synchronization needed
static SystemTrayManager *s_instance = nullptr;

LRESULT CALLBACK SystemTrayManager::trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON && s_instance) {
        s_instance->handleTrayMessage(lParam);
        return 0;
    }
    if (msg == WM_COMMAND && s_instance) {
        switch (LOWORD(wParam)) {
        case IdShow:         emit s_instance->showWindowRequested(); break;
        case IdCheckUpdates: emit s_instance->updateCheckRequested(); break;
        case IdSettings:     emit s_instance->settingsRequested(); break;
        case IdQuit:         emit s_instance->quitRequested(); break;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SystemTrayManager::handleTrayMessage(LPARAM lParam)
{
    switch (LOWORD(lParam)) {
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case NIN_SELECT:      // NOTIFYICON_VERSION_4: single click / select
    case NIN_KEYSELECT:   // NOTIFYICON_VERSION_4: keyboard select
        emit showWindowRequested();
        break;
    case WM_RBUTTONUP:    // NOTIFYICON_VERSION_0 fallback
    case WM_CONTEXTMENU:  // NOTIFYICON_VERSION_4: right-click (replaces WM_RBUTTONUP)
        showContextMenu();
        break;
    case NIN_BALLOONUSERCLICK:
        emit showWindowRequested();
        break;
    }
}

void SystemTrayManager::showContextMenu()
{
    POINT pt;
    GetCursorPos(&pt);

    // Native Win32 context menu — reliable in background (no QML focus tricks needed).
    // TrackPopupMenu is system-managed; it doesn't require our process to be foreground
    // to APPEAR. We still call SetForegroundWindow so the menu dismisses on click-outside.
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING,    IdShow,         L"A\u00e7");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,              nullptr);
    AppendMenuW(hMenu, MF_STRING,    IdSettings,     L"Ayarlar");
    AppendMenuW(hMenu, MF_STRING,    IdCheckUpdates, L"G\u00fcncelleme Kontrol Et");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,              nullptr);
    AppendMenuW(hMenu, MF_STRING,    IdQuit,         L"Tamamen Kapat");

    // Briefly show anchor window so SetForegroundWindow succeeds.
    // 1x1 off-screen, WS_EX_TOOLWINDOW — invisible to user.
    ShowWindow(m_anchorWindow, SW_SHOW);
    SetForegroundWindow(m_anchorWindow);

    // Position: above cursor (taskbar at bottom), right-aligned
    UINT flags = TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN;
    TrackPopupMenu(hMenu, flags, pt.x, pt.y, 0, m_anchorWindow, nullptr);
    PostMessage(m_anchorWindow, WM_NULL, 0, 0); // Ensure menu dismisses

    DestroyMenu(hMenu);
    ShowWindow(m_anchorWindow, SW_HIDE);
}

HICON SystemTrayManager::qIconToHicon(const QIcon &icon, int size)
{
    QPixmap pixmap = icon.pixmap(size, size);
    if (pixmap.isNull()) return nullptr;

    QImage img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    if (img.isNull()) return nullptr;

    BITMAPV5HEADER bi{};
    bi.bV5Size        = sizeof(BITMAPV5HEADER);
    bi.bV5Width       = img.width();
    bi.bV5Height      = -img.height(); // top-down
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    HDC dc = GetDC(nullptr);
    void *bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi),
                                       DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!hBitmap) return nullptr;

    memcpy(bits, img.constBits(), img.sizeInBytes());

    HBITMAP hMask = CreateBitmap(img.width(), img.height(), 1, 1, nullptr);
    if (!hMask) {
        DeleteObject(hBitmap);
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hBitmap;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hBitmap);
    DeleteObject(hMask);

    return hIcon;
}

#endif // Q_OS_WIN

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    s_instance = this;

    // Register message-only window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = trayWndProc;
    wc.hInstance      = GetModuleHandle(nullptr);
    wc.lpszClassName  = TrayWindowClassName;
    RegisterClassExW(&wc);

    // Create message-only window (HWND_MESSAGE = invisible, no taskbar entry)
    m_msgWindow = CreateWindowExW(0, TrayWindowClassName, L"MakineLauncher_TrayMsg",
                                   0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                   GetModuleHandle(nullptr), nullptr);

    // Anchor window: a real (non-HWND_MESSAGE) 1x1 off-screen window used to
    // call SetForegroundWindow() before showing the QML tray popup.
    // WS_EX_TOOLWINDOW hides it from taskbar and Alt+Tab.
    // NOTE: NO WS_EX_NOACTIVATE — that flag prevents activation and breaks SetForegroundWindow.
    m_anchorWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        TrayWindowClassName, L"MakineLauncher_TrayAnchor",
        WS_POPUP,
        -1, -1, 1, 1,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    // Not shown at startup — shown/hidden only in showContextMenu()

    // Prepare NOTIFYICONDATA
    m_nid = NOTIFYICONDATAW{};
    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = m_msgWindow;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.uVersion         = NOTIFYICON_VERSION_4;
    wcscpy_s(m_nid.szTip, _countof(m_nid.szTip), L"Makine Launcher");
#endif

    // Background update check timer (disabled by default)
    m_updateCheckTimer.setInterval(kUpdateCheckIntervalMs);
    connect(&m_updateCheckTimer, &QTimer::timeout,
            this, &SystemTrayManager::updateCheckRequested);
}

SystemTrayManager::~SystemTrayManager()
{
#ifdef Q_OS_WIN
    if (m_visible) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }
    if (m_anchorWindow) {
        DestroyWindow(m_anchorWindow);
    }
    if (m_msgWindow) {
        DestroyWindow(m_msgWindow);
    }
    UnregisterClassW(TrayWindowClassName, GetModuleHandle(nullptr));
    s_instance = nullptr;
#endif
}

void SystemTrayManager::setIcon(const QIcon &icon)
{
#ifdef Q_OS_WIN
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }
    m_hIcon = qIconToHicon(icon, GetSystemMetrics(SM_CXSMICON));
    m_nid.hIcon = m_hIcon;

    if (m_visible) {
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
#else
    Q_UNUSED(icon);
#endif
}

void SystemTrayManager::show()
{
#ifdef Q_OS_WIN
    if (!m_visible) {
        Shell_NotifyIconW(NIM_ADD, &m_nid);
        Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
        m_visible = true;
    }
#endif
}

void SystemTrayManager::hide()
{
#ifdef Q_OS_WIN
    if (m_visible) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_visible = false;
    }
#endif
    m_updateCheckTimer.stop();
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

void SystemTrayManager::updateTooltip()
{
    QString tooltip = "Makine Launcher";
    if (m_pendingUpdates > 0) {
        tooltip += tr(" — %1 güncelleme mevcut").arg(m_pendingUpdates);
    }

#ifdef Q_OS_WIN
    wcsncpy_s(m_nid.szTip, reinterpret_cast<const wchar_t*>(tooltip.utf16()),
              _TRUNCATE);
    if (m_visible) {
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
#endif
}
