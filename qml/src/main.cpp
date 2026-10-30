// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file main.cpp
 * @brief Makine Launcher QML Application Entry Point
 * @copyright (c) 2026 MakineCeviri Team
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
#include <QQuickItem>
#include <QQuickGraphicsConfiguration>
#include <QThread>
#include <QThreadPool>
#include <QLibrary>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QEventLoop>
#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUrlQuery>
#include "services/profiler.h"
#include "services/rendergovernor.h"
#include "services/crashreporter.h"
#include "services/telemetryrules.h"
#include <QLoggingCategory>
#include <cstdlib>

Q_LOGGING_CATEGORY(lcApp, "makine.app")

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>     // EmptyWorkingSet
#include <dwmapi.h>    // DwmExtendFrameIntoClientArea
#include <QAbstractNativeEventFilter>
#include <cmath>
#include <atomic>
#include <array>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include <QFontMetricsF>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>

// Eliminate DWM non-client border on frameless windows.
// Without this, Windows adds an asymmetric invisible border (~8px left, ~0px right)
// that shifts the contentItem and makes centered content appear off-center.
class FramelessFilter : public QAbstractNativeEventFilter {
public:
    static constexpr double kAspectRatio = 900.0 / 620.0; // minWidth / minHeight

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);

        if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
            *result = 0;
            return true;
        }

        // Native aspect ratio lock — zero QML bindings, zero stutter
        if (msg->message == WM_SIZING) {
            auto* rect = reinterpret_cast<RECT*>(msg->lParam);
            int w = rect->right - rect->left;
            int h = rect->bottom - rect->top;

            switch (msg->wParam) {
            case WMSZ_LEFT: case WMSZ_RIGHT:
            case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:
            case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOMRIGHT:
                rect->bottom = rect->top + static_cast<int>(w / kAspectRatio);
                break;
            case WMSZ_TOP: case WMSZ_BOTTOM:
                rect->right = rect->left + static_cast<int>(h * kAspectRatio);
                break;
            }
            *result = TRUE;
            return true;
        }

        return false;
    }
};

// DWM window style — adapts to Windows version
namespace {

DWORD getWindowsBuildNumber() {
    OSVERSIONINFOW ovi{};
    ovi.dwOSVersionInfoSize = sizeof(ovi);
    auto RtlGetVersion = reinterpret_cast<LONG(WINAPI*)(OSVERSIONINFOW*)>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (RtlGetVersion) RtlGetVersion(&ovi);
    return ovi.dwBuildNumber;
}

// DWMWA constants not in older SDK headers
constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
constexpr DWORD DWMWA_BORDER_COLOR = 34;
constexpr DWORD DWMWA_CAPTION_COLOR = 35;
constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE = 38;

// DWM_WINDOW_CORNER_PREFERENCE values
constexpr DWORD DWMWCP_ROUND = 2;

// DWM_SYSTEMBACKDROP_TYPE values
constexpr DWORD DWMSBT_MAINWINDOW = 2; // Mica
constexpr DWORD DWMSBT_TABBEDWINDOW = 4; // Mica Alt

void configureWindowStyle(HWND hwnd) {
    DWORD build = getWindowsBuildNumber();

    // Dark mode frame — W10 1903+ (build >= 18362) and all W11
    if (build >= 18362) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                              &darkMode, sizeof(darkMode));
    }

    if (build >= 22000) {
        // ── Windows 11 ──
        // Rounded corners (DWM handles clipping automatically)
        DWORD cornerPref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &cornerPref, sizeof(cornerPref));

        // Mica backdrop — the signature W11 frosted material
        DWORD backdropType = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropType, sizeof(backdropType));

        // Extend DWM frame into entire client area (needed for Mica to render)
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        // Dark border color
        COLORREF borderColor = RGB(30, 30, 35);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR,
                              &borderColor, sizeof(borderColor));
    }
    // Windows 10 — no special treatment needed for frameless windows.
    // DWM provides standard sharp-cornered dark frame automatically
    // when DWMWA_USE_IMMERSIVE_DARK_MODE is set.
}

} // namespace

// Native splash — shown at once while QML loads. It runs its own message loop
// on a dedicated thread, so it keeps moving while the main thread blocks in
// QML create() (~4 seconds).
//
// A layered window (per-pixel alpha: truly round corners and a soft shadow)
// in the launcher's own look, read from its settings: the page in its dark or
// light mode tinted by the accent the user picked, the two soft orbs of that
// accent where AppBackground puts them, the rim a touch darker, a hairline
// lit along the top and the foot. On it the logo — its colours flowing as on
// Discord (logo_anim.webp) — in a soft glow of the accent, the name as the
// pages title themselves, and along the foot a line in the accent fed by the
// startup steps (setStatus), the step's words on its left, the version on its
// right. It rises in; close() fills the line and fades it out over the window
// that has just drawn its first frame, without holding the caller.
// Qt paints every frame into a QImage (premultiplied BGRA, what
// UpdateLayeredWindow takes). Text is laid out on the main thread, so the
// splash thread only paints images and gradients.
class SplashWindow {
public:
    SplashWindow() = default;
    ~SplashWindow() {
        close();
        if (m_thread) {
            WaitForSingleObject(m_thread, 3000);
            CloseHandle(m_thread);
        }
    }
    SplashWindow(const SplashWindow&) = delete;
    SplashWindow& operator=(const SplashWindow&) = delete;

