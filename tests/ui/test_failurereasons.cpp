// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The tag that decides whether a field defect is visible.
//
// Sentry events carried five tags — operation, subject, failure.side, game.id,
// game.name — so the only thing separating one failure from another was the
// Turkish sentence shown to the user. The cost, measured 2026-09-19: one
// defect (the catalogue API filling every recipe-less package in with
// {"type":"script","steps":[]}) reached 177 users across 67 games and appeared
// as FIFTY unrelated per-game issues. Nobody saw it for months.
//
// Every message below is a live issue title pulled from Sentry's 30-day window
// on 2026-09-20, with its event count. They are the corpus this rule exists to
// classify, so they are what it is tested against — a synthetic string would
// agree with any implementation.

#include <gtest/gtest.h>
#include "failurereasons.h"

using namespace makine::failurereasons;

// ===== The bucket that was invisible ========================================

TEST(FailureReasons, TheDefectThatLookedLikeFiftySeparateIssues)
{
    // 2219 events. Fifty issues, one cause, no way to see it.
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yama otomatik kurulamıyor (kurulum yöntemi: script). Yama dosyaları "
        "şu klasöre çıkarıldı:")), QStringLiteral("no_handler"));
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yama otomatik kurulamıyor (kurulum yöntemi: userPath). Yama dosyaları "
        "şu klasöre çıkarıldı:")), QStringLiteral("no_handler"));
}

// ===== Capability gaps: refused on purpose ==================================

TEST(FailureReasons, DeliberateRefusalsEachGetTheirOwnCode)
{
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yama, oyunun .forge arşivlerine enjeksiyon gerektirdiği için otomatik "
        "kurulamıyor.")), QStringLiteral("forge_inject"));                      // 1502
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yamanın çevirisi harici bir kurulum aracıyla uygulanır")),
        QStringLiteral("external_tool"));                                        // 76
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu çeviri bir Paradox modudur, oyun klasörüne kurulmaz.")),
        QStringLiteral("paradox_mod"));                                          // 35
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yama kendi kurulum sihirbazıyla gelir.")),
        QStringLiteral("installer_wizard"));                                     // 5
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu yama, uygulamanın şu an desteklemediği bir kurulum adımı içeriyor (run)")),
        QStringLiteral("unsupported_step"));                                     // 181
}

TEST(FailureReasons, AForgeRefusalIsNotAGenericNoHandler)
{
    // Both sentences contain "otomatik kurulamıyor". Ordering decides, and
    // getting it wrong would fold .forge's 1502 events into the wrong bucket.
    const QString forge = QStringLiteral(
        "Bu yama, oyunun .forge arşivlerine enjeksiyon gerektirdiği için otomatik "
        "kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:");
    EXPECT_EQ(failureReason(forge), QStringLiteral("forge_inject"));
}

// ===== A message that quotes another failure ================================

TEST(FailureReasons, TheOuterFailureWinsOverTheOneItQuotes)
{
    // Real title, 43 events. It contains "Yedek tamamen geri yüklenemedi", but
    // the thing that failed is the rollback.
    EXPECT_EQ(failureReason(QStringLiteral(
        "Install rollback failed for \"812140\" — restore reported errors: "
        "\"Yedek tamamen geri yüklenemedi\"")), QStringLiteral("rollback_failed"));

    // 5 events. Same shape, different wrapper.
    EXPECT_EQ(failureReason(QStringLiteral(
        "restore failed, uninstall aborted to avoid mixed state: Yedek tamamen "
        "geri yüklenemedi")), QStringLiteral("uninstall_aborted"));

    // And the plain one still classifies as itself.
    EXPECT_EQ(failureReason(QStringLiteral(
        "Yedek tamamen geri yüklenemedi (4/20 başarılı, 16 dosya hata verdi).")),
        QStringLiteral("restore_partial"));                                      // 48
}

// ===== Execution, backup, catalogue =========================================

TEST(FailureReasons, StepFailuresRegardlessOfHowManyFailed)
{
    EXPECT_EQ(failureReason(QStringLiteral("1 adımda hata oluştu")),
              QStringLiteral("step_failed"));                                    // 1048
    EXPECT_EQ(failureReason(QStringLiteral("2/2 adımda hata oluştu")),
              QStringLiteral("step_failed"));                                    // 54
    EXPECT_EQ(failureReason(QStringLiteral("1/1 adımda hata oluştu")),
              QStringLiteral("step_failed"));                                    // 22
}

TEST(FailureReasons, BothCasingsOfTheBackupMessageLandTogether)
{
    // The same condition is reported from two code paths with different
    // capitalisation; splitting them would halve a 376-event bucket.
    EXPECT_EQ(failureReason(QStringLiteral(
        "No backup available for \"2050650\" - refusing uninstall")),
        QStringLiteral("no_backup"));
    EXPECT_EQ(failureReason(QStringLiteral(
        "no backup available; originals would stay patched")),
        QStringLiteral("no_backup"));
}

