// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// End-to-end: PluginProcess -> the real makine-plugin-host.exe -> echo plugin DLL.

#include "pluginprocess.h"

#include <makine/plugin/abi.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>

#include <functional>

namespace {

bool waitFor(const std::function<bool()>& condition, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return condition();
}

struct Call {
    bool done = false;
    QJsonValue result;
    QJsonObject error;
};

// A started echo plugin plus everything it said.
struct Echo {
    PluginProcess process{QStringLiteral(MAKINE_PLUGIN_HOST), QStringLiteral(MAKINE_ECHO_PLUGIN)};
    bool ready = false;
    bool finished = false;
    bool requestedExit = false;
    QStringList logs;

    explicit Echo(const QJsonObject& init = {})
    {
        QObject::connect(&process, &PluginProcess::ready, [this] { ready = true; });
        QObject::connect(&process, &PluginProcess::logLine,
                         [this](int, const QString& line) { logs << line; });
        QObject::connect(&process, &PluginProcess::finished, [this](bool requested, int) {
            finished = true;
            requestedExit = requested;
        });
        process.start(init);
    }

    Call invoke(const QString& method, const QJsonValue& params = QJsonObject{}, int timeoutMs = 10000)
    {
        Call call;
        process.invoke(method, params, [&call](const QJsonValue& r, const QJsonObject& e) {
            call.result = r;
            call.error = e;
            call.done = true;
        }, timeoutMs);
        waitFor([&] { return call.done; }, timeoutMs + 5000);
        return call;
    }
};

} // namespace

TEST(PluginProcess, StartsInvokesAndStops)
{
    Echo echo(QJsonObject{{"pluginId", "test.echo"}});
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    const Call call = echo.invoke(QStringLiteral("echo"), QJsonObject{{"text", QStringLiteral("Türkçe ğüşıöç")}});
    ASSERT_TRUE(call.done);
    EXPECT_TRUE(call.error.isEmpty());
    EXPECT_EQ(call.result.toObject().value("text").toString(), QStringLiteral("Türkçe ğüşıöç"));

    // Logs from the ABI and from stderr arrive; stdout never does.
    EXPECT_TRUE(waitFor([&] { return echo.logs.contains("echo ready") && echo.logs.contains("echo: stderr line"); }))
        << echo.logs.join(" | ").toStdString();
    EXPECT_FALSE(echo.logs.join('\n').contains("stdout must not"));

    echo.process.stop();
    ASSERT_TRUE(waitFor([&] { return echo.finished; }));
    EXPECT_TRUE(echo.requestedExit);
}

TEST(PluginProcess, PluginErrorsComeBackAsStatusCodes)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    Call call = echo.invoke(QStringLiteral("fail"));
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_DENIED);
    EXPECT_EQ(call.error.value("message").toString(), QStringLiteral("nope"));

    call = echo.invoke(QStringLiteral("throw"));
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_FAILED);
    EXPECT_EQ(call.error.value("message").toString(), QStringLiteral("boom"));

    call = echo.invoke(QStringLiteral("no.such.method"));
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_NOT_FOUND);

    // The plugin survives all three.
    call = echo.invoke(QStringLiteral("echo"), QJsonArray{1, 2});
    EXPECT_TRUE(call.error.isEmpty());
    EXPECT_EQ(call.result.toArray().size(), 2);
}

TEST(PluginProcess, PluginCallsReachTheLauncherAndBack)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    QString seenMethod;
    QJsonValue seenParams;
    bool answerWithError = false;
    QObject::connect(&echo.process, &PluginProcess::callRequested,
                     [&](const QJsonValue& id, const QString& method, const QJsonValue& params) {
        seenMethod = method;
        seenParams = params;
        if (answerWithError) echo.process.respondError(id, MAKINE_E_BUSY, QStringLiteral("later"));
        else echo.process.respond(id, QJsonObject{{"answer", 42}});
    });

    Call call = echo.invoke(QStringLiteral("callHost"), QJsonObject{{"q", "x"}});
    EXPECT_EQ(seenMethod, QStringLiteral("test.method"));
    EXPECT_EQ(seenParams.toObject().value("q").toString(), QStringLiteral("x"));
    EXPECT_EQ(call.result.toObject().value("answer").toInt(), 42);

    answerWithError = true;
    call = echo.invoke(QStringLiteral("callHost"));
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_BUSY);
    EXPECT_EQ(call.error.value("message").toString(), QStringLiteral("later"));
}