    // Main thread, after configureApplication() (QGuiApplication and the
    // fonts exist). Starts the splash thread with the card and the logo,
    // then lays out the words: the first text set loads the font database
    // (~300 ms), which the window does not wait for.
    void show() {
        m_dpr = qMax(1.0, GetDpiForSystem() / 96.0);
        m_pw = qRound(kW * m_dpr);
        m_ph = qRound(kH * m_dpr);
        m_logoPx = qRound(kLogo * m_dpr);

        // The launcher's mode and accent (SettingsManager keeps them; Theme.qml
        // has the same table: fill on dark, label on dark, fill on light)
        {
            QSettings settings(QStringLiteral("MakineCeviri"), QStringLiteral("Makine-Launcher"));
            m_dark = settings.value(QStringLiteral("appearance/isDarkMode"), true).toBool();
            const QString preset = settings.value(QStringLiteral("appearance/accentPreset"),
                                                  QStringLiteral("purple")).toString();
            static const QHash<QString, std::array<QRgb, 3>> kAccents = {
                {QStringLiteral("purple"), {0xFF8B5CF6, 0xFFA78BFA, 0xFF7C3AED}},
                {QStringLiteral("blue"),   {0xFF3B82F6, 0xFF60A5FA, 0xFF2563EB}},
                {QStringLiteral("indigo"), {0xFF6366F1, 0xFF818CF8, 0xFF4F46E5}},
                {QStringLiteral("sky"),    {0xFF0EA5E9, 0xFF38BDF8, 0xFF0284C7}},
                {QStringLiteral("teal"),   {0xFF14B8A6, 0xFF2DD4BF, 0xFF0D9488}},
                {QStringLiteral("green"),  {0xFF22C55E, 0xFF4ADE80, 0xFF16A34A}},
                {QStringLiteral("amber"),  {0xFFF59E0B, 0xFFFBBF24, 0xFFD97706}},
                {QStringLiteral("rose"),   {0xFFEC4899, 0xFFF472B6, 0xFFDB2777}},
                {QStringLiteral("red"),    {0xFFEF4444, 0xFFF87171, 0xFFDC2626}},
                {QStringLiteral("black"),  {0xFF71717A, 0xFFA1A1AA, 0xFF52525B}},
            };
            const auto a = kAccents.value(preset, kAccents.value(QStringLiteral("purple")));
            m_accent = QColor::fromRgb(m_dark ? a[0] : a[2]);
            m_accentLight = QColor::fromRgb(m_dark ? a[1] : a[2]);
            m_accentDark = m_accent.darker(115);
            // Theme.bgPrimary: near-black taking a little of the accent
            const auto night = [this](int r, int g, int b, qreal k) {
                return QColor::fromRgbF(r / 255.0 * (1 - k) + m_accent.redF() * k,
                                        g / 255.0 * (1 - k) + m_accent.greenF() * k,
                                        b / 255.0 * (1 - k) + m_accent.blueF() * k);
            };
            m_bg = m_dark ? night(7, 7, 10, 0.035) : QColor(0xF4, 0xF4, 0xF6);
        }

        const QImage logo(QStringLiteral(":/qt/qml/MakineLauncher/resources/images/logo.png"));
        if (!logo.isNull())
            m_logo = logo.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                         .scaled(m_logoPx, m_logoPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // The card's shadow and the logo's, softened once
        m_cardShadow = shadow(QSizeF(kW, kH), kCard.translated(0, 12), kRadius, m_dark ? 150 : 60, 18);
        const qreal ls = kLogo * 1.8;
        m_logoShadow = shadow(QSizeF(ls, ls), QRectF((ls - kLogo) / 2, (ls - kLogo) / 2 + 7, kLogo, kLogo),
                              kLogo * 0.225, 170, 9);

        m_clock.start();
        m_thread = CreateThread(nullptr, 0, splashThreadProc, this, 0, &m_threadId);
        // Wait until the window exists so setStatus/close reach it
        while (!m_ready.load(std::memory_order_acquire))
            Sleep(1);

        QImage title = text(QStringLiteral("Makine Çeviri"), font(26, QFont::Bold, true, -0.4), ink(m_dark ? 245 : 235));
        QImage subtitle = text(QStringLiteral("Türkçe yama başlatıcısı"), font(14, QFont::Medium, true), ink(m_dark ? 140 : 135));
        QImage version = text(QStringLiteral("v" MAKINE_APP_VERSION), font(11, QFont::Medium, false), ink(m_dark ? 95 : 105));
        std::lock_guard lk(m_statusMutex);
        m_title = std::move(title);
        m_subtitle = std::move(subtitle);
        m_version = std::move(version);
        m_textAt = m_clock.elapsed();
    }

    // Main thread: the step's words, laid out here; the splash slides them in
    void setStatus(const wchar_t* status) {
        QImage line = text(QString::fromWCharArray(status), font(12, QFont::Normal, false), ink(m_dark ? 150 : 140));
        std::lock_guard lk(m_statusMutex);
        m_prevStatus = std::move(m_status);
        m_status = std::move(line);
        m_statusAt = m_clock.elapsed();
        ++m_step;
    }

    // No-op — kept for API compatibility (splash pumps its own messages)
    void pumpMessages() {}

    void waitMinimumDisplay(DWORD minMs) {
        if (!m_clock.isValid()) return;
        const qint64 elapsed = m_clock.elapsed();
        if (elapsed < minMs)
            Sleep(DWORD(minMs - elapsed));
    }

    // Fills the line and fades out on the splash thread; returns at once
    // (the destructor waits for the thread).
    void close() {
        if (m_hwnd && !m_closing.exchange(true))
            PostMessageW(m_hwnd, WM_APP + 2, 0, 0);
    }

private:
    // Window and card, in logical pixels (the card leaves room for its shadow)
    static constexpr qreal kW = 520, kH = 352;
    static constexpr QRectF kCard{30, 24, 460, 290};
    static constexpr qreal kRadius = 16;
    static constexpr qreal kLogo = 84;
    static constexpr qreal kPad = 30;
    // Startup steps that call setStatus (the line's share per step)
    static constexpr int kSteps = 14;
    // Seen at least this long, so the intro is never cut off on a fast start
    // (the outro waits for it; close() itself never does)
    static constexpr qint64 kMinShowMs = 1400;

    // Text in the mode's ink: white on the night, near-black on the day
    QColor ink(int alpha) const { return m_dark ? QColor(255, 255, 255, alpha) : QColor(18, 18, 22, alpha); }
    QColor withAlpha(QColor c, qreal a) const { c.setAlphaF(qBound(0.0, a, 1.0)); return c; }

    static qreal clamp01(qreal x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
    // Progress 0..1 of a motion that starts `delay` ms in and lasts `duration`
    static qreal phase(qint64 ms, int delay, int duration) { return clamp01(qreal(ms - delay) / duration); }
    static qreal outCubic(qreal x) { x = 1 - x; return 1 - x * x * x; }
    static qreal outExpo(qreal x) { return x >= 1 ? 1 : 1 - std::pow(2.0, -10 * x); }
    static qreal outBack(qreal x) { const qreal c = 1.70158; x -= 1; return 1 + (c + 1) * x * x * x + c * x * x; }

    static QFont font(int px, QFont::Weight weight, bool display, qreal tracking = 0) {
        QFont f;
        f.setFamilies({display ? QStringLiteral("Inter Display") : QStringLiteral("Inter"),
                       QStringLiteral("Inter"), QStringLiteral("Segoe UI")});
        f.setPixelSize(px);
        f.setWeight(weight);
        f.setHintingPreference(QFont::PreferNoHinting);
        if (tracking != 0)
            f.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
        return f;
    }

    QImage text(const QString& s, const QFont& f, const QColor& color) const {
        const QFontMetricsF fm(f);
        const QSizeF size(std::ceil(fm.horizontalAdvance(s)) + 6, std::ceil(fm.height()) + 2);
        QImage img((size * m_dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(m_dpr);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setFont(f);
        p.setPen(color);
        p.drawText(QRectF(QPointF(0, 0), size), Qt::AlignCenter, s);
        return img;
    }

    // A rounded rect drawn small and scaled back up: a cheap soft shadow
    QImage shadow(const QSizeF& logical, const QRectF& rect, qreal radius, int alpha, qreal blur) const {
        const QSize px = (logical * m_dpr).toSize();
        QImage img(px, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.scale(m_dpr, m_dpr);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, alpha));
            p.drawRoundedRect(rect, radius, radius);
        }
        const qreal k = qMax(2.0, blur * m_dpr / 2);
        const QImage small = img.scaled(qMax(1, int(px.width() / k)), qMax(1, int(px.height() / k)),
                                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QImage soft = small.scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        soft.setDevicePixelRatio(m_dpr);
        return soft;
    }

    // An image of text at x (its left, middle or right by `align`: -1, 0, 1), centred on cy
    static void drawAt(QPainter& p, const QImage& img, qreal x, qreal cy, qreal opacity = 1, int align = 0) {
        if (img.isNull() || opacity <= 0) return;
        const QSizeF s = img.deviceIndependentSize();
        const qreal left = align < 0 ? x : align > 0 ? x - s.width() : x - s.width() / 2;
        p.setOpacity(opacity);
        p.drawImage(QPointF(left, cy - s.height() / 2), img);
        p.setOpacity(1);
    }

    // The logo's frames come in one per tick, so the first loop never waits
    void decodeFrame(qint64 ms) {
        if (m_framesDone) return;
        if (!m_reader) {
            m_reader = std::make_unique<QImageReader>(
                QStringLiteral(":/qt/qml/MakineLauncher/resources/images/logo_anim.webp"));
            if (!m_reader->canRead()) { m_reader.reset(); m_framesDone = true; return; }
        }
        QImage frame;
        const bool more = m_reader->read(&frame);
        if (more)
            m_frames.push_back(frame.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                                    .scaled(m_logoPx, m_logoPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        if (!more || (m_reader->imageCount() > 0 && int(m_frames.size()) >= m_reader->imageCount())) {
            m_reader.reset();
            m_framesDone = true;
            m_framesFrom = ms;
            if (m_frames.size() < 2) m_frames.clear();
        }
    }

    void render() {
        const qint64 ms = m_clock.elapsed();
        const qreal t = ms / 1000.0;
        const qreal dt = m_lastMs ? qMax<qint64>(1, ms - m_lastMs) : 16;
        m_lastMs = ms;
        decodeFrame(ms);

        int step;
        qint64 statusAt, textAt;
        QImage status, prevStatus, title, subtitle, version;
        {
            std::lock_guard lk(m_statusMutex);
            step = m_step;
            statusAt = m_statusAt;
            status = m_status;
            prevStatus = m_prevStatus;
            title = m_title;
            subtitle = m_subtitle;
            version = m_version;
            textAt = m_textAt;
        }

        // Intro, and the outro once close() came: the line fills, then the
        // whole fades (not before kMinShowMs)
        const bool closing = m_closeAt >= 0;
        const qint64 fadeFrom = closing ? qMax(m_closeAt + 140, kMinShowMs - 240) : -1;
        qreal windowAlpha = outCubic(phase(ms, 0, 200));
        qreal cardScale = 0.965 + 0.035 * outExpo(phase(ms, 0, 560));
        if (closing) {
            windowAlpha *= 1 - outCubic(phase(ms, int(fadeFrom), 240));
            cardScale += 0.02 * outCubic(phase(ms, int(fadeFrom) - 20, 260));
        }

        // The line: the steps so far, creeping towards the next while one runs
        qreal goal = qMin(0.97, qreal(step) / kSteps
                     + (1.0 / kSteps) * 0.8 * (1 - std::exp(-(ms - statusAt) / 1800.0)));
        if (closing) goal = 1;
        m_progress += (goal - m_progress) * (1 - std::exp(-dt / (closing ? 70.0 : 150.0)));

        QImage canvas(m_bits, m_pw, m_ph, m_pw * 4, QImage::Format_ARGB32_Premultiplied);
        canvas.setDevicePixelRatio(m_dpr);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        const QPointF c = kCard.center();
        p.translate(c);
        p.scale(cardScale, cardScale);
        p.translate(-c);

        p.drawImage(QPointF(0, 0), m_cardShadow);

        // The card: the launcher's page, its accent's orbs, the rim darker
        QPainterPath card;
        card.addRoundedRect(kCard, kRadius, kRadius);
        p.fillPath(card, m_bg);

        p.save();
        p.setClipPath(card);
        const qreal glow = outCubic(phase(ms, 60, 900));
        const qreal dim = qMax(kCard.width(), kCard.height());
        const auto orb = [&](QPointF at, qreal r, const QColor& tint, qreal strength) {
            QRadialGradient g(at, r);
            g.setColorAt(0.00, withAlpha(tint, strength * glow));
            g.setColorAt(0.12, withAlpha(tint, strength * 0.72 * glow));
            g.setColorAt(0.30, withAlpha(tint, strength * 0.30 * glow));
            g.setColorAt(0.55, withAlpha(tint, strength * 0.08 * glow));
            g.setColorAt(1.00, withAlpha(tint, 0));
            p.fillRect(kCard, g);
        };
        const qreal k = m_dark ? 1.0 : 0.6;
        orb(QPointF(kCard.left() + kCard.width() * 0.78 + 10 * std::sin(t * 0.45),
                    kCard.top() + kCard.height() * 0.12 + 8 * std::cos(t * 0.40)), dim * 0.85, m_accentLight, 0.26 * k);
        orb(QPointF(kCard.left() + kCard.width() * 0.16 + 10 * std::cos(t * 0.35),
                    kCard.top() + kCard.height() * 0.92 + 8 * std::sin(t * 0.50)), dim * 0.75, m_accentDark, 0.22 * k);
        if (m_dark) {
            QRadialGradient rim(QPointF(kCard.center().x(), kCard.top() + kCard.height() * 0.45), dim * 0.95);
            rim.setColorAt(0.55, QColor(0, 0, 0, 0));
            rim.setColorAt(0.85, QColor(0, 0, 0, 26));
            rim.setColorAt(1.00, QColor(0, 0, 0, 56));
            p.fillRect(kCard, rim);
        }
        p.restore();

        // Hairline, lit along the top and (fainter) the foot — the panel's
        p.setPen(QPen(m_dark ? QColor(255, 255, 255, 17) : QColor(0, 0, 0, 18), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(kCard.adjusted(0.5, 0.5, -0.5, -0.5), kRadius - 0.5, kRadius - 0.5);
        const auto litEdge = [&](qreal y, qreal alpha) {
            const QColor lit(255, 255, 255);
            QLinearGradient g(kCard.left() + kRadius, 0, kCard.right() - kRadius, 0);
            g.setColorAt(0.0, withAlpha(lit, 0));
            g.setColorAt(0.3, withAlpha(lit, alpha));
            g.setColorAt(0.7, withAlpha(lit, alpha));
            g.setColorAt(1.0, withAlpha(lit, 0));
            p.fillRect(QRectF(kCard.left() + kRadius, y, kCard.width() - kRadius * 2, 1), g);
        };
        litEdge(kCard.top(), m_dark ? 0.16 : 0.9);
        litEdge(kCard.bottom() - 1, m_dark ? 0.11 : 0.6);

        // The logo in a soft glow of the accent
        const qreal cx = c.x();
        const qreal logoY = kCard.top() + 92;
        const qreal breath = 0.85 + 0.15 * std::sin(t * 1.8);
        {
            const qreal r = kLogo * 0.95;
            QRadialGradient halo(QPointF(cx, logoY + 4), r);
            halo.setColorAt(0.0, withAlpha(m_accentLight, (m_dark ? 0.42 : 0.30) * breath * glow));
            halo.setColorAt(0.5, withAlpha(m_accentLight, (m_dark ? 0.14 : 0.10) * breath * glow));
            halo.setColorAt(1.0, withAlpha(m_accentLight, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(QPointF(cx, logoY + 4), r, r);
        }

        const qreal logoIn = outCubic(phase(ms, 40, 260));
        const qreal s = 0.78 + 0.22 * outBack(phase(ms, 40, 620));
        const QImage& logo = m_framesDone && !m_frames.empty()
            ? m_frames[size_t((ms - m_framesFrom) / 50) % m_frames.size()] : m_logo;
        p.save();
        p.translate(cx, logoY);
        p.scale(s, s);
        p.setOpacity(logoIn);
        p.drawImage(QPointF(-m_logoShadow.deviceIndependentSize().width() / 2,
                            -m_logoShadow.deviceIndependentSize().height() / 2), m_logoShadow);
        if (!logo.isNull())
            p.drawImage(QRectF(-kLogo / 2, -kLogo / 2, kLogo, kLogo), logo);
        p.restore();

        // The name, as a page titles itself, rising in after the logo
        const int words = title.isNull() ? int(ms) + 1000 : int(qMax<qint64>(160, textAt + 16));
        const auto rise = [&](const QImage& img, qreal y, int delay) {
            const qreal k = phase(ms, delay, 640);
            drawAt(p, img, cx, y + 10 * (1 - outExpo(k)), outCubic(phase(ms, delay, 380)));
        };
        rise(title, kCard.top() + 166, words);
        rise(subtitle, kCard.top() + 194, words + 70);

        // Along the foot: a line in the accent, lit from above, a shine running over it
        const qreal barIn = outCubic(phase(ms, words + 120, 380));
        const QRectF track(kCard.left() + kPad, kCard.bottom() - 52, kCard.width() - kPad * 2, 3);
        p.setOpacity(barIn);
        p.setPen(Qt::NoPen);
        p.setBrush(m_dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18));
        p.drawRoundedRect(track, 1.5, 1.5);
        const qreal w = qMax<qreal>(track.height(), track.width() * m_progress);
        const QRectF bar(track.left(), track.top(), w, track.height());
        QLinearGradient fillGrad(bar.topLeft(), bar.topRight());
        fillGrad.setColorAt(0, m_accentDark);
        fillGrad.setColorAt(1, m_accentLight);
        p.setBrush(fillGrad);
        p.drawRoundedRect(bar, 1.5, 1.5);
        // Its glow under the head of the line
        {
            QRadialGradient head(QPointF(bar.right(), bar.center().y()), 18);
            head.setColorAt(0, withAlpha(m_accentLight, 0.45 * barIn));
            head.setColorAt(1, withAlpha(m_accentLight, 0));
            p.setBrush(head);
            p.drawEllipse(QPointF(bar.right(), bar.center().y()), 18, 6);
        }
        const qreal shineX = bar.left() + std::fmod(t * 0.7, 1.6) / 1.6 * (bar.width() + 80) - 40;
        QLinearGradient shine(shineX - 40, 0, shineX + 40, 0);
        shine.setColorAt(0, QColor(255, 255, 255, 0));
        shine.setColorAt(0.5, QColor(255, 255, 255, m_dark ? 130 : 160));
        shine.setColorAt(1, QColor(255, 255, 255, 0));
        p.setBrush(shine);
        p.drawRoundedRect(bar, 1.5, 1.5);
        p.setOpacity(1);

        // Under it: the step on the left, sliding up as the next one comes;
        // the version on the right
        const qreal f = outCubic(phase(ms - statusAt, 0, 260));
        const qreal footY = kCard.bottom() - 28;
        drawAt(p, prevStatus, track.left() - 3, footY - 5 * f, barIn * (1 - f), -1);
        drawAt(p, status, track.left() - 3, footY + 5 * (1 - f), barIn * f, -1);
        drawAt(p, version, track.right() + 3, footY, barIn, 1);
        p.end();

        POINT src{0, 0};
        POINT at{m_x, m_y};
        SIZE size{m_pw, m_ph};
        BLENDFUNCTION bf{AC_SRC_OVER, 0, BYTE(qRound(255 * windowAlpha)), AC_SRC_ALPHA};
        UpdateLayeredWindow(m_hwnd, nullptr, &at, &size, m_memDC, &src, 0, &bf, ULW_ALPHA);

        if (closing && ms >= fadeFrom + 240)
            DestroyWindow(m_hwnd);
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<SplashWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
        case WM_TIMER:
            if (wp == 1 && self) self->render();
            return 0;
        case WM_APP + 2: // close request from main thread
            if (self && self->m_closeAt < 0) self->m_closeAt = self->m_clock.elapsed();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // Splash thread entry point
    static DWORD WINAPI splashThreadProc(LPVOID param) {
        auto* self = static_cast<SplashWindow*>(param);

        WNDCLASSW wc{};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"MakineSplash";
        wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
        RegisterClassW(&wc);

        // Centred in the primary screen's work area
        RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        self->m_x = work.left + (work.right - work.left - self->m_pw) / 2;
        self->m_y = work.top + (work.bottom - work.top - self->m_ph) / 2;

        self->m_hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"MakineSplash", L"",
            WS_POPUP,
            self->m_x, self->m_y, self->m_pw, self->m_ph,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        if (self->m_hwnd) {
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = self->m_pw;
            bmi.bmiHeader.biHeight = -self->m_ph; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            self->m_memDC = CreateCompatibleDC(nullptr);
            self->m_dib = CreateDIBSection(self->m_memDC, &bmi, DIB_RGB_COLORS,
                                           reinterpret_cast<void**>(&self->m_bits), nullptr, 0);
            self->m_oldBmp = SelectObject(self->m_memDC, self->m_dib);

            SetWindowLongPtrW(self->m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (self->m_bits)
                self->render();
            ShowWindow(self->m_hwnd, SW_SHOWNOACTIVATE);
            // ~60 fps
            SetTimer(self->m_hwnd, 1, 15, nullptr);
        }

        // Signal main thread that HWND is ready
        self->m_ready.store(true, std::memory_order_release);
        if (!self->m_hwnd)
            return 0;

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // The window is gone: free what only it needed
        if (self->m_memDC) {
            SelectObject(self->m_memDC, self->m_oldBmp);
            DeleteObject(self->m_dib);
            DeleteDC(self->m_memDC);
        }
        self->m_frames.clear();
        self->m_reader.reset();
        return 0;
    }

    HWND m_hwnd{nullptr};
    HANDLE m_thread{nullptr};
    DWORD m_threadId{0};
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_closing{false};
    QElapsedTimer m_clock;

    // Geometry (physical pixels) and the frame buffer — splash thread
    qreal m_dpr{1};
    int m_pw{0}, m_ph{0}, m_x{0}, m_y{0};
    int m_logoPx{0};
    HDC m_memDC{nullptr};
    HBITMAP m_dib{nullptr};
    HGDIOBJ m_oldBmp{nullptr};
    uchar* m_bits{nullptr};

    // Still parts, made on the main thread before the splash thread starts
    QImage m_logo, m_cardShadow, m_logoShadow;
    // The launcher's look, read from its settings before the thread starts
    bool m_dark{true};
    QColor m_accent, m_accentLight, m_accentDark, m_bg;

    // The flowing logo — splash thread
    std::unique_ptr<QImageReader> m_reader;
    std::vector<QImage> m_frames;
    bool m_framesDone{false};
    qint64 m_framesFrom{0};

    // Motion state — splash thread
    qint64 m_lastMs{0};
    qint64 m_closeAt{-1};
    qreal m_progress{0};

    // The words and the current step — written by the main thread
    std::mutex m_statusMutex;
    QImage m_title, m_subtitle, m_version;
    qint64 m_textAt{0};
    QImage m_status, m_prevStatus;
    qint64 m_statusAt{0};
    int m_step{0};
};
#endif

namespace {
constexpr int kStartupSettleMs = 5000;
}

// Resolve log file path using organized directory layout
#include "services/apppaths.h"
static QString getLogFilePath() {
    QString logDir = makine::AppPaths::logsDir();
    QDir().mkpath(logDir);
    return makine::AppPaths::debugLog();
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

// Anti-RE protection (no-op in debug builds)
#include "services/appprotection.h"

// Backend services
#include "services/gameservice.h"
#include "services/supportedgamesmodel.h"
#include "services/catalogproxymodel.h"
#include "services/settingsmanager.h"
#include "services/backupmanager.h"
#include "services/pluginmanager.h"
#include "services/processscanner.h"
#include "services/systemtraymanager.h"
#include "services/integrityservice.h"
#include "services/batchoperationservice.h"
#include "services/updateservice.h"
#include "services/operationjournal.h"
#include "services/imagecachemanager.h"
#include "services/manifestsyncservice.h"
#include "services/translationdownloader.h"
#include "services/communityservice.h"
#include "services/telemetryservice.h"
#include "services/corebridge.h"
#include "services/translationstatemanager.h"
#include "services/installflowservice.h"
#include "services/companionservice.h"
#ifdef MAKINE_DEV_TOOLS
#include "services/frametimer.h"
#include "services/sceneprofiler.h"
#include "services/memoryprofiler.h"
#endif

// Bring service types into scope for helper function signatures.
// NOLINT: spdlog is not used in this file — no ADL collision risk.
using namespace makine;

// -----------------------------------------------------------------------------
// Forward declarations of helper functions
// -----------------------------------------------------------------------------

// Set Qt environment variables before QGuiApplication is constructed.
static void configureQtEnvironment();

// Single-instance guard. Returns true if this process acquired the slot.
// isPostUpdate: allows extra retries when the previous process is still exiting.
static bool acquireSingleInstance(bool isPostUpdate);

// Configure QGuiApplication properties, fonts, and pipeline-cache env vars.
static void configureApplication(QGuiApplication& app);

// Register QML types and set engine context properties needed before services.
static void configureEngine(QQmlApplicationEngine& engine);

// Phases 1-7: instantiate all backend services, register as context properties,
// and return pointers needed by later phases via out-parameters.
static void createServices(
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    bool isPostUpdate,
    const QElapsedTimer& startupTimer,
#ifdef Q_OS_WIN
    SplashWindow& splash,
#endif
    GameService*& outGameService,
    ManifestSyncService*& outManifestSync,
    ImageCacheManager*& outImageCache,
    SystemTrayManager*& outTrayManager,
    OperationJournal*& outJournal,
    ProcessScanner*& outProcessScanner);

// Wire inter-service signal/slot connections.
static void wireSignals(
    QGuiApplication& app,
    QQmlEngine& engine,
    GameService* gameService,
    ManifestSyncService* manifestSync,
    SystemTrayManager* trayManager,
    ProcessScanner* processScanner);

// Log startup environment diagnostics (graphics backend, Qt version, etc.)
// and attach QML warning/error handlers to the engine.
static void logStartupDiagnostics(QGuiApplication& app, QQmlApplicationEngine& engine);

// Phase 10: configure the root QQuickWindow (sizing, style, pipeline cache,
// first-frame splash teardown, dev-tool attachment).
static void setupRootWindow(
    QObject* rootObject,
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    GameService* gameService,
    ImageCacheManager* imageCache,
    const QElapsedTimer& startupTimer
#ifdef Q_OS_WIN
    , SplashWindow& splash
#endif
);

// Schedule post-startup heap compact + working-set release.
static void scheduleMemoryTrim();

// Wire performance reporting for MAKINE_PERF_ACTIVE builds.
static void setupPerfReporting(QGuiApplication& app, int argc, char* argv[]);

// -----------------------------------------------------------------------------
// Helper function implementations
// -----------------------------------------------------------------------------

static void configureQtEnvironment()
{
    // === HIGH-DPI ROUNDING POLICY ===
    // PassThrough preserves fractional DPI scale (e.g. 125%, 150%) instead
    // of rounding to integer. This prevents layout jumps and font metric
    // shifts when the window crosses between monitors with different DPI
    // (e.g. 4K @ 150% ↔ 1080p @ 100%). Must be set before QGuiApplication.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // === RENDER LOOP ===
    // "threaded" = render on separate thread, overlaps CPU+GPU work.
    // "basic" = single-threaded, best idle efficiency but blocks main thread during render.
    qputenv("QSG_RENDER_LOOP", "threaded");

    // === VSYNC (60 FPS CAP) ===
    // swapInterval=1 locks to monitor refresh (60Hz=60fps, 144Hz capped by governor)
    {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setSwapInterval(1);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    // === GRAPHICS BACKEND ===
    // Apply the user-selected RHI backend (Settings -> Ekran -> Grafik API).
    // Must run before any QQuickWindow is created. Without this, Qt always
    // fell back to its platform default (D3D11 on Windows) regardless of the
    // saved preference, so the "Aktif: ..." label and real rendering never
    // matched the selection. QSG_RHI_BACKEND env override still wins.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND")) {
        QSettings gfxSettings("MakineCeviri", "Makine-Launcher");
        // Force D3D11 unless the user explicitly opted into another backend.
        // Qt 6 picks Vulkan on its own when a Vulkan ICD is present, which
        // crashes ("Failed to initialize graphics backend for Vulkan") on
        // machines whose ICD is missing or broken — common on older Intel
        // iGPUs and freshly-installed Windows boxes. D3D11 ships with every
        // Windows 10+ install, so it is the safe default. Users can opt
        // into Vulkan from Settings -> Performans -> Grafik API.
        //
        // We set QSG_RHI_BACKEND (the authoritative override) AND call
        // setGraphicsApi(); the env var is what Qt's scene graph actually
        // honors, the setter is belt-and-suspenders for API consistency.
        //
        // One-time migration: builds up to v0.1.0-beta rewrote a stored "auto"
        // into "vulkan" and persisted it, so every machine that ever ran an
        // older build carries an explicit "vulkan" on disk. The "auto" default
        // below only applies when the key is absent, so without this reset the
        // D3D11 fix would never reach existing users — precisely the cohort
        // whose launcher fails to start on a missing/broken Vulkan ICD.
        // Runs once; choosing Vulkan again from Settings afterwards sticks.
        if (!gfxSettings.value("performance/graphicsBackendReset", false).toBool()) {
            if (gfxSettings.value("performance/graphicsBackend").toString() == "vulkan")
                gfxSettings.setValue("performance/graphicsBackend", "auto");
            gfxSettings.setValue("performance/graphicsBackendReset", true);
        }

        const QString gfxBackend =
            gfxSettings.value("performance/graphicsBackend", "auto").toString();
        if (gfxBackend == "vulkan") {
            qputenv("QSG_RHI_BACKEND", "vulkan");
            QQuickWindow::setGraphicsApi(QSGRendererInterface::VulkanRhi);
        } else if (gfxBackend == "opengl") {
            qputenv("QSG_RHI_BACKEND", "opengl");
            QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);
        } else {  // "auto", "d3d11", or unknown -> D3D11
            qputenv("QSG_RHI_BACKEND", "d3d11");
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11Rhi);
        }
    }

    // === RELEASE SECURITY ===
#ifdef NDEBUG
    qputenv("QT_QML_NO_DEBUGGER", "1");   // Disable QML debugger in release
    qputenv("QML_DISABLE_DISK_CACHE", "0");
#endif

    // Set org/app names early so QStandardPaths resolves to the correct directory
    // (MakineCeviri/Makine-Launcher) before any path queries.
    QCoreApplication::setOrganizationName("MakineCeviri");
    QCoreApplication::setApplicationName("Makine-Launcher");

    // === QML DISK CACHE ===
    // Pin cache to a stable path so it survives Qt minor version updates.
    // Without this, Qt writes to %TEMP% which may be cleaned between sessions,
    // forcing full QML recompile on every cold start (~200-400ms penalty).
    {
        const QString cacheBase = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        if (!cacheBase.isEmpty())
            qputenv("QML_DISK_CACHE_PATH", (cacheBase + "/qml_cache").toUtf8());
    }

    // Disable RHI debug/validation layers (saves ~10 MB + CPU)
    qputenv("QSG_RHI_DEBUG_LAYER", "0");

    // === MEMORY OPTIMIZATIONS ===
    // Texture atlas: 512x512 = 1 MB/atlas (default 2048x2048 = 16 MB)
    qputenv("QSG_ATLAS_WIDTH", "512");
    qputenv("QSG_ATLAS_HEIGHT", "512");

    // UI Scale: compute effective scale factor before QGuiApplication.
    //
    // Reference logical size: 1280×800.  On screens too small to fit the
    // reference at native DPI, "auto" mode shrinks to fit (floor 0.80).
    // compact = 0.92×, large = 1.12×, auto = adaptive (≤ 1.0).
    // The numeric factor is persisted so configureWindowOnReady() can
    // size the window to the exact matching physical resolution.
    {
        QSettings settings("MakineCeviri", "Makine-Launcher");
        QString uiScale = settings.value("appearance/uiScale", "auto").toString();

        double effectiveScale = 1.0;

        if (uiScale == "manual") {
            effectiveScale = 1.0;  // No scaling — user controls resolution freely
        } else if (uiScale == "compact") {
            effectiveScale = 0.92;
        } else if (uiScale == "large") {
            effectiveScale = 1.12;
        } else {
#ifdef Q_OS_WIN
            // auto: adaptive scale based on logical work area
            // PerMonitorV2: SystemParametersInfo returns logical pixels (DPI-adjusted)
            // No nativeDpr needed — Qt handles physical-to-logical mapping
            RECT wa{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);

            double logicalW = (wa.right - wa.left) * 0.92;
            double logicalH = (wa.bottom - wa.top) * 0.92;
            double fitW = logicalW / 1280.0;
            double fitH = logicalH / 800.0;
            double fitScale = qMin(fitW, fitH);

            if (fitScale < 1.0)
                effectiveScale = qMax(fitScale, 0.80);  // shrink for small screens
            else if (fitScale > 1.08)
                effectiveScale = 1.0 + (fitScale - 1.0) * 0.35;  // gentle grow for large screens
#endif
        }

        if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR") && effectiveScale != 1.0)
            qputenv("QT_SCALE_FACTOR", QByteArray::number(effectiveScale, 'f', 3));

        settings.setValue("appearance/_appliedUiScale", uiScale);
        settings.setValue("appearance/_appliedScaleFactor", effectiveScale);
    }
}

static bool acquireSingleInstance(bool isPostUpdate)
{
    // When launched with --post-update, the old process may still be exiting.
    // Retry a few times to allow the kernel to release the shared memory handle.
    const int maxRetries = isPostUpdate ? 10 : 1;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QSharedMemory cleanupMemory("MakineLauncher_SingleInstance_Guard");
        if (cleanupMemory.attach())
            cleanupMemory.detach();

        QSharedMemory testGuard("MakineLauncher_SingleInstance_Guard");
        if (testGuard.create(1))
            return true;

        if (attempt + 1 < maxRetries)
#ifdef Q_OS_WIN
            Sleep(500);  // Wait 500ms before retry
#else
            QThread::msleep(500);
#endif
    }

    return false;
}

static void configureApplication(QGuiApplication& app)
{
    // Set dynamically after SettingsManager init (based on minimizeToTray)
    app.setApplicationName("Makine-Launcher");
    app.setApplicationDisplayName(QStringLiteral("Makine \u00C7eviri - Makine Launcher"));
    app.setApplicationVersion(MAKINE_APP_VERSION);
    app.setOrganizationName("MakineCeviri");
    app.setOrganizationDomain("makineceviri.org");
    app.setWindowIcon(QIcon(":/qt/qml/MakineLauncher/resources/images/logo.png"));
    QQuickStyle::setStyle("Basic");

    int interRegular = QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/Inter-Bold.ttf");
    // The UI's typeface (Inter's display cut; the text cut stays for the
    // smallest sizes, Dimensions.fontText) and the icon glyphs (Icons.qml)
    int interDisplay = QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/InterDisplay-Regular.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/InterDisplay-Medium.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/InterDisplay-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/InterDisplay-Bold.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/MakineIcons.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/MakineLauncher/resources/fonts/MakineSymbols.ttf");

    QString fontFamily = interDisplay >= 0 ? "Inter Display" : interRegular >= 0 ? "Inter" : "Segoe UI";
    QFont defaultFont(fontFamily, 10);
    // A character the display subset lacks comes from the full text cut
    defaultFont.setFamilies({fontFamily, QStringLiteral("Inter")});
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(defaultFont);

    // Pipeline cache: save compiled shaders so subsequent launches skip compilation stutter
    {
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        qputenv("QSG_RHI_PIPELINE_CACHE_SAVE", (cacheDir + "/pipeline_cache.bin").toUtf8());
        qputenv("QSG_RHI_PIPELINE_CACHE_LOAD", (cacheDir + "/pipeline_cache.bin").toUtf8());
    }

    // Thread pool: 4 threads balances async throughput vs memory (4 MB stack total).
    // Default CPU count (8) is excessive; 2 was too constrained during startup
    // when async Loaders + image downloads + CDN fetch all compete.
    QThreadPool::globalInstance()->setMaxThreadCount(4);
}

static void configureEngine(QQmlApplicationEngine& engine)
{
    // Dev tools availability (must be set BEFORE QML creation)
#ifdef MAKINE_DEV_TOOLS
    engine.rootContext()->setContextProperty("devToolsEnabled", true);
#else
    engine.rootContext()->setContextProperty("devToolsEnabled", false);
#endif


    // Register model types for QML.
    // Manual registration — works in both shared and static Qt builds.
    // (QML_ELEMENT/QML_SINGLETON relies on linker keeping registration code,
    //  which --gc-sections strips in static builds.)
    qmlRegisterUncreatableType<makine::SupportedGamesModel>("MakineLauncher", 1, 0,
        "SupportedGamesModel", "Use GameService.supportedGamesModel");
    qmlRegisterType<makine::CatalogProxyModel>("MakineLauncher", 1, 0, "CatalogProxyModel");
}

static void createServices(
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    bool isPostUpdate,
    const QElapsedTimer& startupTimer,
#ifdef Q_OS_WIN
    SplashWindow& splash,
#endif
    GameService*& outGameService,
    ManifestSyncService*& outManifestSync,
    ImageCacheManager*& outImageCache,
    SystemTrayManager*& outTrayManager,
    OperationJournal*& outJournal,
    ProcessScanner*& outProcessScanner)
{
    // ===== Phase 1: Directory structure + configuration =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 1: Directory structure + configuration");
#ifdef Q_OS_WIN
    splash.setStatus(L"Dizin yap\u0131s\u0131 haz\u0131rlan\u0131yor...");
#endif
    AppPaths::migrateFromLegacyRoot();   // MakineLauncher → MakineCeviri/Makine-Launcher
    AppPaths::ensureDirectories();
    AppPaths::migrateFromFlatLayout();
    AppPaths::cleanupLegacyDirs();       // Remove empty legacy dirs

    // Post-update cleanup (delegate to UpdateService)
    if (isPostUpdate) {
        UpdateService::handlePostUpdate();
        logToFile("Post-update cleanup completed");
    }

#ifdef Q_OS_WIN
    splash.setStatus(L"Yap\u0131land\u0131rma y\u00FCkleniyor...");
#endif
    auto* settingsManager = new SettingsManager(&app);
    engine.rootContext()->setContextProperty("SettingsManager", settingsManager);

    // When minimizeToTray is off, closing the last window should quit the app
    app.setQuitOnLastWindowClosed(!settingsManager->minimizeToTray());
    QObject::connect(settingsManager, &SettingsManager::minimizeToTrayChanged, &app,
                     [&app, settingsManager]() {
        app.setQuitOnLastWindowClosed(!settingsManager->minimizeToTray());
    });

    // Reports switched off stop at once; switched on, they resume — Sentry is
    // brought up then if this session started without it.
    QObject::connect(settingsManager, &SettingsManager::sendCrashReportsChanged, &app,
                     [settingsManager]() {
        makine::CrashReporter::setEnabled(settingsManager->sendCrashReports());
    });

    // ===== Phase 2: Image cache =====
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00F6rsel \u00F6nbelle\u011Fi ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* imageCache = new ImageCacheManager(&app);
    engine.rootContext()->setContextProperty("ImageCache", imageCache);
    outImageCache = imageCache;

    // ===== Phase 2.5: Manifest sync (loads cached index, starts background sync) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Katalog haz\u0131rlan\u0131yor...");
#endif
    auto* manifestSync = new ManifestSyncService(&app);
    engine.rootContext()->setContextProperty("ManifestSync", manifestSync);
    outManifestSync = manifestSync;
    // syncCatalog() called in Phase 7.5 — loads catalog index before QML creation

    auto* translationDownloader = new TranslationDownloader(&app);
    translationDownloader->setDataPath(makine::AppPaths::packagesDir());
    engine.rootContext()->setContextProperty("TranslationDownloader", translationDownloader);

    auto* translationStateManager = new makine::TranslationStateManager(&app);
    engine.rootContext()->setContextProperty("TranslationStateManager", translationStateManager);

    // UpdateService registered as singleton instance in Phase 7 (below)

    // ===== Phase 3: Game library (construction only — data loads after QML) =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 3: Game library construction");
#ifdef Q_OS_WIN
    splash.setStatus(L"Oyun k\u00FCt\u00FCphanesi haz\u0131rlan\u0131yor...");
#endif
    auto* gameService = new GameService(&app);
    gameService->setManifestSync(manifestSync);
    gameService->setImageCache(imageCache);
    engine.rootContext()->setContextProperty("GameService", gameService);
    outGameService = gameService;
    // initialize() deferred to after first frame render (see Phase 10)
    logToFile(QString("Phase 3 (GameService created) at %1 ms").arg(startupTimer.elapsed()));
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    // ===== Phase 4: Operation journal + recovery =====
#ifdef Q_OS_WIN
    splash.setStatus(L"\u0130\u015Flem g\u00FCnl\u00FC\u011F\u00FC kontrol ediliyor...");
#endif
    auto* journal = new OperationJournal(&app);
    // The game whose interrupted operation could not be fully undone; Main.qml
    // tells the user, who would otherwise not know the game is half patched.
    QString recoveryFailedGameId;
    if (journal->hasPendingOperation()) {
        qCDebug(lcApp) << "OperationJournal: recovering from interrupted operation...";
        const JournalEntry pending = journal->readPendingOperation();
        if (!journal->recover()) {
            // The game may be left half patched; nobody would know otherwise.
            const char* operation = "install";
            switch (pending.type) {
            case OpType::Install:       operation = "install"; break;
            case OpType::Uninstall:     operation = "uninstall"; break;
            case OpType::BackupCreate:  operation = "backup"; break;
            case OpType::BackupRestore: operation = "restore"; break;
            }
            makine::CrashReporter::reportFailure(operation, pending.gameId,
                QStringLiteral("interrupted operation could not be rolled back"));
            recoveryFailedGameId = pending.gameId.isEmpty() ? QStringLiteral("?") : pending.gameId;
        }
    }
    engine.rootContext()->setContextProperty("recoveryFailedGameId", recoveryFailedGameId);
    outJournal = journal;
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif

    // ===== Phase 5: Backup + process monitoring =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Yedekleme sistemi ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* backupManager = new BackupManager(&app);
    backupManager->setJournal(journal);
    engine.rootContext()->setContextProperty("BackupManager", backupManager);

#ifdef Q_OS_WIN
    splash.setStatus(L"S\u00FCre\u00E7 izleyici ba\u015Flat\u0131l\u0131yor...");
#endif
    auto* processScanner = new ProcessScanner(&app);
    processScanner->setGameService(gameService);
    engine.rootContext()->setContextProperty("ProcessScanner", processScanner);
    outProcessScanner = processScanner;

    // ===== Phase 6: Security + integrity =====
    auto* integrityService = new IntegrityService(&app);
    engine.rootContext()->setContextProperty("IntegrityService", integrityService);

    auto* batchService = new BatchOperationService(&app);
    engine.rootContext()->setContextProperty("BatchOperationService", batchService);

    // The website's community wall, read-only (Topluluk page)
    auto* communityService = new CommunityService(&app);
    engine.rootContext()->setContextProperty("CommunityService", communityService);

    // ===== Phase 6b: Plugin system (dev builds only until v2 ships, ADR-0009) =====
#ifdef MAKINE_DEV_TOOLS
    auto* pluginManager = new PluginManager(&app);
    pluginManager->setGameService(gameService);
    pluginManager->setProcessScanner(processScanner);
    pluginManager->start();
    engine.rootContext()->setContextProperty("PluginManager", pluginManager);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, pluginManager, &PluginManager::shutdownAll);
#else
    engine.rootContext()->setContextProperty("PluginManager", nullptr);
#endif

    // ===== Phase 7: Update service + system tray =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 7: Update service + system tray");
#ifdef Q_OS_WIN
    splash.setStatus(L"G\u00FCncelleme servisi haz\u0131rlan\u0131yor...");
#endif
    auto* updateService = UpdateService::create(nullptr, nullptr);
    updateService->setParent(&app);
    // Singleton instance: exposes BOTH the instance AND Q_ENUM(State) values to QML.
    // (setContextProperty only exposes the instance — enum constants resolve to undefined)
    qmlRegisterSingletonInstance("MakineLauncher", 1, 0, "UpdateService", updateService);

    // Startup update check — once, async, unless we just updated
    if (!isPostUpdate)
        updateService->check();

#ifdef Q_OS_WIN
    splash.setStatus(L"Sistem tepsisi yap\u0131land\u0131r\u0131l\u0131yor...");
#endif
    auto* trayManager = new SystemTrayManager(&app);
    trayManager->setIcon(app.windowIcon());
    trayManager->show();
    engine.rootContext()->setContextProperty("SystemTrayManager", trayManager);
    outTrayManager = trayManager;

    // Wire journal to CoreBridge
    CoreBridge::instance()->setJournal(journal);
    engine.rootContext()->setContextProperty("CoreBridge", CoreBridge::instance());

    // Install flow state machine (depends on GameService, TranslationDownloader, ManifestSync, CoreBridge)
    auto* installFlow = new makine::InstallFlowService(
        gameService, translationDownloader, manifestSync,
        CoreBridge::instance(), &app);
    engine.rootContext()->setContextProperty("InstallFlowService", installFlow);

    // Makine, the companion: follows the launcher's work, lives in its window (or out on the desktop)
    auto* companion = new CompanionService(settingsManager, &app);
    companion->watch(gameService, translationDownloader);
    engine.rootContext()->setContextProperty("Companion", companion);

    // B2-03: route TranslationDownloader signals natively so the install
    // pipeline cannot stall when the QML controller is reloaded or GC'd.
    QObject::connect(translationDownloader, &makine::TranslationDownloader::packageReady,
                     installFlow, [installFlow](const QString& appId, const QString& /*dirName*/) {
                         installFlow->onDownloadReady(appId);
                     });
    QObject::connect(translationDownloader, &makine::TranslationDownloader::downloadError,
                     installFlow, &makine::InstallFlowService::onDownloadFailed);
    QObject::connect(translationDownloader, &makine::TranslationDownloader::downloadCancelled,
                     installFlow, [installFlow](const QString& appId) {
                         installFlow->onDownloadFailed(appId, QString{});
                     });

    // Outcome counting (telemetryservice.h). Failures reach the counts through
    // CrashReporter's failure sink; successes and cancels exist only as these
    // signals, which is why no success rate could be computed before.
    QObject::connect(translationDownloader, &makine::TranslationDownloader::packageReady,
                     &app, [](const QString& appId, const QString& /*dirName*/) {
                         makine::TelemetryService::record(QStringLiteral("download"),
                                                          QStringLiteral("ok"), appId);
                     });
    QObject::connect(translationDownloader, &makine::TranslationDownloader::downloadCancelled,
                     &app, [](const QString& appId) {
                         makine::TelemetryService::record(QStringLiteral("download"),
                                                          QStringLiteral("cancel"), appId);
                     });
    QObject::connect(gameService, &GameService::translationInstallCompleted,
                     &app, [](const QString& gameId, bool success, const QString&) {
                         if (success)
                             makine::TelemetryService::record(QStringLiteral("install"),
                                                              QStringLiteral("ok"), gameId);
                     });
    QObject::connect(gameService, &GameService::translationUninstalled,
                     &app, [](const QString& gameId, bool success, const QString&) {
                         if (success)
                             makine::TelemetryService::record(QStringLiteral("uninstall"),
                                                              QStringLiteral("ok"), gameId);
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, manifestSync, [manifestSync]() {
        manifestSync->telemetry()->persist();
    });

    logToFile(QString("Services initialized in %1 ms").arg(startupTimer.elapsed()));
#ifdef Q_OS_WIN
    splash.pumpMessages();
#endif
}

static void wireSignals(
    QGuiApplication& app,
    QQmlEngine& engine,
    GameService* gameService,
    ManifestSyncService* manifestSync,
    SystemTrayManager* trayManager,
    ProcessScanner* processScanner)
{
    // Rebuild process map when game library scan completes.
    // PackageManager is lazy-init (created during first scanAllLibraries),
    // so we inject it + rebuild on every scanCompleted.
    QObject::connect(gameService, &GameService::scanCompleted,
                     processScanner, [processScanner]() {
        processScanner->setPackageManager(makine::CoreBridge::instance()->packageManager());
        processScanner->rebuildProcessMap();
    });

    // Tray quit -> app quit directly (bypass QML round-trip)
    QObject::connect(trayManager, &SystemTrayManager::quitRequested,
                     &app, &QCoreApplication::quit);
    // Qt.quit() in QML (Ctrl+Q) quits the same way. Nothing was connected to
    // QQmlEngine::quit ("no receivers" in the log), so Ctrl+Q did nothing.
    // Queued: a quit asked for from a QML handler runs once it has returned.
    QObject::connect(&engine, &QQmlEngine::quit, &app, &QCoreApplication::quit,
                     Qt::QueuedConnection);

    // Clean shutdown: remove tray icon and drain thread pool before exit.
    // Workers hold the services that are destroyed right after this returns;
    // tearing down under one is a use-after-free. They are not cancelled:
    // a cancelled install drops its journal and leaves the rollback to the
    // event loop, which is not running any more.
    // The window goes first: quit from the tray mid-backup left it on screen,
    // frozen for the wait, and the hang watchdog filed that wait as a hang
    // (Sentry 2026-10-29: the GUI stack in QThreadPool::waitForDone, 8 players).
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [trayManager, gameService]() {
        trayManager->hide();
        for (auto* w : QGuiApplication::topLevelWindows()) w->hide();
        makine::CrashReporter::stopHangWatchdog();
        QThreadPool::globalInstance()->clear();
        if (!QThreadPool::globalInstance()->waitForDone(30000)) {
            // Still copying. Leaving without teardown keeps the operation
            // journal, and the next start rolls the operation back.
            logToFile(QStringLiteral("Shutdown: workers still running after 30 s, exiting without teardown"));
            std::_Exit(0);
        }
        gameService->settleOnQuit();
    });

    // Connect ManifestSync signals BEFORE syncing,
    // so catalogReady/packageDetailReady are never missed.
    QObject::connect(manifestSync, &makine::ManifestSyncService::catalogReady,
        gameService, []() {
            if (auto* bridge = makine::CoreBridge::instance())
                bridge->refreshPackageManifest();
        });
    QObject::connect(manifestSync, &makine::ManifestSyncService::packageDetailReady,
        gameService, [manifestSync](const QString& appId) {
            if (auto* bridge = makine::CoreBridge::instance()) {
                QVariantMap detail = manifestSync->getPackageDetail(appId);
                // A failed fetch reports "ready" with nothing in it. Enriching
                // with {} marked the detail loaded, and the install went on
                // with no recipe (InstallFlowService reports the failure).
                if (detail.isEmpty())
                    return;
                QJsonDocument doc(QJsonObject::fromVariantMap(detail));
                bridge->enrichPackageFromJson(appId, doc.toJson(QJsonDocument::Compact));
            }
        });
}

static void logStartupDiagnostics(QGuiApplication& app, QQmlApplicationEngine& engine)
{
    logToFile("=== Makine Launcher Starting ===");
    logToFile(QString("App version: %1").arg(app.applicationVersion()));
    logToFile(QString("Qt version: %1").arg(qVersion()));
    logToFile(QString("Log file: %1").arg(getLogFilePath()));
    logToFile(QString("QSG_RENDER_LOOP: %1").arg(qEnvironmentVariable("QSG_RENDER_LOOP")));
    {
        QSettings settings("MakineCeviri", "Makine-Launcher");
        auto api = QQuickWindow::graphicsApi();
        QString apiName = api == QSGRendererInterface::Direct3D12 ? "D3D12" :
                          api == QSGRendererInterface::Vulkan     ? "Vulkan" :
                          api == QSGRendererInterface::Direct3D11 ? "D3D11" :
                          api == QSGRendererInterface::OpenGL     ? "OpenGL" :
                          "Auto (RHI default)";
        logToFile(QString("Graphics backend: %1").arg(apiName));
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
}

// Qt 6.11: a Shape on the CurveRenderer (the sidebar's icons, the window's buttons…) draws nothing
// once the window comes back from being minimized or hidden, until its path changes. Switching its
// renderer off and on again rebuilds it; every such Shape under `item` gets that. Returns how many.
static int reviveCurveShapes(QQuickItem* item)
{
    int revived = 0;
    for (const QMetaObject* m = item->metaObject(); m; m = m->superClass()) {
        if (qstrcmp(m->className(), "QQuickShape") != 0) continue;
        const QVariant type = item->property("preferredRendererType");
        if (type.toInt() == 4) { // Shape.CurveRenderer
            item->setProperty("preferredRendererType", 1); // Shape.GeometryRenderer
            item->setProperty("preferredRendererType", type);
            ++revived;
        }
        break;
    }
    for (QQuickItem* child : item->childItems()) revived += reviveCurveShapes(child);
    return revived;
}

static void setupRootWindow(
    QObject* rootObject,
    QGuiApplication& app,
    QQmlApplicationEngine& engine,
    GameService* gameService,
    ImageCacheManager* imageCache,
    const QElapsedTimer& startupTimer
#ifdef Q_OS_WIN
    , SplashWindow& splash
#endif
)
{
    auto* window = qobject_cast<QQuickWindow*>(rootObject);
    if (!window) {
        // Fallback: no window — close splash and initialize game library immediately
#ifdef Q_OS_WIN
        splash.close();
#endif
        gameService->initialize();
        return;
    }

#ifdef Q_OS_WIN
    // DPI-aware window sizing + centered positioning via Win32
    //
    // Physical window size = reference (1280×800) × native DPI × effective scale.
    // The effective scale factor was computed in configureQtEnvironment() and
    // persisted to QSettings — it already accounts for auto-fit on small screens,
    // compact/large user preference, and the 0.80 floor.
    {
        // Window sizing is now handled in QML (Main.qml width/height/x/y).
        // Only apply DWM style here — no MoveWindow to avoid resize cascade.
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        configureWindowStyle(hwnd);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
#endif

    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);

    // Back from the tray or the taskbar: the CurveRenderer's Shapes come back too (reviveCurveShapes)
    QObject::connect(window, &QWindow::visibilityChanged, window, [window, away = false](QWindow::Visibility v) mutable {
        if (v == QWindow::Hidden || v == QWindow::Minimized) { away = true; return; }
        if (!std::exchange(away, false)) return;
        QTimer::singleShot(0, window, [window] { reviveCurveShapes(window->contentItem()); });
    });

    // Graphics configuration: pipeline cache via API
    // (supplements env vars set earlier in configureApplication)
    {
        QQuickGraphicsConfiguration gfxConfig;
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                            + "/pipeline_cache.bin";
        gfxConfig.setPipelineCacheSaveFile(cachePath);
        gfxConfig.setPipelineCacheLoadFile(cachePath);
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

    // FPS governor: active (60fps vsync) → idle (on-demand) → background (0fps)
    auto* renderGovernor = new RenderGovernor(window, &app);
    engine.rootContext()->setContextProperty("RenderGovernor", renderGovernor);

    // Makine moves in once the window exists
    if (auto* companion = qobject_cast<CompanionService*>(engine.rootContext()->contextProperty("Companion").value<QObject*>()))
        companion->attachWindow(window);

    logToFile(QString("Phase 10 (first-frame setup) at %1 ms").arg(startupTimer.elapsed()));

    // Close splash on FIRST rendered frame, then dispatch game library load.
    //
    // Why not processEvents here? Any processEvents() call triggers expired
    // QML timers (from createRootObject), which cascade into 4+ seconds of
    // synchronous model resets. Instead, we let QML start with empty data
    // (loading state), close splash on the first rendered frame, then
    // populate data progressively via the event loop.
    QMetaObject::Connection firstFrameConn;
    firstFrameConn = QObject::connect(window, &QQuickWindow::frameSwapped, &app,
        [firstFrameConn, &splash, &startupTimer, gameService]() mutable {
            // Guard: render thread may queue multiple frameSwapped before
            // the first callback runs (threaded loop pipelining).
            static bool done = false;
            if (done) return;
            done = true;

            MAKINE_ZONE_NAMED("Startup::firstFrame");
            QObject::disconnect(firstFrameConn);
            splash.waitMinimumDisplay(200);
            logToFile(QString("First frame rendered, closing splash at %1 ms")
                          .arg(startupTimer.elapsed()));
            splash.close();
            // Dispatch game library loading — results arrive on subsequent
            // event loop iterations, populating QML progressively.
            gameService->initialize();

        }, Qt::QueuedConnection);

    // Request first frame BEFORE preloading Settings — ensures the render
    // pipeline starts immediately.
    window->requestUpdate();
    logToFile(QString("Phase 10 (callback registered) at %1 ms").arg(startupTimer.elapsed()));

    // Preload Settings page AFTER requestUpdate — first frame render has priority.
    // The async Loader runs on the thread pool, so this won't block the first frame,
    // but ordering ensures the render pipeline gets the earliest possible start.
    splash.setStatus(L"Sayfalar haz\u0131rlan\u0131yor...");
    splash.pumpMessages();
    {
        MAKINE_ZONE_NAMED("Preload::SettingsScreen");
        rootObject->setProperty("_settingsPreload", true);
    }
    logToFile(QString("Settings preloaded at %1 ms").arg(startupTimer.elapsed()));
#endif

    // Tracy: frame boundary marker (one FrameMark per rendered frame)
    QObject::connect(window, &QQuickWindow::afterRendering, window, []() {
        MAKINE_FRAME;
    }, Qt::DirectConnection);

#ifdef MAKINE_DEV_TOOLS
    // Dev-only frame timer: high-precision render pipeline metrics
    auto* frameTimer = new FrameTimer(&app);
    engine.rootContext()->setContextProperty("FrameTimer", frameTimer);
    frameTimer->attachToWindow(window);

    // Dump frame stats on exit (integrates with PerfReporter)
    QObject::connect(&app, &QCoreApplication::aboutToQuit, frameTimer, &FrameTimer::dumpStats);

    // Reset frame timer after startup settles — clears the initial jank spike
    // so ongoing metrics reflect actual runtime performance, not first-frame shaders
    QTimer::singleShot(kStartupSettleMs, frameTimer, &FrameTimer::reset);

    // Scene profiler: transition, interaction, dialog, animation tracking
    auto* sceneProfiler = new SceneProfiler(&app);
    engine.rootContext()->setContextProperty("SceneProfiler", sceneProfiler);

    // Memory profiler: working set, image cache monitoring
    auto* memoryProfiler = new MemoryProfiler(&app);
    memoryProfiler->setImageCacheManager(imageCache);
    engine.rootContext()->setContextProperty("MemoryProfiler", memoryProfiler);

    // Dump profiler reports on exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [sceneProfiler, memoryProfiler, imageCache]() {
#ifdef MAKINE_PERF_ACTIVE
        auto& reporter = makine::PerfReporter::instance();
        reporter.addCustomSection(QStringLiteral("scene"), sceneProfiler->sceneReport());
        reporter.addCustomSection(QStringLiteral("memory"), memoryProfiler->memoryReport());

        // Image cache stats
        QJsonObject imgObj;
        imgObj[QStringLiteral("cache_count")] = imageCache->cachedImageCount();
        imgObj[QStringLiteral("cache_bytes")] = imageCache->cachedImageBytes();
        QVariantMap stats = imageCache->imageStats();
        imgObj[QStringLiteral("total_downloads")] = stats.value(QStringLiteral("downloads")).toInt();
        imgObj[QStringLiteral("cache_hits")] = stats.value(QStringLiteral("cacheHits")).toInt();
        imgObj[QStringLiteral("hit_rate")] = stats.value(QStringLiteral("hitRate")).toDouble();
        imgObj[QStringLiteral("queue_peak")] = stats.value(QStringLiteral("queuePeak")).toInt();
        reporter.addCustomSection(QStringLiteral("images"), imgObj);
#endif
    });
#else
    Q_UNUSED(imageCache)
    Q_UNUSED(engine)
#endif
}

static void scheduleMemoryTrim()
{
    // Trim working set after startup settles (DLL init, type registration, QML
    // compilation pages are no longer needed). Hot pages fault back in microseconds.
#ifdef Q_OS_WIN
    QTimer::singleShot(kStartupSettleMs, []() {
        // No HeapCompact over GetProcessHeaps(): those handles belong to other
        // modules, one can be destroyed before the call reaches it, and the
        // startup scan is running right then (fatals MAKINE-LAUNCHER-3C, -1V).
        // Release working set: cold pages (DLL init, QML compile) go to standby list
        EmptyWorkingSet(GetCurrentProcess());
        logToFile("Working set trimmed");
    });
#endif
}

static void setupPerfReporting(QGuiApplication& app, int argc, char* argv[])
{
#ifdef MAKINE_PERF_ACTIVE
    makine::PerfReporter::instance().setMainThread();

    // --profile-duration=N: auto-quit after N seconds (automated profiling)
    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);
        if (arg.startsWith("--profile-duration=")) {
            int secs = arg.mid(19).toInt();
            if (secs > 0) {
                logToFile(QString("Profile mode: auto-quit in %1 seconds").arg(secs));
                QTimer::singleShot(secs * 1000, &app, [&app]() {
                    logToFile("Profile duration reached, exiting...");
                    // Close all windows first (bypasses setQuitOnLastWindowClosed)
                    for (auto* w : QGuiApplication::topLevelWindows())
                        w->close();
                    app.exit(0);
                });
            }
        }
    }

    // Dump performance report on exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        QString reportPath = makine::AppPaths::perfReportFile();
        makine::PerfReporter::instance().dumpReport(reportPath);
        logToFile(QString("Performance report saved to: %1").arg(reportPath));
    });
#else
    Q_UNUSED(app)
    Q_UNUSED(argc)
    Q_UNUSED(argv)
#endif
}

// -----------------------------------------------------------------------------
// main() — high-level orchestrator
// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    configureQtEnvironment();

    // Parse command-line flags before QGuiApplication
    bool isPostUpdate = false;
    bool startMinimized = false;
    bool scanSelfTest = false;
#ifdef MAKINE_DEV_TOOLS
    bool telemetrySelfTest = false;
#endif
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--post-update") == 0) {
            isPostUpdate = true;
        } else if (strcmp(argv[i], "--minimized") == 0) {
            startMinimized = true;
        } else if (strcmp(argv[i], "--selftest-scan") == 0) {
            scanSelfTest = true;
#ifdef MAKINE_DEV_TOOLS
        } else if (strcmp(argv[i], "--selftest-telemetry") == 0) {
            telemetrySelfTest = true;
#endif
        }
    }