TEST(FailureReasons, TheRemainingLiveBuckets)
{
    EXPECT_EQ(failureReason(QStringLiteral(
        "Hiçbir tarayıcı oyun bulamadı (games=0 matched=0 catalog=239 [])")),
        QStringLiteral("scan_empty"));                                           // 75
    EXPECT_EQ(failureReason(QStringLiteral("installed patch files missing: 1/1")),
              QStringLiteral("integrity_missing"));                              // 65
    EXPECT_EQ(failureReason(QStringLiteral(
        "Bu klasöre yazma izni yok. Uygulamayı yönetici olarak çalıştırmayı deneyin")),
        QStringLiteral("no_permission"));                                        // 59
    EXPECT_EQ(failureReason(QStringLiteral("Katalog indirilemedi: Operation timed out")),
              QStringLiteral("catalog_download"));                               // 24
    EXPECT_EQ(failureReason(QStringLiteral(
        "Çeviri dosyaları bulunamadı: Nioh: Complete Edition. İndirme eksik")),
        QStringLiteral("package_missing"));                                      // 19
    EXPECT_EQ(failureReason(QStringLiteral(
        "1/1 dosya kopyalanamadı. Olası neden: oyun açık, antivirüs engelliyor")),
        QStringLiteral("copy_failed"));                                          // 6
    EXPECT_EQ(failureReason(QStringLiteral(
        "Yedek alma yarıda kaldı (3/6 başarılı, 1 dosya kopyalanamadı).")),
        QStringLiteral("backup_partial"));                                       // 6
    EXPECT_EQ(failureReason(QStringLiteral("selective backup failed; install aborted")),
              QStringLiteral("backup_failed"));                                  // 6
    EXPECT_EQ(failureReason(QStringLiteral("Core initialization failed")),
              QStringLiteral("core_init"));                                      // 5
}

// ===== The fallback =========================================================

TEST(FailureReasons, AnUnknownMessageIsNamedNotBlank)
{
    // A blank tag drops the event out of every grouping silently. "other" shows
    // up on the dashboard and asks for a rule.
    EXPECT_EQ(failureReason(QStringLiteral("something nobody has written a rule for")),
              QStringLiteral("other"));
    EXPECT_EQ(failureReason(QString()), QStringLiteral("other"));
}

// ===== Which step action broke ==============================================

TEST(FailureReasons, TheFailingActionIsReadBackOutOfTheDetail)
{
    // Elden Ring, 160 users: "run" failing means a tool the package ships did
    // not start. That is a different problem from our own file copy failing,
    // and today both arrive as "1 adımda hata oluştu".
    EXPECT_EQ(failedStepAction(QStringLiteral(
        "1 adımda hata oluştu\nTürkçe Yama — Adım 3: run ERING_TR.exe")),
        QStringLiteral("run"));
    EXPECT_EQ(failedStepAction(QStringLiteral("Adım 1: copyDir engus")),
              QStringLiteral("copyDir"));
    EXPECT_EQ(failedStepAction(QStringLiteral("Adım 2: setSteamLanguage polish")),
              QStringLiteral("setSteamLanguage"));
}

TEST(FailureReasons, AColonThatIsNotAStepYieldsNothing)
{
    // Only actions the executor dispatches on count; a sentence that happens to
    // follow a colon must not become a tag value.
    EXPECT_EQ(failedStepAction(QStringLiteral("Katalog indirilemedi: Operation timed out")),
              QString());
    EXPECT_EQ(failedStepAction(QStringLiteral("Adım 4: kurulum aracını çalıştırın")),
              QString());
    EXPECT_EQ(failedStepAction(QStringLiteral("1 adımda hata oluştu")), QString());
    EXPECT_EQ(failedStepAction(QString()), QString());
}

// ===== Why a "run" step failed ==============================================
//
// Four different problems left the same trace: a binary antivirus quarantined,
// a UAC prompt the user dismissed, an interactive tool that never returns, and
// a tool that ran and refused. Each needs a different answer. The messages
// below are what the install path now writes into the step detail.

TEST(FailureReasons, TheFourWaysATooledStepFails)
{
    EXPECT_EQ(failureReason(QStringLiteral(
        "1 adımda hata oluştu\nAdım 3: run ERING_TR.exe — yönetici izni verilmedi")),
        QStringLiteral("elevation_declined"));
    EXPECT_EQ(failureReason(QStringLiteral(
        "1 adımda hata oluştu\nAdım 3: run ERING_TR.exe — araç başlatılamadı")),
        QStringLiteral("tool_start_failed"));
    EXPECT_EQ(failureReason(QStringLiteral(
        "1 adımda hata oluştu\nAdım 3: run ERING_TR.exe — araç 30 dakikada bitmedi")),
        QStringLiteral("tool_timeout"));
    EXPECT_EQ(failureReason(QStringLiteral(
        "1 adımda hata oluştu\nAdım 3: run patch.exe — araç hata kodu 2 verdi")),
        QStringLiteral("tool_exit_code"));
}

TEST(FailureReasons, AStepFailureWithNoNamedCauseIsStillStepFailed)
{
    // The cause is only attached to actions that have more than one way to
    // fail; a copyDir that failed must keep its existing bucket.
    EXPECT_EQ(failureReason(QStringLiteral(
        "1 adımda hata oluştu\nAdım 1: copyDir engus")),
        QStringLiteral("step_failed"));
}
