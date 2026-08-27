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

#include "base-logger.h"
#include "change-tracker.h"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <optional>
#include <array>
#include <functional>
#include <chrono>

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
    struct Entry {
        std::string data;
        std::chrono::system_clock::time_point timestamp;
        size_t inputCount = 0;
    };
    void push(Entry item) {
        item.inputCount = totalInputCount_++;
        buffer_[(head_ + count_) % MAX_ENGINE_LOG_LINES] = std::move(item);
        if (count_ < MAX_ENGINE_LOG_LINES) {
            ++count_;
        } else {
            head_ = (head_ + 1) % MAX_ENGINE_LOG_LINES;
        }
        changeTracker_.trackUpdate();
    }
    
    const auto& operator[](size_t index) const {
        return buffer_[(head_ + index) % MAX_ENGINE_LOG_LINES];
    }
    
    [[nodiscard]] size_t size() const { return count_; }
    
    void clear() {
        head_ = 0;
        count_ = 0;
        changeTracker_.trackModification();
    }

    [[nodiscard]] const ChangeTracker& getChangeTracker() const {
        return changeTracker_;
    }
    
private:
    std::array<Entry, MAX_ENGINE_LOG_LINES> buffer_;
    ChangeTracker changeTracker_;
    size_t head_ = 0;
    size_t count_ = 0;
    size_t totalInputCount_ = 0;
};

/**
 * @brief Strategy for creating log files for engine communication.
 */
enum class LogFileStrategy : std::uint8_t {
    global = 0,     // Single log file for all engines
    perEngine = 1  // One log file per engine
};

/**
 * @brief Combined configuration settings for both logger types.
 * This exists for backward compatibility with setConfig().
 */
struct LoggerConfig {
    std::string logPath = "./log";               ///< Directory path for log files
    std::string reportLogBaseName;          ///< Base name for reporting log files
    std::string engineLogBaseName;          ///< Base name for engine log files
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
 * @brief Thread-safe report logger with file output and trace filtering.
 * 
 * Simple logger for test/report output. Always global (singleton).
 * This will be renamed to ReportLogger via IDE refactoring.
 */
class Logger : public BaseLogger {
public:
    /** @brief Never destroyed -- see BaseLogger::logPath_. */
    static inline std::string& logBaseName_ = *new std::string();  ///< Base name for report logs

    /**
     * @brief Returns the static welcome message for the application.
     * @return The welcome message string.
     */
    static std::string getWelcomeMessage() {
        return "Qapla Engine Tester - Prerelease 0.7.0 (c) by Volker Boehm\n";
    }

    /**
     * @brief Writes the welcome message to the file on opening.
     */
    void onFileOpened() override {
        if (fileStream_.is_open()) {
            fileStream_ << getWelcomeMessage() << "\n";
        }
    }

    /**
     * @brief Constructs a logger with default error-level threshold.
     */
    Logger() = default;

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
     * @brief Returns the global report logger instance.
     * 
     * Provides a singleton logger instance specifically for test execution.
     * 
     * @return Reference to the singleton report logger.
     */
    static Logger& reportLogger();

    /**
     * @brief Returns the base name for log files.
     * @return The static logBaseName_ for report logs.
     */
    [[nodiscard]] std::string getBaseName() const override {
        return logBaseName_;
    }
    
};

/**
 * @brief Thread-safe engine logger with optional file output and trace filtering.
 * 
 * Complex logger for engine communication with multiple strategies (global, per-engine, per-game).
 */
class EngineLogger : public BaseLogger {
public:
    static inline LogFileStrategy logStrategy_ = LogFileStrategy::global; ///< Strategy for engine log files
    /** @brief Never destroyed -- see BaseLogger::logPath_. */
    static inline std::string& logBaseName_ = *new std::string();  ///< Base name for engine logs

    /**
     * @brief Constructs an engine logger with default error-level threshold.
     */
    EngineLogger() = default;

    using BaseLogger::log;

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
     * @brief Returns the global engine logger instance.
     * 
     * Provides a singleton logger instance specifically for engine communication.
     * 
     * @return Reference to the singleton engine logger.
     */
    static EngineLogger& engineLogger();

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
    static EngineLogger& engineLogger(const EngineLoggerId& loggerId);

    /**
     * @brief Notification that an engine has terminated.
     * 
     * Closes the log file and removes this logger from the internal map.
     * Only applicable for dynamically created loggers (perEngine, perGame).
     * For the global logger, only the file is closed but the logger instance remains.
     * Frees the file handle immediately.
     * @param loggerId The identity of the terminated engine.
     */
    void engineTerminated(const EngineLoggerId& loggerId);

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
    static void withEngineLogBuffer(
        const std::string& engineId,
        const std::function<void(const RingBuffer&)>& callback);

    /**
     * @brief Returns the base name for log files.
     * @return The static logBaseName_ for engine logs.
     */
    [[nodiscard]] std::string getBaseName() const override {
        // For perEngine strategy, append the engineId to the base name
        if (id_.engineId.has_value()) {
            return logBaseName_ + "-" + *id_.engineId;
        }
        // For perGame strategy, append the gameId to the base name
        if (id_.gameId.has_value()) {
            return logBaseName_ + "-" + *id_.gameId;
        }
        // For global strategy (or if id_ is not set), return base name as-is
        return logBaseName_;
    }

private:
    /**
     * @brief Implementation for global strategy.
     * @return Reference to the global engine logger.
     */
    static EngineLogger& engineLoggerGlobal();

    /**
     * @brief Implementation for perEngine strategy.
     * @param id The logger identity.
     * @return Reference to the logger for this engine.
     */
    static EngineLogger& engineLoggerPerEngine(const EngineLoggerId& id);

    /**
     * @brief Implementation for perGame strategy.
     * @param id The logger identity.
     * @return Reference to the logger for this game.
     */
    static EngineLogger& engineLoggerPerGame(const EngineLoggerId& id);

    EngineLoggerId id_;                         ///< Identity of this logger (for self-removal from map)

    /** @brief None of these are ever destroyed -- see BaseLogger::logPath_. */
    static inline std::mutex& mapMutex_ = *new std::mutex();         ///< Guards the logger map
    static inline std::mutex& engineLogBufferMutex_ = *new std::mutex();  ///< Guards the buffer map

    ///< Map to loggers for each engine used, if the logging strategy is per engine
    static inline std::unordered_map<std::string, std::unique_ptr<EngineLogger>>& engineLoggers_ =
        *new std::unordered_map<std::string, std::unique_ptr<EngineLogger>>();
    static inline std::unordered_map<std::string, RingBuffer>& engineLogBuffers_ =
        *new std::unordered_map<std::string, RingBuffer>();  ///< Ring buffer per engine
};

/**
 * @brief Backward compatibility function for setting both logger configurations.
 * 
 * @param config The combined configuration to apply to both loggers.
 */
inline void setLoggerConfig(const LoggerConfig& config) {
    Logger::logBaseName_ = config.reportLogBaseName;
    BaseLogger::logPath_ = config.logPath;
    
    EngineLogger::logBaseName_ = config.engineLogBaseName;
    EngineLogger::logStrategy_ = config.engineLogStrategy;
}

/**
 * @brief Backward compatibility function for getting combined logger configuration.
 * 
 * @return The combined configuration from both loggers.
 */
inline LoggerConfig getLoggerConfig() {
    return {
        .logPath = BaseLogger::logPath_,
        .reportLogBaseName = Logger::logBaseName_,
        .engineLogBaseName = EngineLogger::logBaseName_,
        .engineLogStrategy = EngineLogger::logStrategy_
    };
}

} // namespace QaplaTester