    // Single-instance guard (must run before QGuiApplication on Windows).
    // The scan self-test stays behind it: initializing Core decrypts the
    // database to disk and re-encrypts it on shutdown, so a second process
    // doing that while the app is running would race the live instance over
    // the same file. A diagnostic must not be able to corrupt user data —
    // the user closes the launcher first.
    if (!acquireSingleInstance(isPostUpdate)) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr,
            L"Makine Launcher zaten \u00e7al\u0131\u015f\u0131yor.\n\n"
            L"L\u00fctfen sistem tepsisindeki simgeyi kontrol edin "
            L"veya g\u00f6rev y\u00f6neticisinden kapat\u0131n.",
            L"Makine Launcher",
            MB_OK | MB_ICONWARNING);
#endif
        return 0;
    }

    // Persistent guard — lives for the lifetime of the process
    QSharedMemory singleInstanceGuard("MakineLauncher_SingleInstance_Guard");
    singleInstanceGuard.create(1);

    QGuiApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Eliminate asymmetric DWM border on frameless window
    static FramelessFilter framelessFilter;
    app.installNativeEventFilter(&framelessFilter);
#endif

    // === Phase 0: Crash reporting (as early as possible after QApp) ===
    // Only with the user's consent (Ayarlar → Genel → "Çökme ve Hata
    // Raporlarını Gönder"). SettingsManager does not exist yet, so the stored
    // value is read directly. An opted-out session does not start Sentry at
    // all: no session, no crash handler, no replay of what an earlier run left
    // queued.
    if (QSettings("MakineCeviri", "Makine-Launcher")
            .value("general/sendCrashReports", true).toBool())
        makine::CrashReporter::initialize();
    makine::CrashReporter::installQtMessageHandler();
    makine::CrashReporter::setFailureSink([](const char* op, const QString& subject,
                                             const QString& side, const QString& reason) {
        makine::TelemetryService::recordFailure(QString::fromLatin1(op), subject, side, reason);
    });
    makine::TelemetryService::record(QStringLiteral("session"), QStringLiteral("ok"));

