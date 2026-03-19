/**
 * @file registry.cpp
 * @brief Windows registry operations
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <string>
#include <vector>
#include <optional>

namespace makine::platform::windows {

// RAII wrapper for Windows registry key handles
class RegKeyGuard {
public:
    RegKeyGuard() = default;
    ~RegKeyGuard() { close(); }
    RegKeyGuard(const RegKeyGuard&) = delete;
    RegKeyGuard& operator=(const RegKeyGuard&) = delete;

    HKEY* ptr() { return &key_; }
    HKEY get() const { return key_; }
    explicit operator bool() const { return key_ != nullptr; }

    void close() {
        if (key_) { RegCloseKey(key_); key_ = nullptr; }
    }

private:
    HKEY key_ = nullptr;
};

/**
 * @brief Read string value from registry
 */
std::optional<std::string> readRegistryString(
    HKEY hKeyRoot,
    const wchar_t* subKey,
    const wchar_t* valueName
) {
    RegKeyGuard hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.ptr());
    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    wchar_t buffer[MAX_PATH * 2] = {0};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;

    result = RegQueryValueExW(hKey.get(), valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer), &bufferSize);

    if (result != ERROR_SUCCESS || type != REG_SZ) {
        return std::nullopt;
    }

    // Convert wide string to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
        nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) {
        return std::nullopt;
    }

    std::string utf8Str(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
        utf8Str.data(), utf8Len, nullptr, nullptr);

    return utf8Str;
}

/**
 * @brief Read DWORD value from registry
 */
std::optional<uint32_t> readRegistryDword(
    HKEY hKeyRoot,
    const wchar_t* subKey,
    const wchar_t* valueName
) {
    RegKeyGuard hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.ptr());
    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    DWORD type = 0;

    result = RegQueryValueExW(hKey.get(), valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &valueSize);

    if (result != ERROR_SUCCESS || type != REG_DWORD) {
        return std::nullopt;
    }

    return value;
}

/**
 * @brief Check if registry key exists
 */
bool registryKeyExists(HKEY hKeyRoot, const wchar_t* subKey) {
    RegKeyGuard hKey;
    return RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.ptr()) == ERROR_SUCCESS;
}

/**
 * @brief Enumerate subkeys of a registry key
 */
std::vector<std::string> enumerateRegistrySubkeys(
    HKEY hKeyRoot,
    const wchar_t* subKey
) {
    std::vector<std::string> subkeys;

    RegKeyGuard hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.ptr());
    if (result != ERROR_SUCCESS) {
        return subkeys;
    }

    wchar_t keyName[256];
    DWORD keyNameSize;
    DWORD index = 0;

    while (true) {
        keyNameSize = sizeof(keyName) / sizeof(wchar_t);
        result = RegEnumKeyExW(hKey.get(), index, keyName, &keyNameSize,
            nullptr, nullptr, nullptr, nullptr);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }

        if (result == ERROR_SUCCESS) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, keyName, -1,
                nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string utf8Str(utf8Len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, keyName, -1,
                    utf8Str.data(), utf8Len, nullptr, nullptr);
                subkeys.push_back(std::move(utf8Str));
            }
        }

        index++;
    }

    return subkeys;
}

/**
 * @brief Get Steam installation path from registry
 */
std::optional<std::string> getSteamPath() {
    // Try HKEY_CURRENT_USER first
    auto path = readRegistryString(
        HKEY_CURRENT_USER,
        L"Software\\Valve\\Steam",
        L"SteamPath"
    );

    if (path) {
        return path;
    }

    // Try HKEY_LOCAL_MACHINE (32-bit on 64-bit Windows)
    return readRegistryString(
        HKEY_LOCAL_MACHINE,
        L"Software\\WOW6432Node\\Valve\\Steam",
        L"InstallPath"
    );
}

/**
 * @brief Get uninstall information for a game
 */
struct UninstallInfo {
    std::string displayName;
    std::string installLocation;
    std::string publisher;
    std::string displayVersion;
};

std::optional<UninstallInfo> getUninstallInfo(const std::string& productCode) {
    // Convert product code to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, productCode.c_str(), -1,
        nullptr, 0);
    if (wideLen <= 0) return std::nullopt;

    std::wstring wideCode(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, productCode.c_str(), -1,
        wideCode.data(), wideLen);

    // Build registry path
    std::wstring regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + wideCode;

    // Try HKEY_LOCAL_MACHINE first
    RegKeyGuard hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, hKey.ptr());

    if (result != ERROR_SUCCESS) {
        // Try WOW6432Node
        regPath = L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + wideCode;
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, hKey.ptr());
    }

    if (result != ERROR_SUCCESS) {
        // Try HKEY_CURRENT_USER
        regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + wideCode;
        result = RegOpenKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, hKey.ptr());
    }

    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    auto readValue = [&hKey](const wchar_t* name) -> std::string {
        wchar_t buffer[MAX_PATH * 2] = {0};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;

        if (RegQueryValueExW(hKey.get(), name, nullptr, &type,
            reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS &&
            type == REG_SZ) {

            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
                nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string utf8Str(utf8Len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
                    utf8Str.data(), utf8Len, nullptr, nullptr);
                return utf8Str;
            }
        }
        return "";
    };

    UninstallInfo info;
    info.displayName = readValue(L"DisplayName");
    info.installLocation = readValue(L"InstallLocation");
    info.publisher = readValue(L"Publisher");
    info.displayVersion = readValue(L"DisplayVersion");

    return info;
}

} // namespace makine::platform::windows

#endif // _WIN32
