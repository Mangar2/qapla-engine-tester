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

#include "change-tracker.h"

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
#include <unordered_map>
#include <memory>
#include <optional>
#include <array>
#include <functional>

namespace QaplaTester {

/// Maximum number of log lines to keep in memory per engine
constexpr size_t MAX_ENGINE_LOG_LINES = 1000;

/**
 * @brief Ring buffer for storing log lines with fixed capacity.
 * 
 * Efficiently stores the last N log lines without heap allocations after initialization.
 */
class RingBuffer {
public:
    void push(const std::string& item) {
        buffer_[(head_ + count_) % MAX_ENGINE_LOG_LINES] = item;
        if (count_ < MAX_ENGINE_LOG_LINES) {
            ++count_;
        } else {
            head_ = (head_ + 1) % MAX_ENGINE_LOG_LINES;
        }
        changeTracker_.trackUpdate();
    }
    
    const std::string& operator[](size_t index) const {
        return buffer_[(head_ + index) % MAX_ENGINE_LOG_LINES];
    }
    
    size_t size() const { return count_; }
    
    void clear() {
        head_ = 0;
        count_ = 0;
        changeTracker_.trackModification();
    }

    const ChangeTracker& getChangeTracker() const {
        return changeTracker_;
    }
    
private:
    std::array<std::string, MAX_ENGINE_LOG_LINES> buffer_;
    ChangeTracker changeTracker_;
    size_t head_ = 0;
    size_t count_ = 0;
};

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
 * @brief Strategy for creating log files for engine communication.
 */
enum class LogFileStrategy : std::uint8_t {
    global = 0,     // Single log file for all engines
    perEngine = 1,  // One log file per engine
    perGame = 2     // One log file per game (all engines of a game share the file)
};

/**
 * @brief Configuration settings for the logger.
 */
struct LoggerConfig {
    std::string logPath = "./log";               ///< Directory path for log files
    std::string reportLogBaseName = "report";    ///< Base name for reporting log files
    std::string engineLogBaseName = "engine";    ///< Base name for engine log files
    LogFileStrategy engineLogStrategy = LogFileStrategy::global; ///< Strategy for engine log files
};

/**
 * @brief Parameters for requesting an engine logger instance.
 * 
 * Uses std::optional to distinguish between "empty string" and "not provided".
 * Only the parameters relevant for the current LogFileStrategy are used.
 */
struct EngineLoggerId {
    std::optional<std::string> engineId{};  ///< Engine identifier (used for perEngine strategy)
    std::optional<std::string> gameId{};    ///< Game identifier (used for perGame strategy)
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
     * @param engineId Logical source identifier.
     * @param message The message content to log.
     * @param isOutput true for outgoing messages (->), false for incoming (<-).
     * @param cliThreshold Trace level threshold for console output.
     * @param fileThreshold Trace level threshold for file logging.
     * @param level The trace level of this message (default: info).
     */
    void log(const std::string& engineId, std::string_view message, bool isOutput, 
        TraceLevel cliThreshold, TraceLevel fileThreshold, TraceLevel level = TraceLevel::info);

    /**
     * @brief Logs a simple message without prefix.
     * 
     * Uses the logger's configured trace level thresholds for filtering.
     * This is used for general log messages not associated with engine I/O and thus never per engine.
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
     * @brief Sets the base name for the log file.
     * 
     * The actual log file will be created lazily on the first write operation.
     * If the basename changes, a new file will be created on the next write.
     * 
     * @param basename Base name for the log file (timestamp will be appended when file is created).
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
    static Logger& reportLogger();

    /**
     * @brief Returns the current console trace level threshold.
     * @return The trace level threshold for console output.
     */
    [[nodiscard]] TraceLevel getCliThreshold() const {
        return cliThreshold_;
    }

    /**
     * @brief Sets the logger configuration and applies it to all logger instances.
     * @param config The configuration to apply.
     */
    static void setConfig(const LoggerConfig& config);

    /**
     * @brief Returns the current logger configuration.
     * @return Reference to the logger configuration.
     */
    static LoggerConfig& getConfig() {
        return config_;
    }

    /**
     * @brief Returns the appropriate engine logger based on current strategy and ID.
     * 
     * This is the main public interface for obtaining engine loggers. The behavior
     * depends on the configured LogFileStrategy:
     * - global: Returns single global logger (engineId parameters ignored)
     * - perEngine: Uses loggerId.engineId to identify the logger
     * - perGame: Uses loggerId.gameId to identify the logger
     * 
     * @param loggerId Parameters identifying which logger to return (strategy-dependent).
     * @return Reference to the appropriate engine logger.
     */
    static Logger& engineLogger(const EngineLoggerId& loggerId);

    /**
     * @brief Closes the log file and removes this logger from the internal map.
     * 
     * Only applicable for dynamically created loggers (perEngine, perGame).
     * For the global logger, only the file is closed but the logger instance remains.
     * Frees the file handle immediately.
     * @param loggerId The identity of the logger to close.
     */
    void close(const EngineLoggerId& loggerId);

    /**
     * @brief Clears all dynamically created logger instances.
     * 
     * Useful for cleanup or when switching strategies.
     * The global singleton loggers are not affected.
     */
    static void clearEngineLoggers();

    /**
     * @brief Provides thread-safe read access to the engine log buffer.
     * 
     * The callback is invoked while holding the mutex, ensuring consistent data access.
     * This is a zero-copy operation - the callback receives a const reference.
     * 
     * @param engineId The engine identifier.
     * @param callback Function to call with the log buffer (if it exists).
     *                 The callback is only called if a buffer exists for this engine.
     */
    static void accessEngineLogBuffer(
        const std::string& engineId,
        const std::function<void(const RingBuffer&)>& callback);


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

    /**
     * @brief Implementation for global strategy.
     * @return Reference to the global engine logger.
     */
    static Logger& engineLoggerGlobal();

    /**
     * @brief Implementation for perEngine strategy.
     * @param id The logger identity.
     * @return Reference to the logger for this engine.
     */
    static Logger& engineLoggerPerEngine(const EngineLoggerId& id);

    /**
     * @brief Implementation for perGame strategy.
     * @param id The logger identity.
     * @return Reference to the logger for this game.
     */
    static Logger& engineLoggerPerGame(const EngineLoggerId& id);

    /**
     * @brief Opens the log file if needed (lazy initialization).
     * 
     * Opens a new file if:
     * - No file is currently open, OR
     * - The basename has changed since last open
     */
    void ensureFileOpen();

    std::mutex loggingMutex_;                   ///< Mutex for thread-safe logging
    std::ofstream fileStream_;                  ///< Output file stream for log file
    TraceLevel cliThreshold_ = TraceLevel::error;  ///< Console output threshold
    TraceLevel fileThreshold_ = TraceLevel::info;  ///< File output threshold
    std::string filename_;                      ///< Current log filename
    std::string basename_;                      ///< Base name for log file (without timestamp)
    EngineLoggerId id_;                         ///< Identity of this logger (for self-removal from map)
    static inline LoggerConfig config_;         ///< Logger configuration
    static inline std::mutex mapMutex_;         ///< Mutex for thread-safe map access
    static inline std::mutex engineLogBufferMutex_;  ///< Mutex for engine log buffer map
    
    ///< Map to loggers for each engine used, if the logging strategy is per engine
    static inline std::unordered_map<std::string, std::unique_ptr<Logger>> engineLoggers_;  
    static inline std::unordered_map<std::string, RingBuffer> engineLogBuffers_;            ///< Map to ring buffer for each engine
};

} // namespace QaplaTester

