/**
 * @file credential_store.cpp
 * @brief Windows Credential Manager wrapper implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/credential_store.hpp"
#include "makine/logging.hpp"
#include "makine/audit.hpp"

#include <optional>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#endif

namespace makine {

std::string CredentialStore::makeTarget(const std::string& key) {
    return std::string(PREFIX) + key;
}

VoidResult CredentialStore::save(const std::string& key, const std::string& value) {
#ifdef _WIN32
    std::string target = makeTarget(key);

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;

    // M-4: Proper UTF-8 to wide string conversion (handles non-ASCII chars)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, nullptr, 0);
    std::wstring wTarget(wLen > 0 ? wLen - 1 : 0, L'\0');
    if (wLen > 0)
        MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, wTarget.data(), wLen);
    cred.TargetName = const_cast<LPWSTR>(wTarget.c_str());

    cred.CredentialBlobSize = static_cast<DWORD>(value.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.data()));

    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    std::wstring wComment = L"Makine credential";
    cred.Comment = const_cast<LPWSTR>(wComment.c_str());

    if (!CredWriteW(&cred, 0)) {
        DWORD error = GetLastError();
        MAKINE_LOG_ERROR(log::SECURITY,
            "Failed to save credential '{}': Windows error {}",
            key, error);
        return std::unexpected(Error(ErrorCode::IOError,
            fmt::format("Failed to save credential: Windows error {}", error)));
    }

    MAKINE_LOG_INFO(log::SECURITY, "Credential saved: {}", key);
    AuditLogger::logSystemEvent("credential_saved",
        fmt::format("Key: {}", key), AuditSeverity::Info);
    return {};
#else
    return std::unexpected(Error(ErrorCode::Unknown,
        "Credential store only available on Windows"));
#endif
}

std::optional<std::string> CredentialStore::load(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    // M-4: Proper UTF-8 to wide string conversion (handles non-ASCII chars)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, nullptr, 0);
    std::wstring wTarget(wLen > 0 ? wLen - 1 : 0, L'\0');
    if (wLen > 0)
        MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, wTarget.data(), wLen);

    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(wTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        MAKINE_LOG_DEBUG(log::SECURITY, "Credential not found: {}", key);
        return std::nullopt;
    }

    std::string value(
        reinterpret_cast<const char*>(pCred->CredentialBlob),
        pCred->CredentialBlobSize
    );

    CredFree(pCred);

    MAKINE_LOG_DEBUG(log::SECURITY, "Credential loaded: {}", key);
    return value;
#else
    return std::nullopt;
#endif
}

VoidResult CredentialStore::remove(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    // M-4: Proper UTF-8 to wide string conversion (handles non-ASCII chars)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, nullptr, 0);
    std::wstring wTarget(wLen > 0 ? wLen - 1 : 0, L'\0');
    if (wLen > 0)
        MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, wTarget.data(), wLen);

    if (!CredDeleteW(wTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND) {
            return {};  // Already removed
        }
        MAKINE_LOG_ERROR(log::SECURITY,
            "Failed to delete credential '{}': Windows error {}",
            key, error);
        return std::unexpected(Error(ErrorCode::IOError,
            fmt::format("Failed to delete credential: Windows error {}", error)));
    }

    MAKINE_LOG_INFO(log::SECURITY, "Credential deleted: {}", key);
    AuditLogger::logSystemEvent("credential_deleted",
        fmt::format("Key: {}", key), AuditSeverity::Info);
    return {};
#else
    return std::unexpected(Error(ErrorCode::Unknown,
        "Credential store only available on Windows"));
#endif
}

bool CredentialStore::exists(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    // M-4: Proper UTF-8 to wide string conversion (handles non-ASCII chars)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, nullptr, 0);
    std::wstring wTarget(wLen > 0 ? wLen - 1 : 0, L'\0');
    if (wLen > 0)
        MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, wTarget.data(), wLen);

    PCREDENTIALW pCred = nullptr;
    if (CredReadW(wTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        CredFree(pCred);
        return true;
    }
    return false;
#else
    return false;
#endif
}

} // namespace makine
