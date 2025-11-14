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
#include <format>

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

void Logger::log(const std::string& engineId, std::string_view message, bool isOutput, 
    TraceLevel cliThreshold, TraceLevel fileThreshold, TraceLevel level) {

    std::scoped_lock lock(loggingMutex_);

    auto toLog = std::format("{} {} {}", engineId, isOutput ? "->" : "<-", message);
    
    // Add to ring buffer if this logger has an engineId
    if (!engineId.empty()) {
        std::scoped_lock bufferLock(engineLogBufferMutex_);
        engineLogBuffers_[engineId].push(toLog);
    }
    
    if (level <= fileThreshold) {
        ensureFileOpen();
        if (fileStream_.is_open()) {
            fileStream_ << toLog << "\n" << std::flush;
        }
    }
    
    if (level <= cliThreshold) {
        std::cout << toLog << "\n" << std::flush;
    }

}

void Logger::log(std::string_view message, TraceLevel level) {
    // Never used for per-engine loggers, thus no update to engineLogBuffers_
    std::scoped_lock lock(loggingMutex_);
    if (level <= fileThreshold_) {
        ensureFileOpen();
        if (fileStream_.is_open()) {
            fileStream_ << message << "\n" << std::flush;
        }
    }

    if (level > cliThreshold_) {
        return;
    }
    std::cout << message << "\n" << std::flush;
}

void Logger::logAligned(std::string_view topic, std::string_view message, TraceLevel level) {
    std::ostringstream oss;
    oss << std::left << std::setw(30) << topic << message;
    log(oss.str(), level);
}

void Logger::setLogFile(const std::string& basename) {
    std::scoped_lock lock(loggingMutex_);
    basename_ = basename;
    // Note: File is NOT opened here. It will be opened lazily in ensureFileOpen().
}

void Logger::ensureFileOpen() {
    // Check if we need to open/reopen the file
    if (basename_.empty()) {
        return; // No basename set, don't create file
    }
    
    // If file is already open and basename hasn't changed, nothing to do
    if (fileStream_.is_open() && filename_.find(basename_) != std::string::npos) {
        return;
    }
    
    // Close existing file if open
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    
    // Create new timestamped filename
    namespace fs = std::filesystem;
    fs::path path = config_.logPath.empty() ? "" : fs::path(config_.logPath);
    filename_ = (path / generateTimestampedFilename(basename_)).string();
    
    // Open the file
    fileStream_.open(filename_, std::ios::app);
}

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

Logger& Logger::engineLogger() {
    static Logger instance;
    return instance;
}

Logger& Logger::reportLogger() {
    static Logger instance;
    return instance;
}

void Logger::setConfig(const LoggerConfig& config) {
    config_ = config;
    
    // Always initialize test/report logger
    reportLogger().setLogFile(config_.reportLogBaseName);
    reportLogger().setTraceLevel(TraceLevel::error, TraceLevel::info);
    
    // Initialize engine logger only for global strategy
    // For PER_ENGINE and PER_GAME strategies, log files are created dynamically
    if (config_.engineLogStrategy == LogFileStrategy::global) {
        engineLoggerGlobal().setLogFile(config_.engineLogBaseName);
        engineLoggerGlobal().setTraceLevel(TraceLevel::error, TraceLevel::info);
    }
}

Logger& Logger::engineLogger(const EngineLoggerId& loggerId) {
    const auto& config = getConfig();
    
    switch (config.engineLogStrategy) {
        case LogFileStrategy::global:
            return engineLoggerGlobal();
            
        case LogFileStrategy::perEngine:
            if (!loggerId.engineId.has_value()) {
                throw std::invalid_argument("engineId is required for perEngine strategy");
            }
            return engineLoggerPerEngine(loggerId);
            
        case LogFileStrategy::perGame:
            if (!loggerId.gameId.has_value()) {
                throw std::invalid_argument("gameId is required for perGame strategy");
            }
            return engineLoggerPerGame(loggerId);
            
        default:
            throw std::logic_error("Unknown LogFileStrategy");
    }
}

