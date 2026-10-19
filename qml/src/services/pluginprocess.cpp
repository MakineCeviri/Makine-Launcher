// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginprocess.h"

#include <makine/plugin/abi.h>

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Q_LOGGING_CATEGORY(lcPlugin, "makine.plugin")

namespace {

constexpr int kMaxStderrLine = 4096;

QJsonObject errorObject(int code, const QString& message)
{
    return {{"code", code}, {"message", message}};
}

} // namespace

PluginProcess::PluginProcess(QString hostExe, QString dllPath, QObject* parent)
    : QObject(parent)
    , m_hostExe(std::move(hostExe))
    , m_dllPath(std::move(dllPath))
{
    m_proc.setProgram(m_hostExe);
    m_proc.setArguments({QDir::toNativeSeparators(QFileInfo(m_dllPath).absoluteFilePath())});
    m_proc.setWorkingDirectory(QFileInfo(m_dllPath).absolutePath());

    connect(&m_proc, &QProcess::readyReadStandardOutput, this, &PluginProcess::onStdout);
    connect(&m_proc, &QProcess::readyReadStandardError, this, &PluginProcess::onStderr);
    connect(&m_proc, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) { onFinished(exitCode); });
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_state = State::Gone;
        const QString message = QStringLiteral("makine-plugin-host could not start: %1")
                                    .arg(m_proc.errorString());
        failPending(MAKINE_E_FAILED, message);
        emit startFailed(-32000, message, {});
    });
}

PluginProcess::~PluginProcess()
{
    m_proc.disconnect(this);
    if (m_proc.state() != QProcess::NotRunning) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

void PluginProcess::start(const QJsonObject& init, int timeoutMs)
{
    if (m_state != State::Idle) return;
    m_state = State::Starting;
    connect(&m_proc, &QProcess::started, this, [this, init, timeoutMs] {
        // The host loads the DLL only on `initialize`, so joining the job here
        // happens before any plugin code can start a child process.
        assignToJob();
        request(QStringLiteral("initialize"), {{"init", init}},
                [this](const QJsonValue&, const QJsonObject& error) {
            if (m_state == State::Stopping) return;
            if (error.isEmpty() && m_state == State::Starting) {
                m_state = State::Ready;
                emit ready();
                return;
            }
            const int code = error.value(u"code").toInt(MAKINE_E_FAILED);
            emit startFailed(code, error.value(u"message").toString(), error.value(u"data"));
            if (m_proc.state() != QProcess::NotRunning) m_proc.kill();
        }, timeoutMs);
    }, Qt::SingleShotConnection);
    m_proc.start();
}

void PluginProcess::invoke(const QString& method, const QJsonValue& params, Reply done, int timeoutMs)
{
    if (m_state != State::Ready) {
        if (done)
            QTimer::singleShot(0, this, [done] {
                done({}, errorObject(MAKINE_E_FAILED, QStringLiteral("plugin is not running")));
            });
        return;
    }
    request(QStringLiteral("invoke"), {{"method", method}, {"params", params}}, std::move(done), timeoutMs);
}

void PluginProcess::sendEvent(const QString& event, const QJsonValue& payload)
{
    if (m_state != State::Ready) return;
    send({{"jsonrpc", "2.0"}, {"method", "event"},
          {"params", QJsonObject{{"event", event}, {"payload", payload}}}});
}

void PluginProcess::respond(const QJsonValue& id, const QJsonValue& result)
{
    send({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
}

void PluginProcess::respondError(const QJsonValue& id, int code, const QString& message)
{
    send({{"jsonrpc", "2.0"}, {"id", id}, {"error", errorObject(code, message)}});
}

void PluginProcess::stop(int graceMs)
{
    if (m_state == State::Idle) {
        m_state = State::Gone;
        return;
    }
    if (m_state == State::Gone || m_state == State::Stopping) return;
    m_state = State::Stopping;
    failPending(MAKINE_E_FAILED, QStringLiteral("plugin is stopping"));
    // The host answers and terminates itself; the timer covers a hung shutdown().
    request(QStringLiteral("shutdown"), {}, nullptr, graceMs);
    QTimer::singleShot(graceMs, this, [this] {
        if (m_proc.state() != QProcess::NotRunning) m_proc.kill();
    });
}

void PluginProcess::send(const QJsonObject& message)
{
    if (m_proc.state() == QProcess::Running)
        m_proc.write(makine::pluginwire::encode(message));
}

qint64 PluginProcess::request(const QString& method, const QJsonObject& params, Reply done, int timeoutMs)
{
    const qint64 id = m_nextId++;
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, id] {
        const auto it = m_pending.constFind(id);
        if (it == m_pending.cend()) return;
        const Pending pending = it.value();
        m_pending.erase(it);
        pending.timer->deleteLater();
        if (pending.done)
            pending.done({}, errorObject(MAKINE_E_TIMEOUT, QStringLiteral("plugin did not answer in time")));
    });
    timer->start(timeoutMs);
    m_pending.insert(id, {std::move(done), timer});
    send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}});
    return id;
}

