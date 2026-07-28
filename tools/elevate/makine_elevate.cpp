// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// makine-elevate — performs a declared batch of file operations with an
// elevated token, then exits.
//
// Why this exists: the launcher ships as an MSIX package. Store apps have no
// "Run as administrator", and CreateProcess cannot elevate in place, so a game
// installed under C:\Program Files was simply unpatchable — the launcher failed
// with "game directory not writable" and told the user to do something the
// Store build cannot do. Keeping the launcher itself asInvoker preserves its
// antivirus profile; only this small, separately signed binary is elevated, and
// only for the operations named in a job file.
//
// It is deliberately dependency-free (no Qt, no JSON library): a parser bug in
// a program that runs as administrator is a privilege-escalation bug.
//
// Job file (UTF-8, LF or CRLF). Every destination is RELATIVE to ROOT:
//
//     ROOT <absolute path>
//     COPY <absolute source>|<relative destination>
//     MKDIR <relative destination>
//     DEL <relative destination>
//     REN <relative source>|<relative destination>
//
// '|' is a separator because Windows forbids it in file names.
//
// Result file (<job>.result), one line per operation, in order:
//     OK <index>
//     ERR <index> <win32 error>
//
// Exit code: 0 when every operation succeeded, otherwise the failure count
// (capped at 100); 101+ for job-level errors.

#include <windows.h>

#include <string>
#include <vector>

namespace {

constexpr int kExitBadUsage = 101;
constexpr int kExitBadJob = 102;
constexpr int kExitNoRoot = 103;

struct Op {
    enum Kind { Copy, Mkdir, Del, Ren } kind;
    std::wstring a;   // absolute source (Copy) or relative source (Ren)
    std::wstring b;   // relative destination
};

std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n);
    return out;
}

std::string readFileUtf8(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    std::string data;
    char buf[8192];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0)
        data.append(buf, got);
    CloseHandle(h);
    return data;
}

bool writeFileUtf8(const std::wstring& path, const std::string& data)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr);
    CloseHandle(h);
    return ok;
}

void normalizeSeparators(std::wstring& s)
{
    for (wchar_t& c : s)
        if (c == L'/') c = L'\\';
}

// A relative path is only accepted when it stays inside ROOT. Rejecting "..",
// absolute paths, drive letters and UNC prefixes is what keeps an elevated
// process from writing outside the directory the user is patching.
bool safeRelative(const std::wstring& rel)
{
    if (rel.empty()) return false;
    if (rel.find(L':') != std::wstring::npos) return false;          // C:\...
    if (rel.size() >= 2 && rel[0] == L'\\' && rel[1] == L'\\') return false;  // UNC
    if (rel[0] == L'\\' || rel[0] == L'/') return false;             // root-relative
    size_t start = 0;
    while (start <= rel.size()) {
        const size_t end = rel.find(L'\\', start);
        const std::wstring part = rel.substr(
            start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (part == L"..") return false;
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return true;
}

bool ensureParentDirs(const std::wstring& fullPath)
{
    const size_t cut = fullPath.find_last_of(L'\\');
    if (cut == std::wstring::npos) return true;
    const std::wstring dir = fullPath.substr(0, cut);
    if (dir.empty()) return true;
    if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    if (!ensureParentDirs(dir)) return false;
    return CreateDirectoryW(dir.c_str(), nullptr)
        || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string trimCr(const std::string& s)
{
    if (!s.empty() && s.back() == '\r') return s.substr(0, s.size() - 1);
    return s;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) return kExitBadUsage;

    const std::wstring jobPath = argv[1];
    const std::string raw = readFileUtf8(jobPath);
    if (raw.empty()) return kExitBadJob;

    std::wstring root;
    std::vector<Op> ops;

    size_t pos = 0;
    while (pos <= raw.size()) {
        const size_t nl = raw.find('\n', pos);
        const std::string line =
            trimCr(raw.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
        pos = (nl == std::string::npos) ? raw.size() + 1 : nl + 1;
        if (line.empty()) continue;

        const size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        const std::string verb = line.substr(0, sp);
        std::wstring rest = utf8ToWide(line.substr(sp + 1));
        normalizeSeparators(rest);

        auto split = [&](std::wstring& l, std::wstring& r) {
            const size_t bar = rest.find(L'|');
            if (bar == std::wstring::npos) return false;
            l = rest.substr(0, bar);
            r = rest.substr(bar + 1);
            return !l.empty() && !r.empty();
        };

        if (verb == "ROOT") {
            root = rest;
            while (!root.empty() && (root.back() == L'\\')) root.pop_back();
        } else if (verb == "COPY") {
            Op o{Op::Copy, {}, {}};
            if (split(o.a, o.b)) ops.push_back(o);
        } else if (verb == "MKDIR") {
            ops.push_back({Op::Mkdir, {}, rest});
        } else if (verb == "DEL") {
            ops.push_back({Op::Del, {}, rest});
        } else if (verb == "REN") {
            Op o{Op::Ren, {}, {}};
            if (split(o.a, o.b)) ops.push_back(o);
        }
    }

    if (root.empty()) return kExitNoRoot;
    if (GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES) return kExitNoRoot;

    std::string result;
    int failures = 0;

    for (size_t i = 0; i < ops.size(); ++i) {
        const Op& op = ops[i];
        DWORD err = 0;
        bool ok = false;

        const bool destOk = safeRelative(op.b);
        const bool srcOk = (op.kind != Op::Ren) || safeRelative(op.a);
        if (!destOk || !srcOk) {
            err = ERROR_INVALID_NAME;
        } else {
            const std::wstring dst = root + L"\\" + op.b;
            switch (op.kind) {
            case Op::Copy:
                ok = ensureParentDirs(dst)
                     && CopyFileW(op.a.c_str(), dst.c_str(), FALSE);
                break;
            case Op::Mkdir:
                ok = ensureParentDirs(dst + L"\\x")
                     || GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES;
                break;
            case Op::Del:
                ok = DeleteFileW(dst.c_str())
                     || GetLastError() == ERROR_FILE_NOT_FOUND;
                break;
            case Op::Ren: {
                const std::wstring src = root + L"\\" + op.a;
                ok = ensureParentDirs(dst)
                     && MoveFileExW(src.c_str(), dst.c_str(),
                                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
                break;
            }
            }
            if (!ok) err = GetLastError();
        }

        char line[64];
        if (ok) {
            wsprintfA(line, "OK %d\n", (int)i);
        } else {
            ++failures;
            wsprintfA(line, "ERR %d %lu\n", (int)i, err);
        }
        result += line;
    }

    writeFileUtf8(jobPath + L".result", result);
    return failures > 100 ? 100 : failures;
}