Logger& Logger::engineLoggerGlobal() {
    static Logger instance;
    static bool initialized = false;
    
    if (!initialized) {
        instance.setLogFile(getConfig().engineLogBaseName);
        instance.setTraceLevel(TraceLevel::error, TraceLevel::info);
        initialized = true;
    }
    
    return instance;
}

Logger& Logger::engineLoggerPerEngine(const EngineLoggerId& id) {
    const auto& config = getConfig();
    std::string loggerKey = id.engineId.value();
    std::string logBaseName = config.engineLogBaseName + "-" + loggerKey;
    
    // Thread-safe access to logger map
    std::scoped_lock lock(mapMutex_);
    
    // Check if logger already exists
    auto it = engineLoggers_.find(loggerKey);
    if (it != engineLoggers_.end()) {
        return *it->second;
    }
    
    // Create new logger instance
    auto logger = std::make_unique<Logger>();
    logger->id_ = id;  // Store identity
    logger->setLogFile(logBaseName);
    logger->setTraceLevel(TraceLevel::error, TraceLevel::info);
    
    auto& loggerRef = *logger;
    engineLoggers_[loggerKey] = std::move(logger);
    
    return loggerRef;
}

Logger& Logger::engineLoggerPerGame(const EngineLoggerId& id) {
    const auto& config = getConfig();
    std::string loggerKey = id.gameId.value();
    std::string logBaseName = config.engineLogBaseName + "-" + loggerKey;
    
    // Thread-safe access to logger map
    std::scoped_lock lock(mapMutex_);
    
    // Check if logger already exists
    auto it = engineLoggers_.find(loggerKey);
    if (it != engineLoggers_.end()) {
        return *it->second;
    }
    
    // Create new logger instance
    auto logger = std::make_unique<Logger>();
    logger->id_ = id;  // Store identity
    logger->setLogFile(logBaseName);
    logger->setTraceLevel(TraceLevel::error, TraceLevel::info);
    
    auto& loggerRef = *logger;
    engineLoggers_[loggerKey] = std::move(logger);
    
    return loggerRef;
}

void Logger::close(const EngineLoggerId& loggerId) {
    // Close file
    {
        std::scoped_lock fileLock(loggingMutex_);
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }
    
    // Determine logger key from stored identity
    const auto& config = getConfig();
    std::string loggerKey;
    
    switch (config.engineLogStrategy) {
        case LogFileStrategy::global:
            // Global logger doesn't remove itself from map
            return;
            
        case LogFileStrategy::perEngine:
            if (!id_.engineId.has_value()) {
                return;
            }
            loggerKey = id_.engineId.value();
            break;
            
        case LogFileStrategy::perGame:
            if (!id_.gameId.has_value()) {
                return;
            }
            loggerKey = id_.gameId.value();
            break;
            
        default:
            return;
    }
    
    // Remove from map (this may destroy this logger instance!)
    {
        std::scoped_lock mapLock(mapMutex_);
        engineLoggers_.erase(loggerKey);
    }
    // The ring buffer is always per engine while the logger may be per program run or game. 
    // Thus we passed the id of the engine to be able to remove the ring buffer for non-engine related loggers
    std::scoped_lock bufferLock(engineLogBufferMutex_);
    if (loggerId.engineId) {
        engineLogBuffers_.erase(*loggerId.engineId);
    }   
}

void Logger::clearEngineLoggers() {
    // This is called only if the logging strategy changes. So no impact on engineLogBuffers_.
    std::scoped_lock lock(mapMutex_);
    engineLoggers_.clear();
}

void Logger::withEngineLogBuffer(const std::string& engineId, 
                                   const std::function<void(const RingBuffer&)>& callback) {
    std::scoped_lock lock(engineLogBufferMutex_);
    auto it = engineLogBuffers_.find(engineId);
    if (it != engineLogBuffers_.end()) {
        callback(it->second);
    }
}

} // namespace QaplaTester
