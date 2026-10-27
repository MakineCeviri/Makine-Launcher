// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// A stable machine code for why an operation failed.
//
// Until this existed, the only thing distinguishing one field failure from
// another in Sentry was the Turkish sentence shown to the user. Events carried
// five tags — operation, subject, failure.side, game.id, game.name — which
// answer "which game" and "which operation", never "why". reportFailure's own
// comment said the catalogue would supply the missing dimension "when
// triaging", and nobody ever did: triage means reading a list, and a list
// cannot group by a field the events do not carry.
//
// What that cost, measured 2026-09-19: a single defect — the catalogue API
// filling every recipe-less package in with {"type":"script","steps":[]} —
// reached 177 users across 67 games and appeared in Sentry as FIFTY unrelated
// per-game issues. It was found by exporting issue titles and running a regex
// over Turkish prose. With a reason code it would have been one row, sorted to
// the top, since July.
//
// The messages are written by our own code from a fixed set of templates, so
// classifying them is deterministic. Every pattern below was taken from a live
// issue title (Sentry, 30-day window, read 2026-09-20) and the event counts in
// the comments are that window's.
//
// Plain QString only: no Qt regular expressions (and certainly not <regex>,
// which is broken on MinGW 13.1) — these are substring tests on strings we
// authored.

#include <QString>
#include <QStringList>

#include <utility>

