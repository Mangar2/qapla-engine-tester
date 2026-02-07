/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include "file-helper.h"
#include "app-error.h"
#include <format>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>
#include <cctype>

namespace QaplaHelpers {

namespace {

/**
 * @brief Performs OS-independent validation of an output path.
 * 
 * @param path The path to validate.
 * @throws QaplaTester::AppError if basic validation fails.
 */
void validateCommon(const std::filesystem::path& path) {
    if (path.empty()) {
        throw QaplaTester::AppError::makeInvalidParameters("The output path is empty.");
    }

    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        throw QaplaTester::AppError::makeInvalidParameters(
            std::format("The specified path is an existing directory, not a file: '{}'", path.string()));
    }

    const auto parentPath = path.parent_path();
    if (!parentPath.empty()) {
        if (!std::filesystem::exists(parentPath)) {
            throw QaplaTester::AppError::makeInvalidParameters(
                std::format("The target directory does not exist: '{}'", parentPath.string()));
        }
        if (!std::filesystem::is_directory(parentPath)) {
            throw QaplaTester::AppError::makeInvalidParameters(
                std::format("The parent path is not a directory: '{}'", parentPath.string()));
        }

        const auto permissions = std::filesystem::status(parentPath).permissions();
        const bool isWritable = (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none ||
            (permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none ||
            (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none;

        if (!isWritable) {
            throw QaplaTester::AppError::makeInvalidParameters(
                std::format("No write permissions in target directory: '{}'", parentPath.string()));
        }
    }

    if (path.filename().empty()) {
        throw QaplaTester::AppError::makeInvalidParameters("The filename is missing (path ends with a separator).");
    }

    const auto filename = path.filename().string();
    for (const auto character : filename) {
        if (std::iscntrl(static_cast<unsigned char>(character)) != 0) {
            throw QaplaTester::AppError::makeInvalidParameters("Filename contains control characters.");
        }
    }
}

} // namespace

#ifdef _WIN32
void validateOutputPath(const std::filesystem::path& path) {
    validateCommon(path);

    const auto filename = path.filename().string();
    const std::string invalidCharacters = "<>:\"/\\|?*";
    for (const auto character : filename) {
        if (invalidCharacters.find(character) != std::string::npos) {
            throw QaplaTester::AppError::makeInvalidParameters(
                std::format("Filename contains invalid Windows character: '{}'", character));
        }
    }

    auto nameUpper = filename;
    std::ranges::transform(nameUpper, nameUpper.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });

    const size_t lastDot = nameUpper.find_last_of('.');
    if (lastDot != std::string::npos) {
        nameUpper = nameUpper.substr(0, lastDot);
    }

    const std::set<std::string> reservedNames = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };

    if (reservedNames.contains(nameUpper)) {
        throw QaplaTester::AppError::makeInvalidParameters(
            std::format("The filename is a reserved Windows system name: '{}'", filename));
    }
}
#elif defined(__linux__)
void validateOutputPath(const std::filesystem::path& path) {
    validateCommon(path);
    // Linux specific: Filename cannot be empty or contains '/' or null byte (already handled by FS and common)
}
#elif defined(__APPLE__)
void validateOutputPath(const std::filesystem::path& path) {
    validateCommon(path);
    // MacOS specific: similar to Linux
}
#else
void validateOutputPath(const std::filesystem::path& path) {
    validateCommon(path);
}
#endif

std::string generateTimestampedFilename(const std::string& baseName, const std::string& logPath, const std::string& extension) {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &local_tm);
#endif

    std::ostringstream oss;
    oss << baseName << '-'
        << std::put_time(&local_tm, "%Y-%m-%d_%H-%M-%S")
        << '.' << std::setw(3) << std::setfill('0') << now_ms.count()
        << "." << extension;
    
    namespace fs = std::filesystem;
    fs::path path = logPath.empty() ? "" : fs::path(logPath);
    return (path / oss.str()).string();
}

}