void PluginProcess::onStdout()
{
    m_decoder.append(m_proc.readAllStandardOutput());
    while (auto message = m_decoder.next())
        dispatch(*message);
    if (m_decoder.failed() && m_proc.state() != QProcess::NotRunning) {
        emit logLine(MAKINE_LOG_ERROR, QStringLiteral("protocol error from plugin host; stopping it"));
        m_proc.kill();
    }
}

void PluginProcess::onStderr()
{
    m_stderrLine += m_proc.readAllStandardError();
    qsizetype nl = 0;
    while ((nl = m_stderrLine.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stderrLine.left(nl)).trimmed();
        m_stderrLine.remove(0, nl + 1);
        if (!line.isEmpty()) emit logLine(MAKINE_LOG_INFO, line);
    }
    if (m_stderrLine.size() > kMaxStderrLine) {
        emit logLine(MAKINE_LOG_INFO, QString::fromUtf8(m_stderrLine.left(kMaxStderrLine)));
        m_stderrLine.clear();
    }
}

void PluginProcess::onFinished(int exitCode)
{
    const bool requested = m_state == State::Stopping;
    m_state = State::Gone;
    onStderr();
    if (!m_stderrLine.trimmed().isEmpty())
        emit logLine(MAKINE_LOG_INFO, QString::fromUtf8(m_stderrLine).trimmed());
    m_stderrLine.clear();
    failPending(MAKINE_E_FAILED, QStringLiteral("plugin host exited (code 0x%1)")
                                     .arg(static_cast<quint32>(exitCode), 0, 16));
    emit finished(requested, exitCode);
}

void PluginProcess::dispatch(const QJsonObject& message)
{
    const QString method = message.value(u"method").toString();
    const QJsonValue id = message.value(u"id");

    if (method.isEmpty()) {   // a response to one of our requests
        const auto it = m_pending.constFind(id.toInteger(-1));
        if (it == m_pending.cend()) return;
        const Pending pending = it.value();
        m_pending.erase(it);
        pending.timer->stop();
        pending.timer->deleteLater();
        if (pending.done) pending.done(message.value(u"result"), message.value(u"error").toObject());
        return;
    }

    const QJsonObject params = message.value(u"params").toObject();
    if (method == QLatin1String("log")) {
        emit logLine(params.value(u"level").toInt(MAKINE_LOG_INFO), params.value(u"message").toString());
    } else if (method == QLatin1String("emit")) {
        emit eventEmitted(params.value(u"event").toString(), params.value(u"payload"));
    } else if (method == QLatin1String("call") && !id.isUndefined()) {
        emit callRequested(id, params.value(u"method").toString(), params.value(u"params"));
    } else if (!id.isUndefined()) {
        respondError(id, -32601, QStringLiteral("unknown method: %1").arg(method));
    }
}

void PluginProcess::failPending(int code, const QString& message)
{
    const auto pending = std::exchange(m_pending, {});
    for (const auto& p : pending) {
        p.timer->stop();
        p.timer->deleteLater();
        if (p.done) p.done({}, errorObject(code, message));
    }
}

void PluginProcess::assignToJob()
{
#ifdef Q_OS_WIN
    // One job for every host; KILL_ON_JOB_CLOSE ends them when the launcher's
    // handle goes away, crash included.
    static const HANDLE job = [] {
        HANDLE h = CreateJobObjectW(nullptr, nullptr);
        if (h) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(h, JobObjectExtendedLimitInformation, &info, sizeof info);
        }
        return h;
    }();
    if (!job) return;
    const HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                       static_cast<DWORD>(m_proc.processId()));
    if (!process) return;
    if (!AssignProcessToJobObject(job, process))
        qCWarning(lcPlugin) << "Could not put plugin host in the job object:" << GetLastError();
    CloseHandle(process);
#endif
}