namespace makine::failurereasons {

// Ordered most-specific-first. Several real messages quote a second failure
// inside themselves — "Install rollback failed … restore reported errors:
// Yedek tamamen geri yüklenemedi" is a rollback, not a restore — so the first
// match wins and the order below is part of the contract, not cosmetic.
inline const QList<std::pair<QString, QString>>& reasonRules()
{
    static const QList<std::pair<QString, QString>> kRules = {
        // ---- wrappers that quote another failure (must precede it) ----
        {QStringLiteral("Install rollback failed"),              QStringLiteral("rollback_failed")},      // 43
        {QStringLiteral("uninstall aborted to avoid mixed state"), QStringLiteral("uninstall_aborted")},  // 5
        {QStringLiteral("selective backup failed"),              QStringLiteral("backup_failed")},        // 6

        // ---- capability gaps: the launcher refuses on purpose ----
        {QStringLiteral(".forge arşivlerine"),                   QStringLiteral("forge_inject")},         // 1502
        {QStringLiteral("harici bir kurulum aracıyla"),          QStringLiteral("external_tool")},        // 76
        {QStringLiteral("kendi kurulum sihirbazıyla"),           QStringLiteral("installer_wizard")},     // 5
        {QStringLiteral("Steam Workshop"),                       QStringLiteral("workshop")},
        {QStringLiteral("Paradox modudur"),                      QStringLiteral("paradox_mod")},          // 35
        {QStringLiteral("desteklemediği bir kurulum adımı"),     QStringLiteral("unsupported_step")},     // 181
        {QStringLiteral("otomatik kurulamıyor (kurulum yöntemi:"), QStringLiteral("no_handler")},         // 2219

        // ---- backup / restore ----
        //
        // Before the execution rules, not after: a backup message quotes the
        // copy failure that caused it ("Yedek alma yarıda kaldı (3/6 başarılı,
        // 1 dosya kopyalanamadı)"), so a generic copy_failed placed first
        // swallows the whole bucket. The corpus test caught exactly that.
        {QStringLiteral("birden fazla oyun sürümü için ayrı dosyalar"), QStringLiteral("variant_unselected")},
        // A recipe that is only the patch's own program, with nothing named to
        // undo it: a catalogue fix, never a retry (Heavy Rain, 2026-10-20).
        {QStringLiteral("names no uninstall program"),           QStringLiteral("run_no_undo")},
        {QStringLiteral("backup file list empty"),               QStringLiteral("backup_unknowable")},
        {QStringLiteral("backup available"),                     QStringLiteral("no_backup")},            // 376
        {QStringLiteral("Yedek tamamen geri yüklenemedi"),       QStringLiteral("restore_partial")},      // 48
        {QStringLiteral("Yedek alma yarıda kaldı"),              QStringLiteral("backup_partial")},       // 6

        // ---- the recipe ran and something in it broke ----
        //
        // The four run-step causes come first: every one of them is reported
        // inside an "N adımda hata oluştu" message, so a generic step_failed
        // placed above would swallow all of them. They are separated because
        // each needs a different answer — an antivirus exclusion, a UAC prompt
        // the user must accept, a tool that wants a person, a tool that ran and
        // refused — and until now all four were the same 915-event bucket.
        // Antivirus refused or quarantined a package file (Windows 225/226),
        // at extraction or at install: an exclusion fixes it, a retry does not.
        {QStringLiteral("Antivirüs (ör. Windows Defender)"),     QStringLiteral("antivirus_blocked")},
        {QStringLiteral("yönetici izni verilmedi"),              QStringLiteral("elevation_declined")},
        // A patch's own program (docs/patch-programs.md): not in the package,
        // its stage could not be made, it ran and changed nothing in the game.
        {QStringLiteral("program bulunamadı"),                   QStringLiteral("tool_missing")},
        {QStringLiteral("program hazırlanamadı"),                QStringLiteral("tool_stage_failed")},
        {QStringLiteral("hiçbir dosyayı değiştirmedi"),          QStringLiteral("program_no_change")},
        {QStringLiteral("araç başlatılamadı"),                   QStringLiteral("tool_start_failed")},
        {QStringLiteral("araç 2 saatte bitmedi"),                QStringLiteral("tool_timeout")},
        // What the program itself said, when it is one of the causes our tools name
        // (steprules::toolOutputCause): before the bare exit code, which they also carry.
        {QStringLiteral("oyunun bu sürümünü tanımadı"),          QStringLiteral("tool_version_mismatch")},
        {QStringLiteral("uygulama denetimi programı engelledi"), QStringLiteral("tool_blocked_by_policy")},
        {QStringLiteral("araç hata kodu"),                       QStringLiteral("tool_exit_code")},
        // VPatch outcomes. "nomatch" is the one that means the user's game is a
        // version the patch was not built for — a catalogue problem, not a
        // launcher fault — and it must not read as generic step breakage.
        {QStringLiteral("oyun sürümü bu yamayla eşleşmiyor"),    QStringLiteral("vpatch_nomatch")},
        {QStringLiteral("yama dosyası okunamadı"),               QStringLiteral("vpatch_corrupt")},
        {QStringLiteral("yamalanan dosya doğrulanamadı"),        QStringLiteral("vpatch_verify")},
        // The file an in-place step patches is not in the folder: a wrong
        // folder or another edition of the game, not a broken step (AC II,
        // Fahrenheit — 0.1.5, all "step_failed" until now).
        {QStringLiteral("oyun dosyası bulunamadı"),              QStringLiteral("target_missing")},
        // The folder lacks what the patch's "requires" names (0.2.0 preflight):
        // not the game's folder, or, under an installed patch, the game changed.
        // The folder against the OyunBul fingerprint of the copy the patch was
        // made on (gameprint.h): another version, or not the game at all.
        // The install waited for the player; counted, not a fault.
        {QStringLiteral("game fingerprint: version differs"),    QStringLiteral("game_version_differs")},
        {QStringLiteral("game fingerprint: folder does not match"), QStringLiteral("fingerprint_mismatch")},
        {QStringLiteral("installed game lacks required files"),  QStringLiteral("requires_missing_installed")},
        {QStringLiteral("game folder lacks required files"),     QStringLiteral("folder_mismatch")},
        {QStringLiteral("adımda hata oluştu"),                   QStringLiteral("step_failed")},          // 1124
        {QStringLiteral("dosya kopyalanamadı"),                  QStringLiteral("copy_failed")},          // 6
        {QStringLiteral("yazma izni yok"),                       QStringLiteral("no_permission")},        // 59

        // ---- package / catalogue / startup ----
        {QStringLiteral("installed patch files missing"),        QStringLiteral("integrity_missing")},    // 65
        {QStringLiteral("Çeviri dosyaları bulunamadı"),          QStringLiteral("package_missing")},      // 19
        {QStringLiteral("katalogdaki sürümle eşleşmiyor"),       QStringLiteral("checksum_mismatch")},
        {QStringLiteral("Katalog indirilemedi"),                 QStringLiteral("catalog_download")},     // 24
        {QStringLiteral("Hiçbir tarayıcı oyun bulamadı"),        QStringLiteral("scan_empty")},           // 75
        {QStringLiteral("Core initialization failed"),           QStringLiteral("core_init")},            // 5

        // ---- what was still "other" (Sentry, 30 days to 2026-10-22: 38 of
        // 128 issues). Each one is a different job, and "other" hid that ----
        // A package that extracts to nothing (Nioh's .lnk data file): ours to
        // repack, never a retry. Both the event and the user-facing wording.
        {QStringLiteral("zero files after a re-download"),       QStringLiteral("package_empty")},        // 5
        {QStringLiteral("yama paketi boş çıktı"),                QStringLiteral("package_empty")},        // 8
        // A recipe without its target (Firewatch, The Sims 4): a D1 fix.
        {QStringLiteral("recipe carries no target"),             QStringLiteral("recipe_no_target")},     // 5
        {QStringLiteral("kurulum hedefi katalogda eksik"),       QStringLiteral("recipe_no_target")},     // 5
        {QStringLiteral("files left on disk"),                   QStringLiteral("uninstall_files_left")},
        {QStringLiteral("no install record"),                    QStringLiteral("uninstall_no_record")},
        {QStringLiteral("uninstallPackage returned false"),      QStringLiteral("uninstall_failed")},     // 17 (0.1.5)
        // Before uninstall_tool, which their messages also match.
        {QStringLiteral("uninstall program changed nothing"),    QStringLiteral("uninstall_tool_no_change")},
        {QStringLiteral("uninstall program skipped"),            QStringLiteral("uninstall_tool_skipped")},
        {QStringLiteral("uninstall program"),                    QStringLiteral("uninstall_tool")},
        {QStringLiteral("restore could not start"),              QStringLiteral("restore_not_started")},  // 5
        {QStringLiteral("geri yükleme zaten devam ediyor"),      QStringLiteral("restore_busy")},         // 4
        // The game folder a backup was asked for is gone (moved, deleted, an
        // Xbox folder the package cannot see).
        {QStringLiteral("Hedef klasör bulunamadı"),              QStringLiteral("game_dir_missing")},     // 3
        {QStringLiteral("could not be rolled back"),             QStringLiteral("journal_rollback")},     // 2
        {QStringLiteral("QML root failed to load"),              QStringLiteral("qml_load")},
        {QStringLiteral("Yetersiz disk alanı"),                 QStringLiteral("disk_full")},            // 2
        {QStringLiteral("Disk alanı yetersiz"),                  QStringLiteral("disk_full")},
        {QStringLiteral("Dosya kilitli"),                        QStringLiteral("file_locked")},          // 4
        {QStringLiteral("İndirme başarısız oldu"),               QStringLiteral("download_failed")},      // 3
    };
    return kRules;
}

// Never empty. A message nothing matches is "other" — an honest bucket that
// shows up on the dashboard and asks for a rule, rather than a blank tag that
// silently drops the event out of every grouping.
inline QString failureReason(const QString& message)
{
    // The user pressed ✕. Exact match: "Kurulum iptal edildi. <why>" is a
    // backup failure that stopped the install, not a cancel.
    if (message.trimmed() == QStringLiteral("Kurulum iptal edildi"))
        return QStringLiteral("cancelled");
    for (const auto& [pattern, code] : reasonRules()) {
        if (message.contains(pattern, Qt::CaseInsensitive))
            return code;
    }
    return QStringLiteral("other");
}

// Which recipe step type failed, read back out of the step detail the install
// path already builds ("Türkçe Yama — Adım 3: run ERING_TR.exe"). Empty when
// the message is not a step failure.
//
// Worth its own tag because "run" failing is a different class of problem from
// "copyDir" failing — one is a tool the package ships, the other is our own
// file copy — and today both arrive as the same "1 adımda hata oluştu".
inline QString failedStepAction(const QString& message)
{
    const int marker = message.indexOf(QStringLiteral("Adım "));
    if (marker < 0) return {};
    const int colon = message.indexOf(QLatin1Char(':'), marker);
    if (colon < 0) return {};

    // "…Adım 3: run ERING_TR.exe" → the word after the colon is the action.
    const QString tail = message.mid(colon + 1).trimmed();
    const int space = tail.indexOf(QLatin1Char(' '));
    const QString action = (space < 0 ? tail : tail.left(space)).trimmed();

    // Only report actions the executor really dispatches on; anything else is
    // a sentence that happened to follow a colon.
    static const QStringList kActions = {
        QStringLiteral("copy"),          QStringLiteral("copyFile"),
        QStringLiteral("copyDir"),       QStringLiteral("delete"),
        QStringLiteral("installFont"),   QStringLiteral("run"),
        QStringLiteral("copyToDesktop"), QStringLiteral("rename"),
        QStringLiteral("setSteamLanguage"), QStringLiteral("vpatch"),
        QStringLiteral("patchBytes"),    QStringLiteral("forgeReimport"),
        QStringLiteral("xdelta"),
    };
    return kActions.contains(action) ? action : QString();
}

// The message as a Sentry issue is titled by it: its first line. For a recipe that failed,
// that line was "1 adımda hata oluştu" whatever broke — 0.2.3's Elden Ring, AC Shadows and
// AC Odyssey issues all read the same, and a triage list could not tell them apart. The
// first failed step's own line ("Adım 3: patchBytes Game/… — oyun dosyası bulunamadı")
// is folded into it; every other message is returned as it is.
inline QString foldFirstStep(const QString& message)
{
    const int nl = message.indexOf(QLatin1Char('\n'));
    if (nl < 0 || !message.left(nl).contains(QStringLiteral("adımda hata oluştu"))) return message;
    const int end = message.indexOf(QLatin1Char('\n'), nl + 1);
    const QString step = message.mid(nl + 1, end < 0 ? -1 : end - nl - 1).trimmed();
    if (!step.contains(QStringLiteral("Adım "))) return message;
    return message.left(nl) + QStringLiteral(": ") + step + (end < 0 ? QString() : message.mid(end));
}

} // namespace makine::failurereasons
