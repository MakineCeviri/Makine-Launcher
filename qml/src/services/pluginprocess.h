// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include "pluginwire.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QString>

#include <functional>

class QTimer;

Q_DECLARE_LOGGING_CATEGORY(lcPlugin)

/**
 * One plugin running in its own makine-plugin-host.exe.
 *
 * start() launches the host and sends `initialize`; ready() or startFailed()
 * follows. Requests the plugin makes arrive as callRequested() and must be
 * answered with respond()/respondError(). The process is placed in a job object
 * that dies with the launcher, so a host never outlives it.
 */
class PluginProcess : public QObject {
    Q_OBJECT

public:
    /// `error` is empty on success, else {code, message, data?}: code is a MakineStatus
    /// from the plugin or a negative JSON-RPC code from the host.
    using Reply = std::function<void(const QJsonValue& result, const QJsonObject& error)>;

    PluginProcess(QString hostExe, QString dllPath, QObject* parent = nullptr);
    ~PluginProcess() override;

    void start(const QJsonObject& init, int timeoutMs = 15000);
    void invoke(const QString& method, const QJsonValue& params, Reply done, int timeoutMs = 30000);
    void sendEvent(const QString& event, const QJsonValue& payload);
    void respond(const QJsonValue& id, const QJsonValue& result);
    void respondError(const QJsonValue& id, int code, const QString& message);

    /// Asks the plugin to shut down; kills the host if it has not exited after graceMs.
    void stop(int graceMs = 3000);

    /// Blocks until the host has exited; only for application shutdown.
    bool waitForExit(int timeoutMs) { return m_proc.state() == QProcess::NotRunning || m_proc.waitForFinished(timeoutMs); }

    bool isReady() const { return m_state == State::Ready; }
    qint64 processId() const { return m_proc.processId(); }

signals:
    void ready();
    void startFailed(int code, const QString& message, const QJsonValue& data);
    void callRequested(const QJsonValue& id, const QString& method, const QJsonValue& params);
    void eventEmitted(const QString& event, const QJsonValue& payload);
    void logLine(int level, const QString& message);
    /// The host is gone. `requested` is true after stop(), false for a crash or kill.
    void finished(bool requested, int exitCode);

private:
    enum class State { Idle, Starting, Ready, Stopping, Gone };

    void send(const QJsonObject& message);
    qint64 request(const QString& method, const QJsonObject& params, Reply done, int timeoutMs);
    void onStdout();
    void onStderr();
    void onFinished(int exitCode);
    void dispatch(const QJsonObject& message);
    void failPending(int code, const QString& message);
    void assignToJob();

    QString m_hostExe;
    QString m_dllPath;
    QProcess m_proc;
    makine::pluginwire::Decoder m_decoder;
    QByteArray m_stderrLine;
    State m_state = State::Idle;
    qint64 m_nextId = 1;

    struct Pending {
        Reply done;
        QTimer* timer = nullptr;
    };
    QHash<qint64, Pending> m_pending;
};
