// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file companionservice.h
 * @brief Makine, the launcher's companion: binds the launcher to him
 * @copyright (c) 2026 MakineCeviri Team
 *
 * He is closed source: a private library (MakineCeviri/Makine-Companion) behind makine/companion.h,
 * built in when its checkout sits next to this repo (MAKINE_HAS_COMPANION). Without it this object
 * stays, does nothing and says so (available), and the launcher hides his settings. This is the
 * launcher's side. It hands him the settings (shown, out on the desktop, size, speech), the
 * launcher's work (a scan, a download and its share, an install; how it ended) and its moments
 * (coming back to the launcher after a while, a national day). QML reaches him as "Companion":
 * where his room is in the window (the sidebar's foot, his corner on the home page), when a dialog
 * is open, when you type, when the cursor is on a main button.
 */

#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRectF>

#include <memory>

class QQuickWindow;

namespace makine {

class SettingsManager;
class GameService;
class TranslationDownloader;

namespace companion {
class Host;
}

class CompanionService : public QObject {
    Q_OBJECT
    // Built in (his private library was there at build time); false: no Makine in this build
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit CompanionService(SettingsManager* settings, QObject* parent = nullptr);
    ~CompanionService() override;

    bool available() const;

    // The launcher's work he follows
    void watch(GameService* games, TranslationDownloader* downloads);
    // The launcher's window: he lives in it (or comes out of it); greets once it is shown
    void attachWindow(QQuickWindow* window);

    // His home in the window: a rect in the window's scene (logical pixels), e.g. the sidebar's foot;
    // an empty one while there is none (the welcome flow)
    Q_INVOKABLE void setDock(const QRectF& sceneRect);
    // His corner on the home page while it is in view (bigger, he flies there); an empty rect: not now
    Q_INVOKABLE void setCorner(const QRectF& sceneRect);
    // A dialog opened (or closed) over the launcher: he leaves his corner if it covers it, and fades
    // only while one covers his room in the sidebar; otherwise he stays in view (he celebrates the
    // install its notes follow)
    Q_INVOKABLE void noteSheet(QObject* sheet, bool open);
    // You typed (the game search): he reads along
    Q_INVOKABLE void noteTyping();
    // The cursor is on something worth doing (a page's main button): he looks forward to it
    Q_INVOKABLE void noteInterest(bool on);
    // He waves a Turkish flag
    Q_INVOKABLE void celebrate();

signals:
    // "Makine ayarları" in his menu: the launcher shows Ayarlar > Görünüm
    void settingsRequested();

private:
    bool covered(const QRectF& sceneRect) const;
    void pushConfig();
    void pushDock();
    void pushWork();

    SettingsManager* m_settings = nullptr;
    QPointer<QQuickWindow> m_window;
    QPointer<TranslationDownloader> m_downloads;
#ifdef MAKINE_HAS_COMPANION
    std::unique_ptr<companion::Host> m_host;
#endif
    QRectF m_dock, m_corner;
    QList<QPointer<QObject>> m_sheets;
    bool m_scanning = false, m_installing = false;
    double m_progress = -1;
    QElapsedTimer m_away;   // how long the launcher has been hidden, or in the background
};

} // namespace makine
