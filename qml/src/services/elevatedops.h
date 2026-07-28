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
    enum class Kind { Copy, Mkdir, Delete, Rename };

    struct Op {
        Kind kind;
        QString src;      // absolute source (Copy) or root-relative (Rename)
        QString relDst;   // always relative to root
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
};

} // namespace makine
