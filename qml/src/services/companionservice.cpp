// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "companionservice.h"

#include <QQuickItem>
#include <QQuickWindow>

#ifdef MAKINE_HAS_COMPANION
#include "gameservice.h"
#include "settingsmanager.h"
#include "translationdownloader.h"

#include "makine/companion.h"

#include <QDate>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <memory>
#endif

namespace makine {

#ifdef MAKINE_HAS_COMPANION

using companion::Moment;
using companion::Work;

namespace {
bool nationalDay(const QDate& d)
{ // 23 Nisan, 19 Mayıs, 30 Ağustos, 29 Ekim
    return (d.month() == 4 && d.day() == 23) || (d.month() == 5 && d.day() == 19)
        || (d.month() == 8 && d.day() == 30) || (d.month() == 10 && d.day() == 29);
}
} // namespace

CompanionService::CompanionService(SettingsManager* settings, QObject* parent)
    : QObject(parent), m_settings(settings), m_host(std::make_unique<companion::Host>())
{
    m_host->openLauncher = [this] {
        if (!m_window) return;
        if (m_window->visibility() == QWindow::Minimized || !m_window->isVisible()) m_window->showNormal();
        m_window->raise();
        m_window->requestActivate();
    };
    m_host->openSettings = [this] {
        m_host->openLauncher();
        emit settingsRequested();
    };
    m_host->setPlace = [this](companion::Place place) {
        m_settings->setCompanionPlace(place == companion::Place::Clock ? QStringLiteral("clock")
                                      : place == companion::Place::Desktop ? QStringLiteral("desktop") : QStringLiteral("launcher"));
    };
    m_host->turnOff = [this] { m_settings->setCompanionEnabled(false); };

    for (const auto signal : {&SettingsManager::companionEnabledChanged, &SettingsManager::companionPlaceChanged,
                              &SettingsManager::companionLookChanged,
                              &SettingsManager::companionCornerChanged,
                              &SettingsManager::companionSizeChanged, &SettingsManager::companionTalkChanged,
                              &SettingsManager::isDarkModeChanged, &SettingsManager::enableAnimationsChanged})
        connect(m_settings, signal, this, &CompanionService::pushConfig);
    pushConfig();
    m_host->start(QRandomGenerator::global()->generate());
}

CompanionService::~CompanionService() = default;

bool CompanionService::available() const
{
    return true;
}

void CompanionService::pushConfig()
{
    companion::HostConfig c;
    c.enabled = m_settings->companionEnabled();
    const QString place = m_settings->companionPlace();
    c.place = place == QLatin1String("clock") ? companion::Place::Clock
            : place == QLatin1String("desktop") ? companion::Place::Desktop : companion::Place::Launcher;
    c.hands = m_settings->companionLook() != QLatin1String("plain");
    c.corner = m_settings->companionCorner();
    c.sheets = covered(m_dock) ? 1 : 0;
    const QString size = m_settings->companionSize();
    // logical pixels a world unit: his head 104 × 86 units, so 83 × 69 px at the middle size
    c.size = size == QLatin1String("small") ? 0.64 : size == QLatin1String("large") ? 0.96 : 0.8;
    c.talk = m_settings->companionTalk();
    c.reduceMotion = !m_settings->enableAnimations();
    c.lightTheme = !m_settings->isDarkMode();
    m_host->setConfig(c);
}

void CompanionService::watch(GameService* games, TranslationDownloader* downloads)
{
    m_downloads = downloads;
    if (games) {
        connect(games, &GameService::isScanningChanged, this, [this, games] {
            m_scanning = games->isScanning();
            pushWork();
        });
        connect(games, &GameService::translationInstallStarted, this, [this](const QString&) {
            m_installing = true;
            pushWork();
        });
        connect(games, &GameService::translationInstallCompleted, this,
                [this](const QString&, bool success, const QString& message) {
                    m_installing = false;
                    pushWork();
                    if (success) m_host->moment(Moment::Success);
                    else if (message != QLatin1String("Kurulum iptal edildi")) // you cancelled it: nothing went wrong
                        m_host->moment(Moment::Error, QStringLiteral("Kurulum tamamlanamadı."));
                });
    }
    if (downloads) {
        connect(downloads, &TranslationDownloader::activeDownloadsChanged, this, [this] {
            if (!m_downloads->hasActiveDownloads()) m_progress = -1;
            pushWork();
        });
        connect(downloads, &TranslationDownloader::downloadProgress, this, [this](const QString&, qint64 received, qint64 total) {
            m_progress = total > 0 ? double(received) / double(total) : -1;
            m_host->setProgress(m_progress);
        });
        connect(downloads, &TranslationDownloader::downloadError, this, [this](const QString&, const QString&) {
            m_host->moment(Moment::Error, QStringLiteral("İndirme yarıda kaldı."));
        });
    }
}

void CompanionService::pushWork()
{ // what he works along with: an install, else a download, else a scan
    const Work w = m_installing ? Work::Install
                 : m_downloads && m_downloads->hasActiveDownloads() ? Work::Download
                 : m_scanning ? Work::Scan : Work::None;
    m_host->setWork(w, m_progress);
}

void CompanionService::attachWindow(QQuickWindow* window)
{
    m_window = window;
    if (!window) return;
    m_host->setLauncher(quintptr(window->winId()));
    pushDock();
    connect(window, &QWindow::screenChanged, this, &CompanionService::pushDock);
    // You came back: from the tray after a few seconds, or to the launcher after a few minutes elsewhere
    // (his hello as he first shows himself is his window's)
    const auto back = [this](qint64 after) {
        if (m_away.isValid() && m_away.elapsed() > after) m_host->moment(Moment::Welcome);
        m_away.invalidate();
    };
    connect(window, &QWindow::visibilityChanged, this, [this, back](QWindow::Visibility v) {
        if (v == QWindow::Hidden || v == QWindow::Minimized) { m_away.start(); return; }
        back(4000);
    });
    connect(window, &QWindow::activeChanged, this, [this, back] {
        if (!m_window->isActive()) { if (!m_away.isValid()) m_away.start(); return; }
        back(3 * 60 * 1000);
    });
    // On a national day, the flag after his hello
    if (nationalDay(QDate::currentDate()))
        QTimer::singleShot(4300, this, [this] { celebrate(); });
}

void CompanionService::setDock(const QRectF& sceneRect)
{
    m_dock = sceneRect;
    pushDock();
}

void CompanionService::setCorner(const QRectF& sceneRect)
{
    m_corner = sceneRect;
    pushDock();
}

void CompanionService::noteSheet(QObject* sheet, bool open)
{
    m_sheets.removeAll(nullptr);
    m_sheets.removeAll(sheet);
    if (open && sheet) m_sheets.append(sheet);
    pushDock();
    pushConfig();
}

bool CompanionService::covered(const QRectF& sceneRect) const
{ // an open dialog's card over this rect (the scene's logical pixels), a little larger for its entrance
    if (!sceneRect.isValid() || sceneRect.isEmpty()) return false;
    for (const QPointer<QObject>& sheet : m_sheets) {
        auto* card = sheet ? qobject_cast<QQuickItem*>(sheet->property("background").value<QObject*>()) : nullptr;
        if (!card || card->width() <= 0) continue;
        const QRectF r = card->mapRectToScene(card->boundingRect());
        if (r.adjusted(-r.width() * 0.04, -r.height() * 0.04, r.width() * 0.04, r.height() * 0.04).intersects(sceneRect))
            return true;
    }
    return false;
}

void CompanionService::pushDock()
{ // the scene's logical pixels → the window's client area in physical pixels, which his thread maps to the screen
    if (!m_window) return;
    const qreal dpr = m_window->devicePixelRatio();
    const auto physical = [dpr](const QRectF& r) {
        return r.isValid() && !r.isEmpty() ? QRectF(r.topLeft() * dpr, r.size() * dpr).toAlignedRect() : QRect();
    };
    m_host->setDock(physical(m_dock));
    m_host->setCorner(physical(covered(m_corner) ? QRectF() : m_corner));
}

void CompanionService::noteTyping()
{
    m_host->typing();
}

void CompanionService::noteInterest(bool on)
{
    m_host->interest(on);
}

void CompanionService::celebrate()
{
    m_host->moment(Moment::Flag);
}

#else // built without him: QML still reaches "Companion", and nothing answers

CompanionService::CompanionService(SettingsManager* settings, QObject* parent)
    : QObject(parent), m_settings(settings) {}
CompanionService::~CompanionService() = default;
bool CompanionService::available() const { return false; }
void CompanionService::watch(GameService*, TranslationDownloader*) {}
void CompanionService::attachWindow(QQuickWindow* window) { m_window = window; }
void CompanionService::setDock(const QRectF&) {}
void CompanionService::setCorner(const QRectF&) {}
void CompanionService::noteSheet(QObject*, bool) {}
void CompanionService::noteTyping() {}
void CompanionService::noteInterest(bool) {}
void CompanionService::celebrate() {}

#endif

} // namespace makine