#ifdef MAKINE_DEV_TOOLS
    // Telemetry self-test (dev builds only, opt-in via --selftest-telemetry).
    //
    // Reporting can be broken in ways that compile perfectly: the DSN is passed
    // through the MAKINE_SENTRY_DSN environment variable at configure time, so
    // building without loading .env yields SENTRY_DSN="" and every report
    // silently goes nowhere. "It builds" is not evidence that reports arrive.
    //
    // This sends one event through the real path — same initialize(), same
    // reportFailure(), same transport — then flushes and exits, so the chain
    // can be verified on demand instead of assumed.
    if (telemetrySelfTest) {
        // Breadcrumbs travel with the event and used to carry paths verbatim,
        // so a live event leaked the Windows user name while the message body
        // next to it was correctly redacted. Planting a known name here makes
        // the redaction verifiable end to end: the event in Sentry must show
        // "[redacted]" and must not contain "selftest_user_name".
        makine::CrashReporter::addBreadcrumb(
            "selftest",
            "breadcrumb path check: C:\\Users\\selftest_user_name\\AppData\\Local\\pkg",
            "warning");
        makine::CrashReporter::reportFailure(
            "selftest", QStringLiteral("telemetry"),
            QStringLiteral("telemetry self-test event: DSN, init and transport reachable; "
                           "message path check: C:\\Users\\selftest_user_name\\AppData"));
        // MAKINE_SELFTEST_TOGGLE also proves the runtime switch (Ayarlar → "Çökme
        // ve Hata Raporlarını Gönder"): a report made while it is off must never
        // reach the server, one made after it is back on must. The pause lets the
        // first event leave before the switch, since switching off also drops
        // whatever is still queued.
        if (qEnvironmentVariableIsSet("MAKINE_SELFTEST_TOGGLE")) {
            QThread::sleep(2);
            makine::CrashReporter::setEnabled(false);
            makine::CrashReporter::reportFailure(
                "selftest", QStringLiteral("telemetry"),
                QStringLiteral("telemetry self-test toggle: made while reports are OFF"));
            makine::CrashReporter::setEnabled(true);
            makine::CrashReporter::reportFailure(
                "selftest", QStringLiteral("telemetry"),
                QStringLiteral("telemetry self-test toggle: made after reports are ON again"));
        }
        // MAKINE_SELFTEST_QML proves the QML script-error path through the
        // real message handler. The line number changes per run because the
        // gate keys QML errors by file:line for a week.
        if (qEnvironmentVariableIsSet("MAKINE_SELFTEST_QML")) {
            qWarning("qrc:/qt/qml/MakineLauncher/qml/selftest/SelfTest.qml:%lld: TypeError: "
                     "telemetry self-test QML error",
                     static_cast<long long>(QDateTime::currentSecsSinceEpoch() % 100000));
        }
        // MAKINE_SELFTEST_HANG proves the hang watchdog: the UI thread blocks
        // past the threshold inside a running event loop.
        if (qEnvironmentVariableIsSet("MAKINE_SELFTEST_HANG")) {
            makine::CrashReporter::addBreadcrumb("selftest", "hang check: blocking the UI thread");
            makine::CrashReporter::startHangWatchdog();
            QTimer::singleShot(200, &app, [] {
                QThread::msleep(static_cast<unsigned long>(
                    makine::telemetryrules::kHangThresholdMs + 2500));
            });
            QTimer::singleShot(makine::telemetryrules::kHangThresholdMs + 4500, &app,
                               &QCoreApplication::quit);
            app.exec();
        }
        makine::CrashReporter::shutdown();   // flushes queued envelopes
        qInfo("telemetry self-test event dispatched");

        // The counting channel has no quota to hide behind, but it can still
        // be silently dead (it was, until 2026-09-23) — so it is proven the
        // same way: one record through the real transport, the reply checked.
        const int status = makine::TelemetryService::selfTest();
        qInfo("counting channel self-test: HTTP %d", status);
        return (status >= 200 && status < 300) ? 0 : 1;
    }
