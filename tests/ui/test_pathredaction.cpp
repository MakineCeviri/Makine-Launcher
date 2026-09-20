// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The check that keeps real people's names off the Sentry dashboard.
//
// Every failure we report quotes paths — the install path puts
// "Yama dosyaları şu klasöre çıkarıldı: C:\Users\<name>\AppData\Local\…" into
// the message the user sees, and that same message is the Sentry issue title.
// The transform lived as a file-static in crashreporter.cpp, reachable only
// from the code that sends events, so it could only be verified by leaking.
// Each case below cost a real leak to discover.

#include <gtest/gtest.h>
#include "pathredaction.h"

using makine::redaction::redactUserPaths;

TEST(PathRedaction, TheOrdinaryInstallPath)
{
    EXPECT_EQ(redactUserPaths(
        "C:\\Users\\Ahmet\\AppData\\Local\\MakineCeviri\\packages\\Elden Ring"),
        "C:\\Users\\[redacted]\\AppData\\Local\\MakineCeviri\\packages\\Elden Ring");
    EXPECT_EQ(redactUserPaths("C:/Users/Ahmet/AppData/Local"),
              "C:/Users/[redacted]/AppData/Local");
}

TEST(PathRedaction, EveryOccurrenceNotJustTheFirst)
{
    // A captured install failure quotes both the source and the destination.
    // Sanitising only the first still puts the name in the issue title.
    EXPECT_EQ(redactUserPaths(
        "kopyalanamadi: C:\\Users\\Mehmet\\paket -> C:\\Users\\Mehmet\\oyun"),
        "kopyalanamadi: C:\\Users\\[redacted]\\paket -> C:\\Users\\[redacted]\\oyun");
}

TEST(PathRedaction, MixedSeparatorsInOnePath)
{
    // Paths reach us mixed because Qt and Win32 disagree; searching only for
    // the separator that opened the match found nothing and left the name.
    EXPECT_EQ(redactUserPaths("C:\\Users\\Ayse/AppData/Local/x"),
              "C:\\Users\\[redacted]/AppData/Local/x");
    EXPECT_EQ(redactUserPaths("C:/Users/Ayse\\AppData\\x"),
              "C:/Users/[redacted]\\AppData\\x");
}

TEST(PathRedaction, AMessageThatEndsAtTheUserName)
{
    // No trailing separator. Bailing out here used to leave the name in place.
    EXPECT_EQ(redactUserPaths("klasör: C:\\Users\\Zeynep"),
              "klasör: C:\\Users\\[redacted]");
    EXPECT_EQ(redactUserPaths("C:/Users/Zeynep"), "C:/Users/[redacted]");
}

TEST(PathRedaction, AlreadyRedactedStaysPutAndDoesNotLoop)
{
    // Re-sanitising happens: a message can pass through more than one layer.
    EXPECT_EQ(redactUserPaths("C:\\Users\\[redacted]\\AppData"),
              "C:\\Users\\[redacted]\\AppData");
}

TEST(PathRedaction, NothingToRedactIsLeftAlone)
{
    EXPECT_EQ(redactUserPaths("D:\\Steam\\steamapps\\common\\Elden Ring"),
              "D:\\Steam\\steamapps\\common\\Elden Ring");
    EXPECT_EQ(redactUserPaths("1 adımda hata oluştu"), "1 adımda hata oluştu");
    EXPECT_EQ(redactUserPaths(""), "");
    EXPECT_EQ(redactUserPaths(nullptr), "");
    // A trailing separator with no name must not spin or corrupt the string.
    EXPECT_EQ(redactUserPaths("C:\\Users\\"), "C:\\Users\\");
}
