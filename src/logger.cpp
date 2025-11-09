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

#include "logger.h"

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

/**
 * @brief Logs a message with prefix and direction indicator.
 * 
 * Messages are written to both file and console based on their respective trace level thresholds.
 * 
 * @param prefix Logical source identifier (e.g., engine name).
 * @param message The message content to log.
 * @param isOutput true for outgoing messages (->), false for incoming (<-).
 * @param cliThreshold Trace level threshold for console output.
 * @param fileThreshold Trace level threshold for file logging.
 * @param level The trace level of this message (default: info).
 */
void Logger::log(std::string_view prefix, std::string_view message, bool isOutput, 
    TraceLevel cliThreshold, TraceLevel fileThreshold, TraceLevel level) {

    std::scoped_lock lock(mutex_);
    if (level <= fileThreshold && fileStream_.is_open()) {
        fileStream_ << prefix << (isOutput ? " -> " : " <- ") << message << "\n" << std::flush;
    }
    
    if (level > cliThreshold) {
        return;
    }
    if (message.empty()) {
        std::cout << prefix << (isOutput ? " -> " : " <- ") << "\n" << std::flush;
        return;
    }
    std::cout << prefix << (isOutput ? " -> " : " <- ") << message << "\n" << std::flush;
}

/**
 * @brief Logs a simple message without prefix.
 * @param message The message content to log.
 * @param level The trace level of this message (default: command).
 */
void Logger::log(std::string_view message, TraceLevel level) {

    std::scoped_lock lock(mutex_);
    if (level <= fileThreshold_ && fileStream_.is_open()) {
        fileStream_ << message << "\n" << std::flush;
    }

    if (level > cliThreshold_) {
        return;
    }
    std::cout << message << "\n" << std::flush;
}

/**
 * @brief Logs a message with aligned topic and content.
 * 
 * The topic is left-aligned with a fixed width for consistent formatting.
 * 
 * @param topic The topic or label to display (will be aligned).
 * @param message The message content to display after the topic.
 * @param level The trace level of this message (default: command).
 */
void Logger::logAligned(std::string_view topic, std::string_view message, TraceLevel level) {
    std::ostringstream oss;
    oss << std::left << std::setw(30) << topic << message;
    log(oss.str(), level);
}

/**
 * @brief Sets the output log file with timestamp.
 * 
 * Creates a new log file with a timestamped filename in the configured log directory.
 * If a file is already open, it will be closed first.
 * 
 * @param basename Base name for the log file (timestamp will be appended).
 */
void Logger::setLogFile(const std::string& basename) {
    std::scoped_lock lock(mutex_);
    namespace fs = std::filesystem;
    fs::path path = logPath_.empty() ? "" : fs::path(logPath_);
    filename_ = (path / generateTimestampedFilename(basename)).string();
    fileStream_.close();
    fileStream_.open(filename_, std::ios::app);
}

/**
 * @brief Generates a timestamped filename.
 * 
 * Creates a filename in the format: basename-YYYY-MM-DD_HH-MM-SS.mmm.log
 * 
 * @param baseName The base name for the file.
 * @return Complete filename with timestamp and .log extension.
 */
std::string Logger::generateTimestampedFilename(const std::string& baseName) {
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
    return oss.str();
}

/**
 * @brief Returns the global engine logger instance.
 * @return Reference to the singleton engine logger.
 */
Logger& Logger::engineLogger() {
    static Logger instance;
    return instance;
}

/**
 * @brief Returns the global test logger instance.
 * @return Reference to the singleton test logger.
 */
Logger& Logger::testLogger() {
    static Logger instance;
    return instance;
}

} // namespace QaplaTester
