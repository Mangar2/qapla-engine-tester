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
#include <fstream>
#include <functional>

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
 * @brief Base class for loggers with file and console output.
 * 
 * Provides common functionality for file management and trace level handling.
 */
class BaseLogger {
public:
    /**
     * @brief Constructs a base logger with default error-level threshold.
     */
    BaseLogger() = default;

    /**
     * @brief Destructor - closes the log file if open.
     */
    virtual ~BaseLogger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    // Prevent copying
    BaseLogger(const BaseLogger&) = delete;
    BaseLogger& operator=(const BaseLogger&) = delete;

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
     * @brief Logs a status message to CLI (potentially overwriting line) and MCP.
     * @param message The status message.
     * @param toolName The name of the tool (for MCP JSON-RPC).
     * @param level The trace level (default: result).
     * @param overwrite If true, overwrites the current line on CLI using \r.
     */
    void logStatus(std::string_view message, std::string_view toolName = "", TraceLevel level = TraceLevel::result, bool overwrite = false);

    /**
     * @brief Returns the base name for log files.
     * 
     * Must be implemented by derived classes to return their static logBaseName_.
     * 
     * @return The base name for log files (without timestamp).
     */
    [[nodiscard]] virtual std::string getBaseName() const = 0;

    /**
     * @brief Returns the current log filename.
     * @return The full path and name of the log file.
     */
    [[nodiscard]] std::string getFilename();

    /**
     * @brief Sets the trace level thresholds for console and file logging.
     * 
     * Only messages with a level less than or equal to the threshold will be logged.
     * 
     * @param cli The minimum trace level for console output.
     * @param file The minimum trace level for file logging (default: info).
     * @param mcp The minimum trace level for MCP logging (default: none).
     */
    void setTraceLevel(TraceLevel cli, TraceLevel file = TraceLevel::info, TraceLevel mcp = TraceLevel::none) {
        cliThreshold_ = cli;
        fileThreshold_ = file;
        mcpThreshold_ = mcp;
    }

    /**
     * @brief Sets a callback for MCP JSON-RPC logging.
     * @param callback Function to call for MCP logging.
     */
    void setMcpCallback(std::function<void(std::string_view, std::string_view)> callback) {
        mcpCallback_ = std::move(callback);
    }

    /**
     * @brief Returns the current console trace level threshold.
     * @return The trace level threshold for console output.
     */
    [[nodiscard]] TraceLevel getCliThreshold() const {
        return cliThreshold_;
    }

    /**
     * @brief Returns the current file trace level threshold.
     * @return The trace level threshold for file output.
     */
    [[nodiscard]] TraceLevel getFileThreshold() const {
        return fileThreshold_;
    }

    /**
     * @brief Returns the current MCP trace level threshold.
     * @return The trace level threshold for MCP output.
     */
    [[nodiscard]] TraceLevel getMcpThreshold() const {
        return mcpThreshold_;
    }

    static inline std::string logPath_ = "./log";               ///< Directory path for log files

protected:
    /**
     * @brief Generates a timestamped filename.
     * 
     * Creates a filename in the format: basename-YYYY-MM-DD_HH-MM-SS.mmm.log
     * 
     * @param baseName The base name for the file.
     * @param logPath The directory path for log files.
     * @return Complete filename with timestamp and .log extension.
     */
    static std::string generateTimestampedFilename(const std::string& baseName, const std::string& logPath);

    /**
     * @brief Opens the log file if needed (lazy initialization).
     * 
     * Opens a new file if:
     * - No file is currently open, OR
     * - The basename has changed since last open
     * 
     * @param logPath The directory path for log files.
     */
    void ensureFileOpen(const std::string& logPath);

    /**
     * @brief Hook called immediately after a new log file is successfully opened.
     * 
     * Derived classes can use this to write headers or initial status information.
     */
    virtual void onFileOpened() {}

    std::mutex loggingMutex_;                   ///< Mutex for thread-safe logging
    std::ofstream fileStream_;                  ///< Output file stream for log file
    TraceLevel cliThreshold_ = TraceLevel::error;  ///< Console output threshold
    TraceLevel fileThreshold_ = TraceLevel::info;  ///< File output threshold
    TraceLevel mcpThreshold_ = TraceLevel::none;  ///< MCP output threshold
    std::function<void(std::string_view, std::string_view)> mcpCallback_; ///< Callback for MCP logging
    std::string filename_;                      ///< Current log filename
    std::string openedBasename_;                ///< Basename of the currently open log file
    std::string openedLogPath_;                 ///< Directory path of the currently open log file
};

} // namespace QaplaTester
