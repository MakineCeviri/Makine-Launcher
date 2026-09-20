// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// The one thing between a user's Windows account name and our Sentry project.
//
// Every failure we report quotes paths: the install path writes "Yama
// dosyaları şu klasöre çıkarıldı: C:\Users\<name>\AppData\Local\…" into the
// message a user sees, and that message is also the Sentry issue title. Stack
// frames carry source and module paths from the build machine and the user's
// disk alike. Without this the dashboard would list real people's names.
//
// It lived as a file-static in crashreporter.cpp, which meant it could not be
// exercised: the only code that could reach it was the code that sends events
// to Sentry, so the check could only ever be verified by leaking. It is a pure
// string transform with several edge cases that each cost a real leak to
// discover — a message quoting two paths, a mixed-separator path, a path that
// ends at the user name — so it belongs here, next to the other rules, with a
// test per case.
//
// std::string rather than QString on purpose: the caller receives const char*
// from the Sentry SDK and passes the result straight back to it.

#include <algorithm>
#include <string>

namespace makine::redaction {

inline constexpr const char* kRedacted = "[redacted]";

// Replace the account name in EVERY "Users\<name>" / "Users/<name>" it finds.
//
// Every occurrence, not just the first: a stack frame value holds one path,
// but a captured install failure can quote several, and a partially sanitized
// message still leaks the name.
inline std::string redactUserPaths(const char* raw)
{
    if (!raw) return {};
    std::string path(raw);
    const std::string redacted = kRedacted;

    for (const auto& sep : {std::string("Users\\"), std::string("Users/")}) {
        std::string::size_type pos = 0;
        while ((pos = path.find(sep, pos)) != std::string::npos) {
            const auto nameStart = pos + sep.size();
            // Look for either separator, not just the one that opened the
            // match: paths reach us mixed ("C:\Users\Ahmet/AppData/…") and
            // searching only for the opening style would find nothing.
            const auto nameEnd = std::min(path.find('\\', nameStart),
                                          path.find('/', nameStart));
            // A message can end at the user name — "klasör: C:\Users\Ahmet"
            // carries no trailing separator, and bailing out here left the
            // name in place. Redact to the end of the string in that case.
            const auto nameLen = (nameEnd == std::string::npos)
                                     ? path.size() - nameStart
                                     : nameEnd - nameStart;
            if (nameLen == 0)
                break;
            if (path.compare(nameStart, nameLen, redacted) != 0)
                path.replace(nameStart, nameLen, redacted);
            pos = nameStart + redacted.size();
        }
    }
    return path;
}

} // namespace makine::redaction
