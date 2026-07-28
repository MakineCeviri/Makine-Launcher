// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "elevatedops.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#  include <objbase.h>
#endif

Q_LOGGING_CATEGORY(lcElevated, "makine.elevated")

namespace makine {

namespace {

bool g_lastDeclined = false;

QString helperPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("makine-elevate.exe"));
}

// '|' separates the two path fields and Windows forbids it in file names, so a
// path can never smuggle a field break. Newlines are impossible for the same
// reason. Anything that still contains one is a bug, not user input — drop it
// rather than hand a malformed line to a process running as administrator.
bool fieldSafe(const QString& s)
{
    return !s.contains(QLatin1Char('|'))
        && !s.contains(QLatin1Char('\n'))
        && !s.contains(QLatin1Char('\r'));
}

} // namespace

bool ElevatedOps::available()
{
#ifdef Q_OS_WIN
    return QFileInfo::exists(helperPath());
#else
    return false;
#endif
}

bool ElevatedOps::lastRunDeclined()
{
    return g_lastDeclined;
}

bool ElevatedOps::run(const QString& root, const QList<Op>& ops,
                      QString* error, QList<int>* failedIndices)
{
    g_lastDeclined = false;
    const auto fail = [error](const QString& msg) {
        if (error) *error = msg;
        qCWarning(lcElevated) << msg;
        return false;
    };

    if (ops.isEmpty()) return true;

#ifndef Q_OS_WIN
    Q_UNUSED(root)
    return fail(QStringLiteral("elevated helper is Windows-only"));
#else
    if (!available())
        return fail(QStringLiteral("makine-elevate.exe not found next to the app"));

    // Build the job.
    QString job = QStringLiteral("ROOT ") + QDir::toNativeSeparators(root) + QLatin1Char('\n');
    for (const Op& op : ops) {
        if (!fieldSafe(op.src) || !fieldSafe(op.relDst))
            return fail(QStringLiteral("refusing job: path contains a field separator"));
        switch (op.kind) {
        case Kind::Copy:
            job += QStringLiteral("COPY ") + QDir::toNativeSeparators(op.src)
                 + QLatin1Char('|') + op.relDst + QLatin1Char('\n');
            break;
        case Kind::Mkdir:
            job += QStringLiteral("MKDIR ") + op.relDst + QLatin1Char('\n');
            break;
        case Kind::Delete:
            job += QStringLiteral("DEL ") + op.relDst + QLatin1Char('\n');
            break;
        case Kind::Rename:
            job += QStringLiteral("REN ") + op.src + QLatin1Char('|')
                 + op.relDst + QLatin1Char('\n');
            break;
        }
    }

    // Per-user temp with an unpredictable name: the elevated helper trusts this
    // file, so another user must not be able to place or swap it.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString jobPath = QDir(dir).filePath(
        QStringLiteral("makine-elevate-%1.job")
            .arg(QRandomGenerator::system()->generate64(), 16, 16, QLatin1Char('0')));

    QFile jobFile(jobPath);
    if (!jobFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("cannot write elevated job file"));
    jobFile.write(job.toUtf8());
    jobFile.close();

    const QString resultPath = jobPath + QStringLiteral(".result");
    QFile::remove(resultPath);

    const QString exe = QDir::toNativeSeparators(helperPath());
    const QString arg = QStringLiteral("\"") + QDir::toNativeSeparators(jobPath)
                      + QStringLiteral("\"");

    const HRESULT hr = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool ownsCom = SUCCEEDED(hr);

    SHELLEXECUTEINFOW info{};
    info.cbSize       = sizeof(info);
    info.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb       = L"runas";
    info.lpFile       = reinterpret_cast<LPCWSTR>(exe.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(arg.utf16());
    info.nShow        = SW_HIDE;

    const BOOL started = ShellExecuteExW(&info);
    const DWORD startErr = started ? 0 : GetLastError();
    if (ownsCom) CoUninitialize();

    if (!started) {
        QFile::remove(jobPath);
        if (startErr == ERROR_CANCELLED) {
            g_lastDeclined = true;
            return fail(QStringLiteral("user declined the UAC prompt"));
        }
        return fail(QStringLiteral("elevated helper failed to start (win32 %1)")
                        .arg(startErr));
    }

    WaitForSingleObject(info.hProcess, 10 * 60 * 1000);
    DWORD exitCode = 0;
    GetExitCodeProcess(info.hProcess, &exitCode);
    CloseHandle(info.hProcess);

    QFile resultFile(resultPath);
    if (!resultFile.open(QIODevice::ReadOnly)) {
        QFile::remove(jobPath);
        return fail(QStringLiteral("elevated helper produced no result (exit %1)")
                        .arg(exitCode));
    }
    const QString result = QString::fromUtf8(resultFile.readAll());
    resultFile.close();
    QFile::remove(jobPath);
    QFile::remove(resultPath);

    int failures = 0;
    const auto lines = result.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (!line.startsWith(QLatin1String("ERR "))) continue;
        ++failures;
        const auto parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2 && failedIndices)
            failedIndices->append(parts.at(1).toInt());
    }

    qCInfo(lcElevated) << "elevated job:" << ops.size() << "ops,"
                       << failures << "failed, exit" << exitCode;
    if (failures > 0 && error)
        *error = QStringLiteral("%1/%2 elevated operations failed")
                     .arg(failures).arg(ops.size());
    return failures == 0;
#endif
}

} // namespace makine
