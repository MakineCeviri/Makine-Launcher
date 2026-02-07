/**
 * @file credential_store.cpp
 * @brief Windows Credential Manager wrapper implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/credential_store.hpp"
#include "makineai/logging.hpp"
#include "makineai/audit.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#endif

namespace makineai {

std::string CredentialStore::makeTarget(const std::string& key) {
    return std::string(PREFIX) + key;
}

VoidResult CredentialStore::save(const std::string& key, const std::string& value) {
#ifdef _WIN32
    std::string target = makeTarget(key);

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;

    std::wstring wTarget(target.begin(), target.end());
    cred.TargetName = const_cast<LPWSTR>(wTarget.c_str());

    cred.CredentialBlobSize = static_cast<DWORD>(value.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.data()));

    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    std::wstring wComment = L"MakineAI credential";
    cred.Comment = const_cast<LPWSTR>(wComment.c_str());

    if (!CredWriteW(&cred, 0)) {
        DWORD error = GetLastError();
        MAKINEAI_LOG_ERROR(log::SECURITY,
            "Failed to save credential '{}': Windows error {}",
            key, error);
        return std::unexpected(Error(ErrorCode::IOError,
            "Failed to save credential: Windows error " + std::to_string(error)));
    }

    MAKINEAI_LOG_INFO(log::SECURITY, "Credential saved: {}", key);
    AuditLogger::logSystemEvent("credential_saved",
        "Key: " + key, AuditSeverity::Info);
    return {};
#else
    return std::unexpected(Error(ErrorCode::Unknown,
        "Credential store only available on Windows"));
#endif
}

std::optional<std::string> CredentialStore::load(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    std::wstring wTarget(target.begin(), target.end());

    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(wTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        MAKINEAI_LOG_DEBUG(log::SECURITY, "Credential not found: {}", key);
        return std::nullopt;
    }

    std::string value(
        reinterpret_cast<const char*>(pCred->CredentialBlob),
        pCred->CredentialBlobSize
    );

    CredFree(pCred);

    MAKINEAI_LOG_DEBUG(log::SECURITY, "Credential loaded: {}", key);
    return value;
#else
    return std::nullopt;
#endif
}

VoidResult CredentialStore::remove(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    std::wstring wTarget(target.begin(), target.end());

    if (!CredDeleteW(wTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND) {
            return {};  // Already removed
        }
        MAKINEAI_LOG_ERROR(log::SECURITY,
            "Failed to delete credential '{}': Windows error {}",
            key, error);
        return std::unexpected(Error(ErrorCode::IOError,
            "Failed to delete credential: Windows error " + std::to_string(error)));
    }

    MAKINEAI_LOG_INFO(log::SECURITY, "Credential deleted: {}", key);
    AuditLogger::logSystemEvent("credential_deleted",
        "Key: " + key, AuditSeverity::Info);
    return {};
#else
    return std::unexpected(Error(ErrorCode::Unknown,
        "Credential store only available on Windows"));
#endif
}

bool CredentialStore::exists(const std::string& key) {
#ifdef _WIN32
    std::string target = makeTarget(key);
    std::wstring wTarget(target.begin(), target.end());

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

} // namespace makineai
