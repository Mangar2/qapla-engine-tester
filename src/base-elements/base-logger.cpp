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

#include "base-logger.h"

#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace QaplaTester {

/**
 * @brief Converts TraceLevel enum to its string representation.
 * @param level The trace level to convert.
 * @return String representation of the trace level.
 */
std::string to_string(QaplaTester::TraceLevel level) {
    switch (level) {
        case QaplaTester::TraceLevel::error: return "error";
        case QaplaTester::TraceLevel::command: return "command";
        case QaplaTester::TraceLevel::result: return "result";
        case QaplaTester::TraceLevel::warning: return "warning";
        case QaplaTester::TraceLevel::info: return "all";
        case QaplaTester::TraceLevel::none: return "none";
        default: return "command";
    }
}

void BaseLogger::ensureFileOpen(const std::string& logPath) {
    std::string basename = getBaseName();
    if (basename.empty()) {
        return;
    }

    if (fileStream_.is_open() && basename == openedBasename_ && logPath == openedLogPath_) {
        return;
    }

    if (fileStream_.is_open()) {
        fileStream_.close();
    }

    filename_ = generateTimestampedFilename(basename, logPath);
    openedBasename_ = basename;
    openedLogPath_ = logPath;
    fileStream_.open(filename_, std::ios::app);
}

std::string BaseLogger::getFilename() {
    std::scoped_lock lock(loggingMutex_);
    ensureFileOpen(logPath_);
    return filename_;
}

std::string BaseLogger::generateTimestampedFilename(const std::string& baseName, const std::string& logPath) {
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
        << ".log";
    
    namespace fs = std::filesystem;
    fs::path path = logPath.empty() ? "" : fs::path(logPath);
    return (path / oss.str()).string();
}

void BaseLogger::log(std::string_view message, TraceLevel level) {
    // Never used for per-engine loggers, thus no update to engineLogBuffers_
    std::scoped_lock lock(loggingMutex_);
    if (level <= fileThreshold_) {
        ensureFileOpen(logPath_);
        if (fileStream_.is_open()) {
            fileStream_ << message << "\n" << std::flush;
        }
    }

    if (level <= mcpThreshold_ && mcpCallback_) {
        mcpCallback_(message, "");
    }

    if (level <= cliThreshold_) {
        std::cout << message << "\n" << std::flush;
    }
}

void BaseLogger::logStatus(std::string_view message, std::string_view toolName, TraceLevel level, bool overwrite) {
    std::scoped_lock lock(loggingMutex_);

    if (level <= fileThreshold_) {
        ensureFileOpen(logPath_);
        if (fileStream_.is_open()) {
            fileStream_ << message << "\n" << std::flush;
        }
    }

    if (level <= mcpThreshold_ && mcpCallback_) {
        mcpCallback_(message, toolName);
    }

    if (level <= cliThreshold_) {
        if (overwrite) {
            std::cout << message << "\r" << std::flush;
        } else {
            std::cout << message << "\n" << std::flush;
        }
    }
}

} // namespace QaplaTester
