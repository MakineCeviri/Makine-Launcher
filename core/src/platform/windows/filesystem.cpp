/**
 * @file filesystem.cpp
 * @brief Windows filesystem utilities
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <string>
#include <algorithm>

namespace makine::platform::windows {

/**
 * @brief Get known folder path
 */
Result<fs::path> getKnownFolderPath(const KNOWNFOLDERID& folderId) {
    wchar_t* path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(folderId, 0, nullptr, &path);

    if (FAILED(hr)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Failed to get known folder path"));
    }

    // Convert to fs::path (handles wide string)
    fs::path result(path);
    CoTaskMemFree(path);

    return result;
}

/**
 * @brief Get local app data path
 */
Result<fs::path> getLocalAppDataPath() {
    return getKnownFolderPath(FOLDERID_LocalAppData);
}

/**
 * @brief Get roaming app data path
 */
Result<fs::path> getRoamingAppDataPath() {
    return getKnownFolderPath(FOLDERID_RoamingAppData);
}

/**
 * @brief Get program data path
 */
Result<fs::path> getProgramDataPath() {
    return getKnownFolderPath(FOLDERID_ProgramData);
}

/**
 * @brief Get documents path
 */
Result<fs::path> getDocumentsPath() {
    return getKnownFolderPath(FOLDERID_Documents);
}

/**
 * @brief Get program files path (x64)
 */
Result<fs::path> getProgramFilesPath() {
    return getKnownFolderPath(FOLDERID_ProgramFiles);
}

/**
 * @brief Get program files path (x86)
 */
Result<fs::path> getProgramFilesX86Path() {
    return getKnownFolderPath(FOLDERID_ProgramFilesX86);
}

/**
 * @brief Open folder in Windows Explorer
 */
VoidResult openInExplorer(const fs::path& path) {
    // Convert to wide string
    std::wstring widePath = path.wstring();

    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"explore",
        widePath.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );

    if (reinterpret_cast<intptr_t>(result) <= 32) {
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to open Explorer"));
    }

    return {};
}

/**
 * @brief Launch executable
 */
VoidResult launchExecutable(const fs::path& exePath, const std::string& args) {
    // H-5: Validate the executable path before launching
    if (exePath.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Empty executable path"));
    }

    std::error_code ec;
    if (!fs::exists(exePath, ec)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Executable not found: " + exePath.string()));
    }

    // Block launching from system-sensitive directories
    auto canonical = fs::canonical(exePath, ec);
    if (ec) canonical = fs::absolute(exePath);
    auto pathStr = canonical.wstring();
    // Normalize to lowercase for comparison
    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::towlower);
    if (pathStr.find(L"\\windows\\system32") != std::wstring::npos ||
        pathStr.find(L"\\windows\\syswow64") != std::wstring::npos) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Refusing to launch system executable"));
    }

    std::wstring wideExe = exePath.wstring();
    std::wstring wideArgs;

    if (!args.empty()) {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, args.c_str(), -1,
            nullptr, 0);
        if (wideLen > 0) {
            wideArgs.resize(wideLen - 1);
            MultiByteToWideChar(CP_UTF8, 0, args.c_str(), -1,
                wideArgs.data(), wideLen);
        }
    }

    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = wideExe.c_str();
    sei.lpParameters = wideArgs.empty() ? nullptr : wideArgs.c_str();
    auto wideDir = exePath.parent_path().wstring();
    sei.lpDirectory = wideDir.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to launch executable"));
    }

    return {};
}

/**
 * @brief Check if file is locked by another process
 */
bool isFileLocked(const fs::path& filePath) {
    std::wstring widePath = filePath.wstring();

    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,  // No sharing
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return true;  // File is locked or doesn't exist
    }

    CloseHandle(hFile);
    return false;
}

/**
 * @brief Get file version info
 */
struct FileVersionInfo {
    uint16_t major;
    uint16_t minor;
    uint16_t build;
    uint16_t revision;
    std::string productName;
    std::string companyName;
    std::string fileDescription;
};

Result<FileVersionInfo> getFileVersionInfo(const fs::path& filePath) {
    std::wstring widePath = filePath.wstring();

    DWORD dummy;
    DWORD versionInfoSize = GetFileVersionInfoSizeW(widePath.c_str(), &dummy);
    if (versionInfoSize == 0) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "No version info available"));
    }

    std::vector<uint8_t> buffer(versionInfoSize);
    if (!GetFileVersionInfoW(widePath.c_str(), 0, versionInfoSize, buffer.data())) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Failed to get version info"));
    }

    FileVersionInfo info = {0};

    // Get fixed file info
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoLen = 0;
    if (VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoLen)) {
        info.major = HIWORD(fileInfo->dwFileVersionMS);
        info.minor = LOWORD(fileInfo->dwFileVersionMS);
        info.build = HIWORD(fileInfo->dwFileVersionLS);
        info.revision = LOWORD(fileInfo->dwFileVersionLS);
    }

    // Get string info
    struct LANGANDCODEPAGE {
        WORD language;
        WORD codePage;
    } *translate;

    UINT translateLen;
    if (VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
        reinterpret_cast<LPVOID*>(&translate), &translateLen)) {

        auto readString = [&](const wchar_t* name) -> std::string {
            wchar_t subBlock[128];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\%s",
                translate[0].language, translate[0].codePage, name);

            wchar_t* value = nullptr;
            UINT valueLen = 0;
            if (VerQueryValueW(buffer.data(), subBlock,
                reinterpret_cast<LPVOID*>(&value), &valueLen)) {

                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, value, -1,
                    nullptr, 0, nullptr, nullptr);
                if (utf8Len > 0) {
                    std::string utf8Str(utf8Len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, value, -1,
                        utf8Str.data(), utf8Len, nullptr, nullptr);
                    return utf8Str;
                }
            }
            return "";
        };

        info.productName = readString(L"ProductName");
        info.companyName = readString(L"CompanyName");
        info.fileDescription = readString(L"FileDescription");
    }

    return info;
}

/**
 * @brief Create shortcut (.lnk file)
 */
VoidResult createShortcut(
    const fs::path& shortcutPath,
    const fs::path& targetPath,
    const std::string& description,
    const fs::path& iconPath,
    int iconIndex
) {
    CoInitialize(nullptr);

    IShellLinkW* shellLink = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_ShellLink,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IShellLinkW,
        reinterpret_cast<void**>(&shellLink)
    );

    if (FAILED(hr)) {
        CoUninitialize();
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to create shell link"));
    }

    // Set target
    shellLink->SetPath(targetPath.wstring().c_str());
    auto wideWorkDir = targetPath.parent_path().wstring();
    shellLink->SetWorkingDirectory(wideWorkDir.c_str());

    // Set description
    if (!description.empty()) {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, description.c_str(), -1,
            nullptr, 0);
        if (wideLen > 0) {
            std::wstring wideDesc(wideLen - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, description.c_str(), -1,
                wideDesc.data(), wideLen);
            shellLink->SetDescription(wideDesc.c_str());
        }
    }

    // Set icon
    if (!iconPath.empty()) {
        shellLink->SetIconLocation(iconPath.wstring().c_str(), iconIndex);
    }

    // Save
    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile,
        reinterpret_cast<void**>(&persistFile));

    if (SUCCEEDED(hr)) {
        hr = persistFile->Save(shortcutPath.wstring().c_str(), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    CoUninitialize();

    if (FAILED(hr)) {
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to save shortcut"));
    }

    return {};
}

} // namespace makine::platform::windows

#endif // _WIN32
