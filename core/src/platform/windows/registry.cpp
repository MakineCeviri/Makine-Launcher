/**
 * @file registry.cpp
 * @brief Windows registry operations
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/types.hpp"
#include "makineai/error.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <string>
#include <vector>
#include <optional>

namespace makineai::platform::windows {

/**
 * @brief Read string value from registry
 */
std::optional<std::string> readRegistryString(
    HKEY hKeyRoot,
    const wchar_t* subKey,
    const wchar_t* valueName
) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    wchar_t buffer[MAX_PATH * 2] = {0};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;

    result = RegQueryValueExW(hKey, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer), &bufferSize);
    RegCloseKey(hKey);

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
    HKEY hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    DWORD type = 0;

    result = RegQueryValueExW(hKey, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &valueSize);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || type != REG_DWORD) {
        return std::nullopt;
    }

    return value;
}

/**
 * @brief Check if registry key exists
 */
bool registryKeyExists(HKEY hKeyRoot, const wchar_t* subKey) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

/**
 * @brief Enumerate subkeys of a registry key
 */
std::vector<std::string> enumerateRegistrySubkeys(
    HKEY hKeyRoot,
    const wchar_t* subKey
) {
    std::vector<std::string> subkeys;

    HKEY hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return subkeys;
    }

    wchar_t keyName[256];
    DWORD keyNameSize;
    DWORD index = 0;

    while (true) {
        keyNameSize = sizeof(keyName) / sizeof(wchar_t);
        result = RegEnumKeyExW(hKey, index, keyName, &keyNameSize,
            nullptr, nullptr, nullptr, nullptr);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }

        if (result == ERROR_SUCCESS) {
            // Convert to UTF-8
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

    RegCloseKey(hKey);
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
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        // Try WOW6432Node
        regPath = L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + wideCode;
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);
    }

    if (result != ERROR_SUCCESS) {
        // Try HKEY_CURRENT_USER
        regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + wideCode;
        result = RegOpenKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey);
    }

    if (result != ERROR_SUCCESS) {
        return std::nullopt;
    }

    auto readValue = [&hKey](const wchar_t* name) -> std::string {
        wchar_t buffer[MAX_PATH * 2] = {0};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;

        if (RegQueryValueExW(hKey, name, nullptr, &type,
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

    RegCloseKey(hKey);
    return info;
}

} // namespace makineai::platform::windows

#endif // _WIN32
