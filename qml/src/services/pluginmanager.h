// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include "pluginmanifest.h"
#include "plugininstaller.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <map>
#include <optional>

namespace makine {
class GameService;
class ProcessScanner;
} // namespace makine
class PluginProcess;
class PluginRegistry;
class QNetworkAccessManager;

/**
 * Plugin system v2 (docs/adr/0009-plugin-system-v2.md), as QML sees it.
 *
 * Installed plugins come from AppPaths::pluginsDir(); the official catalogue from
 * the signed CDN registry. Each enabled plugin runs in its own
 * makine-plugin-host.exe (PluginProcess). Plugins never run before the user has
 * granted every capability their manifest declares.
 *
 * `plugins` entries: id, name, version, description, publisher, homepage, license,
 *   verified, enabled, state, statusText, lastError, capabilities [{id,label,reason}],
 *   missingCapabilities [{id,label,reason}], actions [{id,label,description}],
 *   hasSettings, updateAvailable, availableVersion.
 *   state: disabled | starting | running | error | crashed | needs-consent |
 *          incompatible | invalid
 * `catalog` entries: id, name, version, description, publisher, homepage, size,
 *   capabilities [{id,label,reason}], installed, installedVersion, updateAvailable,
 *   compatible, incompatibleReason.
 */
class PluginManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY pluginsChanged)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY catalogChanged)
    Q_PROPERTY(QString catalogError READ catalogError NOTIFY catalogChanged)
    Q_PROPERTY(QString busyPluginId READ busyPluginId NOTIFY busyChanged)
    Q_PROPERTY(double busyProgress READ busyProgress NOTIFY busyChanged)
    Q_PROPERTY(bool developerMode READ developerMode WRITE setDeveloperMode NOTIFY developerModeChanged)

    // The launcher-drawn overlay that plugins with the `overlay` capability drive.
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayChanged)
    Q_PROPERTY(QRect overlayRegion READ overlayRegion NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayText READ overlayText NOTIFY overlayChanged)
    Q_PROPERTY(QString overlaySourceText READ overlaySourceText NOTIFY overlayChanged)

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    void setGameService(makine::GameService* service);
    void setProcessScanner(makine::ProcessScanner* scanner);

    /// Reads installed plugins, starts the enabled ones and fetches the registry.
    void start();
    /// Asks every plugin to shut down and waits briefly. Call on aboutToQuit.
    void shutdownAll();

    QVariantList plugins() const;
    int updateCount() const;
    QVariantList catalog() const;
    bool catalogLoading() const;
    QString catalogError() const;
    QString busyPluginId() const { return m_busyId; }
    double busyProgress() const { return m_busyProgress; }
    bool developerMode() const;
    void setDeveloperMode(bool on);
    bool overlayVisible() const { return m_overlay.visible; }
    QRect overlayRegion() const { return m_overlay.region; }
    QString overlayText() const { return m_overlay.text; }
    QString overlaySourceText() const { return m_overlay.sourceText; }

    Q_INVOKABLE QVariantMap plugin(const QString& id) const;
    Q_INVOKABLE void refreshCatalog();
    /// Installs or updates from the registry. The caller has shown the entry's
    /// capabilities, so they count as granted.
    Q_INVOKABLE void install(const QString& id);
    /// Developer mode only: an unverified .mkplugin chosen by the user.
    Q_INVOKABLE void installFromFile(const QUrl& file);
    Q_INVOKABLE void uninstall(const QString& id, bool removeData);
    Q_INVOKABLE void setEnabled(const QString& id, bool enabled);
    Q_INVOKABLE void restart(const QString& id);
    /// Grants what the manifest asks for and starts the plugin if it is enabled.
    Q_INVOKABLE void grantCapabilities(const QString& id);

    /// Setting definitions with current values. Secrets carry `hasValue`, never the value.
    Q_INVOKABLE QVariantList settings(const QString& id) const;
    Q_INVOKABLE bool setSetting(const QString& id, const QString& key, const QVariant& value);
    Q_INVOKABLE void runAction(const QString& id, const QString& actionId);
    Q_INVOKABLE QStringList logLines(const QString& id) const;
    /// Sidebar entries for enabled plugins that have settings or actions.
    Q_INVOKABLE QVariantList settingsCategories() const;

    Q_INVOKABLE void answerAttach(int requestId, bool allow, bool remember);
    /// Qt logical global coordinates; the plugin receives physical pixels. width <= 0 means the user cancelled.
    Q_INVOKABLE void answerRegion(int requestId, int x, int y, int width, int height);

