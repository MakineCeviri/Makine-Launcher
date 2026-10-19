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
#include <QTemporaryFile>

#include <vector>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#  include <objbase.h>
#  include <sddl.h>
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

#ifdef Q_OS_WIN
// The account this process runs as ("S-1-5-21-…"). GRANT names it explicitly
// because the elevated helper may run as a different, administrator account
// than the user it is opening the folder for.
QString currentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    QString out;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buf(size);
    if (size > 0 && GetTokenInformation(token, TokenUser, buf.data(), size, &size)) {
        LPWSTR text = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &text)) {
            out = QString::fromWCharArray(text);
            LocalFree(text);
        }
    }
    CloseHandle(token);
    return out;
}
#endif

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
            job += QStringLiteral("COPY ") + QDir::toNativeSeparators(realPath(op.src))
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
        case Kind::Grant:
            job += QStringLiteral("GRANT ") + op.src + QLatin1Char('\n');
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

    // The helper reads the job, and writes the result next to it, outside our
    // MSIX container, where only the real path of our temp folder resolves.
    const QString exe = QDir::toNativeSeparators(helperPath());
    const QString arg = QStringLiteral("\"") + QDir::toNativeSeparators(realPath(jobPath))
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
        if (parts.size() >= 3)
            qCWarning(lcElevated) << "elevated op" << parts.at(1) << "failed, win32" << parts.at(2);
    }

    qCInfo(lcElevated) << "elevated job:" << ops.size() << "ops,"
                       << failures << "failed, exit" << exitCode;
    if (failures > 0 && error)
        *error = QStringLiteral("%1/%2 elevated operations failed")
                     .arg(failures).arg(ops.size());
    return failures == 0;
#endif
}

bool ElevatedOps::canWrite(const QString& dir)
{
    if (dir.isEmpty() || !QDir(dir).exists())
        return false;
    QTemporaryFile probe(QDir(dir).filePath(QStringLiteral("makine-write-probe-XXXXXX.tmp")));
    if (!probe.open())
        return false;
    const bool wrote = probe.write("makine", 6) == 6;
    probe.close();
    return wrote;
}

bool ElevatedOps::ensureWritable(const QString& dir, QString* error)
{
    g_lastDeclined = false;
    if (canWrite(dir))
        return true;
#ifdef Q_OS_WIN
    const QString sid = currentUserSid();
    if (sid.isEmpty()) {
        if (error) *error = QStringLiteral("cannot read the current account SID");
        return false;
    }
    qCInfo(lcElevated) << "asking for write access to" << dir;
    if (!run(dir, {{Kind::Grant, sid, {}}}, error))
        return false;
    if (canWrite(dir))
        return true;
    if (error) *error = QStringLiteral("write access granted, folder still not writable");
    qCWarning(lcElevated) << "write access granted, folder still not writable:" << dir;
    return false;
#else
    if (error) *error = QStringLiteral("folder is not writable");
    return false;
#endif
}

QString ElevatedOps::realPath(const QString& path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty())
        return path;

    // Only a packaged process has a family name. Resolved at runtime so the
    // unpackaged build neither links against nor depends on the app-model API.
    using GetFamilyNameFn = LONG(WINAPI*)(UINT32*, PWSTR);
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32)
        return path;
    const auto getFamilyName = reinterpret_cast<GetFamilyNameFn>(
        reinterpret_cast<void*>(
            GetProcAddress(kernel32, "GetCurrentPackageFamilyName")));
    if (!getFamilyName)
        return path;

    constexpr UINT32 kFamilyNameCapacity = 128;  // PACKAGE_FAMILY_NAME_MAX_LENGTH + 1
    wchar_t familyName[kFamilyNameCapacity];
    UINT32 nameLength = kFamilyNameCapacity;
    if (getFamilyName(&nameLength, familyName) != ERROR_SUCCESS)
        return path;  // APPMODEL_ERROR_NO_PACKAGE — we run unpackaged
    if (nameLength == 0)
        return path;

    // qEnvironmentVariable, not qgetenv: the latter round-trips through the
    // ANSI code page, so a user folder with characters the code page cannot
    // represent comes back mangled, the prefix test below misses, and the
    // rewrite silently never happens for exactly the users who need it.
    const QString localAppData =
        QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA"));
    if (localAppData.isEmpty())
        return path;

    const QString normalized = QDir::fromNativeSeparators(path);
    if (!normalized.startsWith(localAppData, Qt::CaseInsensitive))
        return path;

    const QString realRoot = localAppData + QStringLiteral("/Packages/")
        + QString::fromWCharArray(familyName, static_cast<int>(nameLength) - 1)
        + QStringLiteral("/LocalCache/Local");
    if (normalized.startsWith(realRoot, Qt::CaseInsensitive))
        return path;  // already a real path

    const QString rewritten = realRoot + normalized.mid(localAppData.length());

    // Only take the rewrite when it actually resolves. Paths the container
    // does not redirect must keep working exactly as they did before.
    if (!QFileInfo::exists(rewritten))
        return path;

    qCInfo(lcElevated) << "De-virtualised path for elevation:" << path << "->" << rewritten;
    return rewritten;
#else
    return path;
#endif
}

} // namespace makine