#endif

    // Scan self-test (--selftest-scan): runs the real library scan, prints a
    // one-line summary, exits.
    //
    // Detection is the one subsystem where "works on my machine" proves nothing:
    // the outcome depends entirely on which stores, folders and games that
    // particular machine has. Without a way to run the scan and read the result,
    // a "my game is not found" report cannot be turned into evidence. This calls
    // the same scanAllLibraries() the UI calls, so there is no second code path
    // that could drift from what users actually run.
    if (scanSelfTest) {
        auto* bridge = makine::CoreBridge::instance();
        QEventLoop loop;
        int detected = -1;
        QObject::connect(bridge, &makine::CoreBridge::scanCompleted, &loop,
                         [&](int n) { detected = n; loop.quit(); });
        QTimer::singleShot(180000, &loop, &QEventLoop::quit);  // never hang
        bridge->scanAllLibraries();
        loop.exec();

        // Results cross thread boundaries via a queued call; let it settle.
        QCoreApplication::processEvents();

        if (detected < 0) {
            fprintf(stderr, "scan self-test: TIMEOUT - scan did not complete\n");
            return 1;
        }

        auto* pkgMgr = bridge->packageManager();
        const QVariantList catalog = bridge->allSupportedGames();
        QStringList lines;
        for (const QVariant& v : catalog) {
            const QVariantMap e = v.toMap();
            if (!e.value(QStringLiteral("isInstalled")).toBool())
                continue;
            const QString appId = e.value(QStringLiteral("steamAppId")).toString();

            // An installed patch whose files are gone is the failure users
            // describe as "the patch stopped working" — the launcher still says
            // installed, the game runs untranslated. Surface it here, since this
            // is the command support will ask them to run.
            QString patch;
            if (pkgMgr && pkgMgr->isInstalled(appId)) {
                const int gone = pkgMgr->missingInstalledFiles(appId).size();
                patch = gone > 0 ? QStringLiteral("  YAMA BOZUK (%1 dosya eksik)").arg(gone)
                                 : QStringLiteral("  yama tam");
            }
            lines << QStringLiteral("  %1 [%2] %3%4")
                         .arg(e.value(QStringLiteral("gameName")).toString(),
                              e.value(QStringLiteral("source")).toString(),
                              e.value(QStringLiteral("installPath")).toString(),
                              patch);
        }
        printf("scan self-test: detected=%d catalog=%d matched=%d\n",
               detected, static_cast<int>(catalog.size()),
               static_cast<int>(lines.size()));
        // The path matters as much as the count: a match pointing at the wrong
        // folder installs the patch next to the game instead of into it.
        for (const QString& l : lines)
            printf("%s\n", l.toUtf8().constData());
        fflush(stdout);
        return 0;
    }

    // Anti-RE: run all checks before anything else (no-op in debug builds)
    makine::protection::initialize();

    // The app's fonts first: the splash sets its words in them
    configureApplication(app);

