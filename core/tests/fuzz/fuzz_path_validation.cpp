/**
 * @file fuzz_path_validation.cpp
 * @brief Fuzzer for path validation and sanitization
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests path traversal prevention, symlink handling, and path normalization.
 * Critical for security: prevents directory traversal attacks.
 *
 * Run:
 *   ./fuzz_path_validation corpus/paths -max_len=4096
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <filesystem>

#include <makineai/path_utils.hpp>
#include <makineai/sandbox.hpp>

using namespace makineai;
namespace fs = std::filesystem;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Need some content
    if (size < 1) {
        return 0;
    }

    // Limit path length
    if (size > 4096) {
        return 0;
    }

    try {
        // Convert to string
        std::string pathStr(reinterpret_cast<const char*>(data), size);

        // =====================================================================
        // Test path normalization
        // =====================================================================

        // Should handle any byte sequence without crashing
        auto normalized = path::normalize(pathStr);
        volatile size_t normLen = normalized.string().length();
        (void)normLen;

        // =====================================================================
        // Test path traversal detection
        // =====================================================================

        // Base directory for containment check
        fs::path baseDir = "/safe/game/directory";

        // Check if path attempts traversal
        auto isContained = path::isContainedIn(pathStr, baseDir);
        volatile bool contained = isContained;
        (void)contained;

        // Check for path traversal patterns
        auto hasTraversal = path::containsTraversalPattern(pathStr);
        volatile bool traversal = hasTraversal;
        (void)traversal;

        // =====================================================================
        // Test path sanitization
        // =====================================================================

        // Remove dangerous characters and sequences
        auto sanitized = path::sanitize(pathStr);
        volatile size_t sanLen = sanitized.length();
        (void)sanLen;

        // Sanitize for specific platforms
        auto sanitizedWin = path::sanitizeForWindows(pathStr);
        volatile size_t sanWinLen = sanitizedWin.length();
        (void)sanWinLen;

        auto sanitizedUnix = path::sanitizeForUnix(pathStr);
        volatile size_t sanUnixLen = sanitizedUnix.length();
        (void)sanUnixLen;

        // =====================================================================
        // Test path component extraction
        // =====================================================================

        fs::path fsPath(pathStr);

        // Extract components safely
        auto filename = path::safeFilename(fsPath);
        volatile size_t fnLen = filename.length();
        (void)fnLen;

        auto extension = path::safeExtension(fsPath);
        volatile size_t extLen = extension.length();
        (void)extLen;

        auto stem = path::safeStem(fsPath);
        volatile size_t stemLen = stem.length();
        (void)stemLen;

        // =====================================================================
        // Test path validation
        // =====================================================================

        // Check if valid filename
        auto validFilename = path::isValidFilename(pathStr);
        volatile bool vfn = validFilename;
        (void)vfn;

        // Check if valid path
        auto validPath = path::isValidPath(pathStr);
        volatile bool vp = validPath;
        (void)vp;

        // Check for reserved names (Windows: CON, PRN, etc.)
        auto isReserved = path::isReservedName(pathStr);
        volatile bool reserved = isReserved;
        (void)reserved;

        // =====================================================================
        // Test path joining with safety
        // =====================================================================

        if (size >= 2) {
            // Split input into base and relative parts
            size_t mid = size / 2;
            std::string basePart(reinterpret_cast<const char*>(data), mid);
            std::string relPart(reinterpret_cast<const char*>(data + mid), size - mid);

            // Safe join should prevent traversal
            auto joined = path::safeJoin(basePart, relPart);
            volatile size_t joinLen = joined.string().length();
            (void)joinLen;

            // Verify joined path is still contained
            auto stillContained = path::isContainedIn(joined, basePart);
            volatile bool sc = stillContained;
            (void)sc;
        }

        // =====================================================================
        // Test symlink and special file detection
        // =====================================================================

        // Check for symlink patterns in path string
        auto looksLikeSymlink = path::mightBeSymlinkTarget(pathStr);
        volatile bool symlink = looksLikeSymlink;
        (void)symlink;

        // Check for device file patterns
        auto isDevicePath = path::isDeviceFilePath(pathStr);
        volatile bool device = isDevicePath;
        (void)device;

        // =====================================================================
        // Test encoding handling
        // =====================================================================

        // Handle different path encodings
        auto utf8Path = path::toUtf8(pathStr);
        volatile size_t utf8Len = utf8Path.length();
        (void)utf8Len;

        // Detect encoding issues
        auto hasEncodingIssues = path::hasInvalidEncoding(pathStr);
        volatile bool encIssues = hasEncodingIssues;
        (void)encIssues;

        // =====================================================================
        // Test case sensitivity handling
        // =====================================================================

        // Normalize case for comparison
        auto lowerPath = path::toLowerPath(pathStr);
        volatile size_t lowerLen = lowerPath.length();
        (void)lowerLen;

        // Compare paths case-insensitively
        if (size >= 2) {
            size_t mid = size / 2;
            std::string path1(reinterpret_cast<const char*>(data), mid);
            std::string path2(reinterpret_cast<const char*>(data + mid), size - mid);

            auto equal = path::equalsCaseInsensitive(path1, path2);
            volatile bool eq = equal;
            (void)eq;
        }

        // =====================================================================
        // Test path length limits
        // =====================================================================

        // Check against platform limits
        auto exceedsWindowsLimit = path::exceedsWindowsPathLimit(pathStr);
        volatile bool winLimit = exceedsWindowsLimit;
        (void)winLimit;

        auto exceedsUnixLimit = path::exceedsUnixPathLimit(pathStr);
        volatile bool unixLimit = exceedsUnixLimit;
        (void)unixLimit;

        // Truncate to safe length
        auto truncated = path::truncateToLimit(pathStr, 260);
        volatile size_t truncLen = truncated.length();
        (void)truncLen;

    } catch (const std::bad_alloc&) {
        return 0;
    } catch (const std::exception&) {
        return 0;
    } catch (...) {
        return 0;
    }

    return 0;
}

// Seed corpus with various path patterns
#ifdef GENERATE_CORPUS
#include <fstream>

void generateSeedCorpus(const char* outputDir) {
    // Normal paths
    std::ofstream(std::string(outputDir) + "/seed_normal.txt")
        << "game/data/strings.json";

    // Absolute path
    std::ofstream(std::string(outputDir) + "/seed_absolute.txt")
        << "/home/user/games/test/data.bin";

    // Windows path
    std::ofstream(std::string(outputDir) + "/seed_windows.txt")
        << "C:\\Games\\Test\\data\\strings.json";

    // Traversal attempt
    std::ofstream(std::string(outputDir) + "/seed_traversal.txt")
        << "../../../etc/passwd";

    // Double traversal
    std::ofstream(std::string(outputDir) + "/seed_double_traversal.txt")
        << "data/....//....//....//etc/passwd";

    // URL encoded traversal
    std::ofstream(std::string(outputDir) + "/seed_url_encoded.txt")
        << "data/%2e%2e/%2e%2e/%2e%2e/etc/passwd";

    // Null byte injection
    std::ofstream(std::string(outputDir) + "/seed_null_byte.txt")
        << std::string("data/file.txt\0.jpg", 18);

    // Unicode normalization attack
    std::ofstream(std::string(outputDir) + "/seed_unicode.txt")
        << "data/\xc0\xae\xc0\xae/etc/passwd";  // Overlong encoding

    // Reserved Windows names
    std::ofstream(std::string(outputDir) + "/seed_reserved.txt")
        << "CON";

    std::ofstream(std::string(outputDir) + "/seed_reserved2.txt")
        << "PRN.txt";

    std::ofstream(std::string(outputDir) + "/seed_reserved3.txt")
        << "data/NUL/file.txt";

    // Very long path
    std::ofstream out(std::string(outputDir) + "/seed_long.txt");
    for (int i = 0; i < 100; ++i) {
        out << "verylongdirectoryname/";
    }
    out << "file.txt";

    // Special characters
    std::ofstream(std::string(outputDir) + "/seed_special.txt")
        << "data/file<>:\"|?*.txt";

    // Symlink patterns
    std::ofstream(std::string(outputDir) + "/seed_symlink.txt")
        << "/proc/self/root/etc/passwd";

    // Device files
    std::ofstream(std::string(outputDir) + "/seed_device.txt")
        << "/dev/null";

    std::ofstream(std::string(outputDir) + "/seed_device_win.txt")
        << "\\\\.\\PhysicalDrive0";

    // Mixed separators
    std::ofstream(std::string(outputDir) + "/seed_mixed_sep.txt")
        << "data\\subdir/file.txt";

    // Trailing spaces/dots (Windows issue)
    std::ofstream(std::string(outputDir) + "/seed_trailing.txt")
        << "data/file.txt   ";

    std::ofstream(std::string(outputDir) + "/seed_trailing_dot.txt")
        << "data/file.txt...";

    // UNC path
    std::ofstream(std::string(outputDir) + "/seed_unc.txt")
        << "\\\\server\\share\\file.txt";

    // Path with query string (web-style)
    std::ofstream(std::string(outputDir) + "/seed_query.txt")
        << "data/file.txt?param=value";
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    generateSeedCorpus(argv[1]);
    return 0;
}
#endif