signals:
    /// The set of plugins or their install-level data changed (install, update,
    /// uninstall, enable, consent, catalog flags). Re-read `plugins`.
    void pluginsChanged();
    /// One plugin's runtime data changed: state, lastError, statusText. Frequent
    /// (ui.setStatus); re-read plugin(id) only, never the whole list.
    void pluginChanged(const QString& id);
    void catalogChanged();
    void busyChanged();
    void developerModeChanged();
    void overlayChanged();
    void logChanged(const QString& id);
    /// A setting value changed, by the user or by the plugin itself (settings.set).
    void settingsChanged(const QString& id);
    /// Install, update and start failures, worded for the user.
    void pluginError(const QString& id, const QString& message);
    void pluginInstalled(const QString& id);
    void actionFinished(const QString& id, const QString& actionId, bool ok, const QString& message);
    /// ui.notify from a plugin. kind: info | success | warning | error
    void notify(const QString& id, const QString& pluginName, const QString& title,
                const QString& message, const QString& kind);
    void attachRequested(int requestId, const QString& id, const QString& pluginName,
                         const QString& gameName);
    void regionRequested(int requestId, const QString& id, const QString& pluginName);
    /// The plugin behind an open attach or region request stopped; close its UI.
    void requestCancelled(int requestId);

private:
    struct Installed {
        QString id;
        QString dir;
        std::optional<PluginManifest> manifest;   // empty: folder with a broken manifest
        bool enabled = false;
        bool verified = false;      // installed from the signed registry
        QStringList granted;        // capabilities the user agreed to
        QString state;
        QString statusText;
        QString lastError;
        PluginProcess* process = nullptr;
        QList<qint64> crashTimes;   // msecs since epoch, last 5 minutes
        bool updatePending = false; // .previous/<id> waits for this version to start
        std::function<void()> afterStop;
        QJsonObject values;         // settings.json "values"
        QJsonObject secrets;        // settings.json "secrets": base64 DPAPI blobs
        QStringList log;
    };

    struct UserRequest {
        QString kind;     // attach | region
        QString pluginId;
        QJsonValue callId;
        QString gameId;   // attach
    };

    Installed* find(const QString& id);
    const Installed* find(const QString& id) const;
    void reloadOne(const QString& id);
    void loadFolder(const plugininstaller::InstalledFolder& folder);
    QVariantMap toVariant(const Installed& p) const;

    void startPlugin(Installed& p);
    void stopThen(Installed& p, std::function<void()> next);
    void onReady(const QString& id, PluginProcess* proc);
    void onStartFailed(const QString& id, PluginProcess* proc, int code, const QString& message,
                       const QJsonValue& data);
    void onFinished(const QString& id, PluginProcess* proc, bool requested);
    void onCall(const QString& id, const QJsonValue& callId, const QString& method, const QJsonValue& params);
    void broadcast(const QString& event, const QJsonObject& payload);

    /// A call (callId set) or a fire-and-forget emit (callId undefined) from a plugin.
    void handleRequest(Installed& p, const QJsonValue& callId, const QString& method,
                       const QJsonObject& params);
    QJsonObject attachResult(const QString& gameId) const;

    void replaceInstalled(const plugininstaller::StagedPlugin& staged, bool verified,
                          const QStringList& grant);
    void setBusy(const QString& id, double progress);
    void appendLog(Installed& p, int level, const QString& line);
    void setState(Installed& p, const QString& state, const QString& error = {});

    QJsonObject settingValues(const Installed& p) const;   // defaults + values + decrypted secrets
    bool storeSetting(Installed& p, const QString& key, const QJsonValue& value);
    void loadSettings(Installed& p);
    void saveSettings(const Installed& p) const;

    void saveEnabled() const;
    QString hostExe() const;
    void hideOverlayOf(const QString& id);

    QNetworkAccessManager* m_net = nullptr;
    PluginRegistry* m_registry = nullptr;
    makine::GameService* m_gameService = nullptr;
    makine::ProcessScanner* m_processScanner = nullptr;
    std::map<QString, Installed> m_plugins;
    QHash<int, UserRequest> m_requests;
    int m_nextRequestId = 1;
    QString m_busyId;
    double m_busyProgress = 0.0;
    QString m_runningGameId;
    bool m_shuttingDown = false;

    struct {
        QString owner;
        bool visible = false;
        QRect region;
        QString text;
        QString sourceText;
    } m_overlay;
};