#ifdef Q_OS_WIN
    SplashWindow splash;
    splash.show();
#endif

    QQmlApplicationEngine engine;
    configureEngine(engine);

    QElapsedTimer startupTimer;
    startupTimer.start();

    // Phases 1-7: create all backend services
    GameService* gameService = nullptr;
    ManifestSyncService* manifestSync = nullptr;
    ImageCacheManager* imageCache = nullptr;
    SystemTrayManager* trayManager = nullptr;
    OperationJournal* journal = nullptr;
    ProcessScanner* processScanner = nullptr;

    createServices(app, engine, isPostUpdate, startupTimer,
#ifdef Q_OS_WIN
        splash,
#endif
        gameService, manifestSync, imageCache, trayManager, journal, processScanner);

    // Wire inter-service signal/slot connections
    wireSignals(app, engine, gameService, manifestSync, trayManager, processScanner);

    // Attach QML warning handlers and log environment info
    logStartupDiagnostics(app, engine);

    // ===== Phase 7.5: Sync catalog index =====
    // Signals wired above — safe to sync now so catalogReady is never missed.
    manifestSync->syncCatalog();
    logToFile(QString("ManifestSync::syncCatalog() completed at %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 8: QML engine loading (heaviest single operation) =====
#ifdef Q_OS_WIN
    splash.setStatus(L"Aray\u00FCz derleniyor...");
#endif
    logToFile(QString("Phase 8 (QML load start) at %1 ms").arg(startupTimer.elapsed()));

    // Use QQmlComponent for incremental loading — keeps splash alive
    QQmlComponent mainComponent(&engine);
    {
        MAKINE_ZONE_NAMED("QML::loadFromModule");
        mainComponent.loadFromModule("MakineLauncher", "Main");

        // Pump events while QML compiles (first launch is slow)
#ifdef Q_OS_WIN
        while (mainComponent.isLoading()) {
            splash.pumpMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
        }
#endif
    }

    // A launcher whose QML does not load closes before its first frame, and
    // until now said so only in the local log: no crash, no event, nothing
    // in either channel. Reported once per day per install; shutdown() flushes.
    const auto reportQmlLoadFailure = [&mainComponent](const QString& what) {
        QStringList errors;
        for (const auto& error : mainComponent.errors())
            errors << error.toString();
        makine::CrashReporter::reportFailure("startup", QStringLiteral("qml"),
            QStringLiteral("QML root failed to load (%1): %2")
                .arg(what, errors.join(QStringLiteral(" | ")).left(1500)));
    };

    if (mainComponent.isError()) {
        for (const auto& error : mainComponent.errors())
            logToFile(QString("QML Error: %1").arg(error.toString()));
        reportQmlLoadFailure(QStringLiteral("compile"));
#ifdef Q_OS_WIN
        splash.close();
#endif
        makine::CrashReporter::shutdown();
        return -1;
    }

    logToFile(QString("Phase 8 (QML compiled) at %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 9: Create root window =====
    // Note: QQmlIncubator::Asynchronous does NOT work for root objects
    // (Qt forces synchronous creation without an incubation controller).
    // The 4+ seconds here is Qt framework overhead (module loading, type registration,
    // shader compilation). Optimize by reducing QML module surface, not creation strategy.
    QObject* rootObject = nullptr;
    {
        MAKINE_ZONE_NAMED("QML::createRootObject");
#ifdef Q_OS_WIN
        splash.setStatus(L"Pencere olu\u015Fturuluyor...");
        splash.pumpMessages();
#endif
        rootObject = mainComponent.create();
    }
    if (!rootObject) {
        logToFile("ERROR: Failed to create root object!");
        reportQmlLoadFailure(QStringLiteral("create"));
#ifdef Q_OS_WIN
        splash.close();
#endif
        makine::CrashReporter::shutdown();
        return -1;
    }
    // The main window lives as long as the app. JavaScript ownership left it
    // to the garbage collector: with nothing in JS holding it, a collection
    // destroyed the window seconds after it opened (2026-10-26, seen once the
    // UI grew enough to collect at startup) and the process ran on with no
    // window. It is deleted below, after the event loop, before the engine.
    engine.setObjectOwnership(rootObject, QQmlEngine::CppOwnership);

    logToFile(QString("QML loaded + created in %1 ms").arg(startupTimer.elapsed()));

    // ===== Phase 10: Pre-render + finalize =====
    makine::CrashReporter::addBreadcrumb("startup", "Phase 10: Pre-render + finalize");
#ifdef Q_OS_WIN
    splash.setStatus(L"Son haz\u0131rl\u0131klar yap\u0131l\u0131yor...");
    splash.pumpMessages();
#endif

    setupRootWindow(rootObject, app, engine, gameService, imageCache, startupTimer
#ifdef Q_OS_WIN
        , splash
#endif
    );

    // Hide window if launched with --minimized (Windows autostart → system tray)
    if (startMinimized) {
        if (auto* window = qobject_cast<QQuickWindow*>(rootObject))
            window->setVisible(false);
        // A hidden window renders no frame, so the first-frame callback that
        // closes the splash and loads the library never ran: after sign-in the
        // splash stayed on screen and the library stayed unloaded until the
        // user opened the window from the tray.
#ifdef Q_OS_WIN
        splash.close();
#endif
        gameService->initialize();
        logToFile(QStringLiteral("Started minimized: splash closed, library loading"));
    }

    scheduleMemoryTrim();

    MAKINE_THREAD_NAME("Main/UI");

    setupPerfReporting(app, argc, argv);

    logToFile(QString("Total startup: %1 ms").arg(startupTimer.elapsed()));
    logToFile("Entering event loop...");

    // Anti-RE: periodic re-checks once event loop is running
    makine::protection::schedulePeriodicChecks();

    makine::CrashReporter::startHangWatchdog();

    int exitCode = app.exec();
    delete rootObject;
    makine::CrashReporter::shutdown();
    return exitCode;
}
