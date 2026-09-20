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
        {QStringLiteral("yönetici izni verilmedi"),              QStringLiteral("elevation_declined")},
        {QStringLiteral("araç başlatılamadı"),                   QStringLiteral("tool_start_failed")},
        {QStringLiteral("araç 30 dakikada bitmedi"),             QStringLiteral("tool_timeout")},
        {QStringLiteral("araç hata kodu"),                       QStringLiteral("tool_exit_code")},
        {QStringLiteral("adımda hata oluştu"),                   QStringLiteral("step_failed")},          // 1124
        {QStringLiteral("dosya kopyalanamadı"),                  QStringLiteral("copy_failed")},          // 6
        {QStringLiteral("yazma izni yok"),                       QStringLiteral("no_permission")},        // 59

        // ---- package / catalogue / startup ----
        {QStringLiteral("installed patch files missing"),        QStringLiteral("integrity_missing")},    // 65
        {QStringLiteral("Çeviri dosyaları bulunamadı"),          QStringLiteral("package_missing")},      // 19
        {QStringLiteral("Katalog indirilemedi"),                 QStringLiteral("catalog_download")},     // 24
        {QStringLiteral("Hiçbir tarayıcı oyun bulamadı"),        QStringLiteral("scan_empty")},           // 75
        {QStringLiteral("Core initialization failed"),           QStringLiteral("core_init")},            // 5
    };
    return kRules;
}

// Never empty. A message nothing matches is "other" — an honest bucket that
// shows up on the dashboard and asks for a rule, rather than a blank tag that
// silently drops the event out of every grouping.
inline QString failureReason(const QString& message)
{
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
        QStringLiteral("setSteamLanguage"),
    };
    return kActions.contains(action) ? action : QString();
}

} // namespace makine::failurereasons
