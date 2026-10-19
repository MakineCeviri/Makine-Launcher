// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

#include <QList>
#include <QString>

namespace makine {

// Runs a batch of file operations through makine-elevate.exe, which Windows
// raises to administrator via UAC.
//
// The launcher deliberately stays asInvoker: elevating the whole application
// changes its antivirus profile, and the Store build cannot be elevated at all
// (MSIX packages have no "Run as administrator"). That left games installed
// under C:\Program Files unpatchable — the install failed with "game directory
// not writable" and offered advice the user could not follow.
//
// Only operations that actually failed on the normal path should be sent here,
// so the user sees at most one UAC prompt per install and only when it is
// genuinely required.
class ElevatedOps
{
public:
    enum class Kind { Copy, Mkdir, Delete, Rename, Grant };

    struct Op {
        Kind kind;
        QString src;      // absolute source (Copy), root-relative (Rename), account SID (Grant)
        QString relDst;   // always relative to root; unused by Grant
    };

    // True when the helper binary is present next to the application.
    static bool available();

    // Executes `ops` against `root`. Returns true when every operation
    // succeeded. `failedIndices` receives the indices that did not, so the
    // caller can report precisely what remains broken. `error` is set for
    // job-level failures (helper missing, user declined UAC, no result file).
    static bool run(const QString& root, const QList<Op>& ops,
                    QString* error = nullptr, QList<int>* failedIndices = nullptr);

    // True when the user dismissed the UAC prompt on the last run() call.
    static bool lastRunDeclined();

    // Whether this process can create files in `dir`. Probes with a real file:
    // on Windows QFileInfo::isWritable reflects the read-only attribute, not
    // the ACL, so a folder under C:\Program Files reads "writable" right up
    // until the write fails.
    static bool canWrite(const QString& dir);

    // Makes `dir` writable for the current user when it is not: one UAC prompt,
    // after which the helper grants this account Modify on the folder and
    // everything below it (what Steam does for its own library). Every later
    // write — copies, in-place patches, restore, uninstall — then takes the
    // normal path. Blocks until the prompt is answered: call it off the GUI
    // thread. lastRunDeclined() tells a refusal from a failure.
    static bool ensureWritable(const QString& dir, QString* error = nullptr);

    // An elevated child runs OUTSIDE our MSIX container, so a virtualised
    // AppData path we hold means nothing to it. Returns the real LocalCache
    // location for such a path, or the path unchanged when it is not
    // redirected (unpackaged build, or a path the container does not touch).
    static QString realPath(const QString& path);
};

} // namespace makine
