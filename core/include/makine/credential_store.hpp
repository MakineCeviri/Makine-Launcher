/**
 * @file credential_store.hpp
 * @brief Secure credential storage using Windows Credential Manager
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides secure storage for API keys, tokens, and other secrets
 * using the Windows Credential Manager (CredWrite/CredRead).
 * Credentials are protected by the user's Windows login.
 */

#pragma once

#include "error.hpp"

#include <optional>
#include <string>

namespace makine {

/**
 * @brief Secure credential storage
 *
 * Uses Windows Credential Manager for secure storage of sensitive data.
 * Each credential is identified by a target name (key).
 *
 * Usage:
 * @code
 * CredentialStore store;
 * store.save("api_key", "sk-...");
 * auto key = store.load("api_key");
 * @endcode
 */
class CredentialStore {
public:
    static constexpr const char* PREFIX = "MakineCeviri/Makine-Launcher/";

    /**
     * @brief Save a credential securely
     * @param key Credential identifier (e.g., "api_key", "repo_token")
     * @param value Secret value to store
     * @return Success or error
     */
    [[nodiscard]] static VoidResult save(
        const std::string& key,
        const std::string& value
    );

    /**
     * @brief Load a credential
     * @param key Credential identifier
     * @return Secret value or nullopt if not found
     */
    [[nodiscard]] static std::optional<std::string> load(
        const std::string& key
    );

    /**
     * @brief Delete a credential
     * @param key Credential identifier
     * @return Success or error
     */
    [[nodiscard]] static VoidResult remove(const std::string& key);

    /**
     * @brief Check if a credential exists
     * @param key Credential identifier
     */
    [[nodiscard]] static bool exists(const std::string& key);

private:
    static std::string makeTarget(const std::string& key);
};

} // namespace makine
