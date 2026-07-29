// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Failure classification contract.
//
// reportFailure() derives severity from the message text, so the guidance
// strings in LocalPackageManager are effectively part of the telemetry schema:
// reword one and its events silently change class. Sentry showed what that
// costs — 3440 of 4784 events (72%) were "this patch needs an installer we do
// not have", filed as `error` alongside real defects, which is also the tag the
// "Widespread Failure" alert alerts on.
//
// The strings below are verbatim copies of the ones emitted in
// localpackagemanager.cpp. Same mirroring convention as catalog_validate.py's
// copy of kOverlaySafeTypes.

#include <gtest/gtest.h>
#include <QString>

#include "crashreporter.h"

using makine::CrashReporter;

namespace {

// --- Verbatim guidance strings (localpackagemanager.cpp) ---------------------

// installPackage(): forge_inject branch
const QString kForge = QStringLiteral(
    "Bu yama, oyunun .forge arşivlerine enjeksiyon gerektirdiği için otomatik "
    "kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:\n"
    "C:/Users/[redacted]/AppData/Local/MakineCeviri/packages/AC Odyssey\n"
    "Klasörün içindeki kurulum aracını çalıştırın ve istendiğinde oyun "
    "klasörünüzü gösterin.");

// installPackage(): unityPatch branch
const QString kUnity = QStringLiteral(
    "Bu yama Unity bundle yaması gerektiriyor; Makine Launcher bunu henüz "
    "otomatik kuramıyor. Yama dosyaları şu klasöre çıkarıldı:\n/tmp/x\n"
    "Klasörün içindeki kurulum aracını (UnityEX / UABEA) çalıştırın ve "
    "talimatları izleyin.");

// installPackage(): honesty gate, generic branch (script, d2r_mod, …)
const QString kScript = QStringLiteral(
    "Bu yama otomatik kurulamıyor (kurulum yöntemi: script). Yama dosyaları şu "
    "klasöre çıkarıldı:\n/tmp/x\nKlasördeki kurulum talimatını izleyin.");

// installPackage(): honesty gate, external branch
const QString kExternal = QStringLiteral(
    "Bu yamanın çevirisi harici bir kurulum aracıyla uygulanır, Makine Launcher "
    "tarafından otomatik kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:\n"
    "/tmp/x\nKlasördeki kurulum talimatını izleyin.");

// installPackage(): honesty gate, installer branch
const QString kInstaller = QStringLiteral(
    "Bu yama kendi kurulum sihirbazıyla gelir. Çözüm: yama arşivini açın, "
    "içindeki kurulum (.exe) dosyasını çalıştırın ve istendiğinde oyun "
    "klasörünü seçin.");

// installPackage(): honesty gate, workshop branch
const QString kWorkshop = QStringLiteral(
    "Bu çeviri Steam Workshop üzerinden dağıtılıyor. Çözüm: Steam'de oyunun "
    "Workshop sayfasını açın, Türkçe yama öğesine abone olun; Steam otomatik "
    "indirip etkinleştirir.");

// installPackage(): honesty gate, paradox-mod branch
const QString kParadox = QStringLiteral(
    "Bu çeviri bir Paradox modudur, oyun klasörüne kurulmaz. Yama dosyaları şu "
    "klasöre çıkarıldı:\n/tmp/x\nBu klasörün içindekileri Belgeler/Paradox "
    "Interactive/<oyun>/mod klasörüne kopyalayın, ardından oyun başlatıcısında "
    "mod listesinden etkinleştirin.");

// installWithOptions(): unexecutable-step pre-flight
const QString kUnexecutable = QStringLiteral(
    "Bu yama, uygulamanın şu an desteklemediği bir kurulum adımı içeriyor "
    "(cmd, pattern). Otomatik kurulamıyor.");

// --- Capability gaps are not failures ---------------------------------------

TEST(CrashReporterClassify, EveryNoHandlerGuidanceIsUnsupported)
{
    for (const QString& m : {kForge, kUnity, kScript, kExternal, kInstaller,
                             kWorkshop, kParadox, kUnexecutable}) {
        EXPECT_TRUE(CrashReporter::isUnsupportedCapability(m))
            << "not classified as a capability gap: " << m.left(60).toStdString();
    }
}

// The whole point of the split: these must not also read as user-actionable,
// because reportFailure() checks `unsupported` first and a message landing in
// both classes would be a coin flip if that order were ever changed back.
TEST(CrashReporterClassify, UnsupportedTakesPrecedenceOverUserKeywords)
{
    // kForge says "çalıştırın", kInstaller says "çalıştırın … seçin" — near
    // misses for the user-side "çalışıyor" keyword. kParadox and kWorkshop
    // both carry a "Çözüm:"-style remedy. None may be filed as `user`.
    for (const QString& m : {kForge, kInstaller, kWorkshop, kParadox}) {
        EXPECT_TRUE(CrashReporter::isUnsupportedCapability(m));
    }
}

// --- Genuine failures must keep their existing class ------------------------

TEST(CrashReporterClassify, RealDefectsStaySystemSide)
{
    // Alan Wake 2: stale cached recipe pointed at files absent from the package.
    const QString stepFailure = QStringLiteral(
        "2/2 adımda hata oluştu\nAdım 1: copyFile AW2.exe\n"
        "Adım 2: copyFile MediaDecodersWindowsDesktop.dll");
    EXPECT_FALSE(CrashReporter::isUnsupportedCapability(stepFailure));
    EXPECT_FALSE(CrashReporter::isUserActionable(stepFailure));

    // Empty or corrupt payload — our packaging defect, not a capability gap.
    const QString emptyPayload = QStringLiteral(
        "Kurulacak dosya bulunamadı. Yama paketi boş veya bozuk olabilir.");
    EXPECT_FALSE(CrashReporter::isUnsupportedCapability(emptyPayload));
}

TEST(CrashReporterClassify, EnvironmentProblemsStayUserSide)
{
    const QString diskFull = QStringLiteral(
        "Yetersiz disk alanı. Çözüm: sürücüde yer açın.");
    EXPECT_FALSE(CrashReporter::isUnsupportedCapability(diskFull));
    EXPECT_TRUE(CrashReporter::isUserActionable(diskFull));

    const QString noNetwork = QStringLiteral(
        "İndirme başarısız oldu. İnternet bağlantınızı kontrol edin.");
    EXPECT_FALSE(CrashReporter::isUnsupportedCapability(noNetwork));
    EXPECT_TRUE(CrashReporter::isUserActionable(noNetwork));
}

// A step recipe that merely *ran* something is still a real failure: only the
// launcher's own "I cannot install this kind of patch" wording is a gap.
TEST(CrashReporterClassify, RunStepFailureIsNotACapabilityGap)
{
    const QString eldenRing = QStringLiteral(
        "1 adımda hata oluştu\nTürkçe Yama — Adım 3: run ERING_TR.exe");
    EXPECT_FALSE(CrashReporter::isUnsupportedCapability(eldenRing));
}

} // namespace
