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
#pragma once
#include <mutex>
#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace QaplaTester {

/**
 * @brief Trace levels for logging control.
 * IMPORTANT: Order and numeric values matter for comparison logic!
 * Lower enum values = higher priority (more restrictive filtering).
 * Comparison logic: if (messageLevel <= threshold) -> message is logged
 * 
 * Example: If threshold is 'command', only 'none', 'error' and 'command' messages are logged.
 */
enum class TraceLevel : std::uint8_t {
    none = 0,    // Log nothing (most restrictive)
    error = 1,   // Log only errors
    command = 2, // Log errors + commands
    result = 3,  // Log errors + commands + results
    warning = 4, // Log errors + commands + results + warnings
    info = 5     // Log everything (least restrictive)
};

/**
 * @brief Converts TraceLevel enum to its string representation.
 * @param level The trace level to convert.
 * @return String representation of the trace level.
 */
std::string to_string(QaplaTester::TraceLevel level);


/**
 * @brief Thread-safe logger with optional file output and trace filtering.
 * 
 * Provides singleton instances for engine and test logging with configurable
 * trace levels for both console and file output.
 */
class Logger {
public:
    /**
     * @brief Constructs a logger with default error-level threshold.
     */
    Logger() = default;

    /**
     * @brief Destructor - closes the log file if open.
     */
    ~Logger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    /**
     * @brief Logs a message with prefix and direction indicator.
     * 
     * Messages are written to both file and console based on their respective trace level thresholds.
     * The direction is indicated by -> (output) or <- (input).
     * 
     * @param prefix Logical source identifier (e.g., engine name).
     * @param message The message content to log.
     * @param isOutput true for outgoing messages (->), false for incoming (<-).
     * @param cliThreshold Trace level threshold for console output.
     * @param fileThreshold Trace level threshold for file logging.
     * @param level The trace level of this message (default: info).
     */
    void log(std::string_view prefix, std::string_view message, bool isOutput, 
        TraceLevel cliThreshold, TraceLevel fileThreshold, TraceLevel level = TraceLevel::info);

    /**
     * @brief Logs a simple message without prefix.
     * 
     * Uses the logger's configured trace level thresholds for filtering.
     * 
     * @param message The message content to log.
     * @param level The trace level of this message (default: command).
     */
    void log(std::string_view message, TraceLevel level = TraceLevel::command);

    /**
     * @brief Logs a message with aligned topic and content.
     * 
     * The topic is left-aligned with a fixed width for consistent formatting.
     * 
     * @param topic The topic or label to display (will be aligned).
     * @param message The message content to display after the topic.
     * @param level The trace level of this message (default: command).
     */
    void logAligned(std::string_view topic, std::string_view message, TraceLevel level = TraceLevel::command);

    /**
     * @brief Sets the output log file with timestamp.
     * 
     * Creates a new log file with a timestamped filename in the configured log directory.
     * If a file is already open, it will be closed first.
     * 
     * @param basename Base name for the log file (timestamp will be appended).
     */
    void setLogFile(const std::string& basename);

    /**
     * @brief Returns the current log filename.
     * @return The full path and name of the log file.
     */
    [[nodiscard]] std::string getFilename() const {
        return filename_;
    }

    /**
     * @brief Sets the trace level thresholds for console and file logging.
     * 
     * Only messages with a level less than or equal to the threshold will be logged.
     * 
     * @param cli The minimum trace level for console output.
     * @param file The minimum trace level for file logging (default: info).
     */
    void setTraceLevel(TraceLevel cli, TraceLevel file = TraceLevel::info) {
        cliThreshold_ = cli;
        fileThreshold_ = file;
    }

    /**
     * @brief Returns the global engine logger instance.
     * 
     * Provides a singleton logger instance specifically for engine communication.
     * 
     * @return Reference to the singleton engine logger.
     */
    static Logger& engineLogger();

    /**
     * @brief Returns the global test logger instance.
     * 
     * Provides a singleton logger instance specifically for test execution.
     * 
     * @return Reference to the singleton test logger.
     */
    static Logger& testLogger();

    /**
     * @brief Sets the directory path for log files.
     * @param path The directory where log files will be created.
     */
    static void setLogPath(const std::string& path) {
        logPath_ = path;
    }

    /**
     * @brief Returns the current console trace level threshold.
     * @return The trace level threshold for console output.
     */
    [[nodiscard]] TraceLevel getCliThreshold() const {
        return cliThreshold_;
    }

private:
    /**
     * @brief Generates a timestamped filename.
     * 
     * Creates a filename in the format: basename-YYYY-MM-DD_HH-MM-SS.mmm.log
     * 
     * @param baseName The base name for the file.
     * @return Complete filename with timestamp and .log extension.
     */
    static std::string generateTimestampedFilename(const std::string& baseName);

    std::mutex mutex_;                          ///< Mutex for thread-safe logging
    std::ofstream fileStream_;                  ///< Output file stream for log file
    TraceLevel cliThreshold_ = TraceLevel::error;  ///< Console output threshold
    TraceLevel fileThreshold_ = TraceLevel::info;  ///< File output threshold
    std::string filename_;                      ///< Current log filename
    static inline std::string logPath_;    ///< Directory path for log files
};

} // namespace QaplaTester