TEST(PluginProcess, EventsFlowBothWays)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    echo.process.sendEvent(QStringLiteral("game.started"), QJsonObject{{"gameId", "123"}});
    const Call call = echo.invoke(QStringLiteral("lastEvent"));
    EXPECT_EQ(call.result.toObject().value("event").toString(), QStringLiteral("game.started"));
    EXPECT_EQ(call.result.toObject().value("payload").toObject().value("gameId").toString(), QStringLiteral("123"));

    QString emitted;
    QObject::connect(&echo.process, &PluginProcess::eventEmitted,
                     [&](const QString& event, const QJsonValue&) { emitted = event; });
    echo.invoke(QStringLiteral("emit"), QJsonObject{{"text", "hi"}});
    EXPECT_TRUE(waitFor([&] { return emitted == QLatin1String("test.event"); }));
}

TEST(PluginProcess, HostManifestGivesUtf8AndPhysicalPixels)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    const QJsonObject env = echo.invoke(QStringLiteral("env")).result.toObject();
    EXPECT_EQ(env.value("acp").toInt(), 65001);        // CP_UTF8: narrow paths take UTF-8
    EXPECT_EQ(env.value("dpiAwareness").toInt(), 2);   // DPI_AWARENESS_PER_MONITOR_AWARE
}

TEST(PluginProcess, CrashIsAnUnrequestedExit)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    const Call call = echo.invoke(QStringLiteral("crash"));
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_FAILED);
    ASSERT_TRUE(waitFor([&] { return echo.finished; }));
    EXPECT_FALSE(echo.requestedExit);
}

TEST(PluginProcess, HungPluginTimesOutAndStopKillsIt)
{
    Echo echo;
    ASSERT_TRUE(waitFor([&] { return echo.ready; }));

    const Call call = echo.invoke(QStringLiteral("hang"), QJsonObject{}, 500);
    EXPECT_EQ(call.error.value("code").toInt(), MAKINE_E_TIMEOUT);

    echo.process.stop(500);
    ASSERT_TRUE(waitFor([&] { return echo.finished; }, 5000));
}

TEST(PluginProcess, MissingDllFailsToStart)
{
    PluginProcess process(QStringLiteral(MAKINE_PLUGIN_HOST), QStringLiteral(MAKINE_ECHO_PLUGIN) + ".missing.dll");
    int code = 0;
    QJsonValue data;
    QObject::connect(&process, &PluginProcess::startFailed,
                     [&](int c, const QString&, const QJsonValue& d) { code = c; data = d; });
    process.start({});
    ASSERT_TRUE(waitFor([&] { return code != 0; }));
    EXPECT_EQ(code, -32000);
    EXPECT_EQ(data.toObject().value("winError").toInt(), 126);   // ERROR_MOD_NOT_FOUND
}

TEST(PluginProcess, InitializeFailureIsReported)
{
    PluginProcess process(QStringLiteral(MAKINE_PLUGIN_HOST), QStringLiteral(MAKINE_ECHO_PLUGIN));
    int code = 0;
    QString message;
    QObject::connect(&process, &PluginProcess::startFailed,
                     [&](int c, const QString& m, const QJsonValue&) { code = c; message = m; });
    process.start(QJsonObject{{"failInit", true}});
    ASSERT_TRUE(waitFor([&] { return code != 0; }));
    EXPECT_EQ(code, MAKINE_E_DENIED);
    EXPECT_EQ(message, QStringLiteral("echo: init refused"));   // the plugin's last error line
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
