/**
 * @file systemtraymanager.cpp
 * @brief Native Win32 system tray implementation (no Qt6Widgets)
 * @copyright (c) 2026 MakineAI Team
 */

#include "systemtraymanager.h"
#include <QCoreApplication>
#include <QImage>
#include <QPixmap>

#ifdef Q_OS_WIN

static const UINT WM_TRAYICON = WM_APP + 1;
static const wchar_t kTrayWindowClass[] = L"MakineAI_TrayMsgWindow";

// Static instance pointer for window proc callback
static SystemTrayManager *s_instance = nullptr;

LRESULT CALLBACK SystemTrayManager::trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON && s_instance) {
        s_instance->handleTrayMessage(lParam);
        return 0;
    }
    if (msg == WM_COMMAND && s_instance) {
        switch (LOWORD(wParam)) {
        case IdShow:    emit s_instance->showWindowRequested(); break;
        case IdSettings: emit s_instance->settingsRequested(); break;
        case IdQuit:    emit s_instance->quitRequested(); break;
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
        emit showWindowRequested();
        break;
    case WM_RBUTTONUP:
        showContextMenu();
        break;
    case NIN_BALLOONUSERCLICK:
        emit showWindowRequested();
        break;
    }
}

void SystemTrayManager::showContextMenu()
{
    if (!m_contextMenu) return;

    // Required: SetForegroundWindow before TrackPopupMenu, else menu won't dismiss
    SetForegroundWindow(m_msgWindow);

    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(m_contextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_msgWindow, nullptr);

    // Required: post empty message to dismiss menu properly
    PostMessage(m_msgWindow, WM_NULL, 0, 0);
}

HICON SystemTrayManager::qIconToHicon(const QIcon &icon, int size)
{
    QPixmap pm = icon.pixmap(size, size);
    if (pm.isNull()) return nullptr;

    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
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
    wc.lpszClassName  = kTrayWindowClass;
    RegisterClassExW(&wc);

    // Create message-only window (HWND_MESSAGE = invisible, no taskbar entry)
    m_msgWindow = CreateWindowExW(0, kTrayWindowClass, L"MakineAI_TrayMsg",
                                   0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                   GetModuleHandle(nullptr), nullptr);

    // Build context menu
    m_contextMenu = CreatePopupMenu();
    // Title (grayed, acts as header)
    QString title = "MakineAI v" + QCoreApplication::applicationVersion();
    AppendMenuW(m_contextMenu, MF_STRING | MF_GRAYED, 0,
                reinterpret_cast<LPCWSTR>(title.utf16()));
    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_contextMenu, MF_STRING, IdShow,
                reinterpret_cast<LPCWSTR>(tr("MakineAI'ı Aç").utf16()));
    AppendMenuW(m_contextMenu, MF_STRING, IdSettings,
                reinterpret_cast<LPCWSTR>(tr("Ayarlar").utf16()));
    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_contextMenu, MF_STRING, IdQuit,
                reinterpret_cast<LPCWSTR>(tr("Tamamen Kapat").utf16()));

    // Prepare NOTIFYICONDATA
    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = m_msgWindow;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.uVersion         = NOTIFYICON_VERSION_4;
    wcscpy_s(m_nid.szTip, L"MakineAI");
#endif

    // Background update check timer (disabled by default)
    m_updateCheckTimer.setInterval(6 * 60 * 60 * 1000); // 6 hours
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
    if (m_contextMenu) {
        DestroyMenu(m_contextMenu);
    }
    if (m_msgWindow) {
        DestroyWindow(m_msgWindow);
    }
    UnregisterClassW(kTrayWindowClass, GetModuleHandle(nullptr));
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
#ifdef Q_OS_WIN
    if (!m_visible) return;

    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    nid.uTimeout = static_cast<UINT>(durationMs);

    wcsncpy_s(nid.szInfoTitle, reinterpret_cast<const wchar_t*>(title.utf16()),
              _TRUNCATE);
    wcsncpy_s(nid.szInfo, reinterpret_cast<const wchar_t*>(message.utf16()),
              _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &nid);
#else
    Q_UNUSED(title); Q_UNUSED(message); Q_UNUSED(durationMs);
#endif
}

void SystemTrayManager::enterServiceMode()
{
    if (m_serviceMode) return;
    m_serviceMode = true;
    updateTooltip();
    emit serviceModeChanged(true);
}

void SystemTrayManager::exitServiceMode()
{
    if (!m_serviceMode) return;
    m_serviceMode = false;
    updateTooltip();
    emit serviceModeChanged(false);
}

void SystemTrayManager::updateTooltip()
{
    QString tooltip = "MakineAI";
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
