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
#include <iomanip>
#include <iostream>
#include <sstream>


namespace QaplaTester {

// ============================================================================
// Logger (Report Logger) Implementation
// ============================================================================


void Logger::logAligned(std::string_view topic, std::string_view message, TraceLevel level) {
    std::ostringstream oss;
    oss << std::left << std::setw(30) << topic << message;
    log(oss.str(), level);
}

Logger& Logger::reportLogger() {
    // Never destroyed -- see BaseLogger::logPath_.
    static Logger* instance = new Logger();
    return *instance;
}


// ============================================================================
// EngineLogger Implementation
// ============================================================================

void EngineLogger::log(const std::string& engineId, std::string_view message, bool isOutput, 
    TraceLevel cliThreshold, TraceLevel fileThreshold, TraceLevel level) {

    std::scoped_lock lock(loggingMutex_);

    auto toLog = std::format("{} {} {}", engineId, isOutput ? "->" : "<-", message);
    
    // Add to ring buffer if this logger has an engineId
    if (!engineId.empty()) {
        std::scoped_lock bufferLock(engineLogBufferMutex_);
        engineLogBuffers_[engineId].push({
            .data = toLog, .timestamp = std::chrono::system_clock::now()
        });
    }
    
    if (level <= fileThreshold) {
        ensureFileOpen(logPath_);
        if (fileStream_.is_open()) {
            fileStream_ << toLog << "\n" << std::flush;
        }
    }
    
    if (level <= cliThreshold) {
        std::cout << toLog << "\n" << std::flush;
    }

}

EngineLogger& EngineLogger::engineLogger() {
    // Never destroyed -- see BaseLogger::logPath_.
    static EngineLogger* instance = new EngineLogger();
    return *instance;
}


EngineLogger& EngineLogger::engineLogger(const EngineLoggerId& loggerId) {
    
    switch (logStrategy_) {
        case LogFileStrategy::global:
            return engineLoggerGlobal();
            
        case LogFileStrategy::perEngine:
            if (!loggerId.engineId.has_value()) {
                throw std::invalid_argument("engineId is required for perEngine strategy");
            }
            return engineLoggerPerEngine(loggerId);
            
        default:
            throw std::logic_error("Unknown LogFileStrategy");
    }
}

EngineLogger& EngineLogger::engineLoggerGlobal() {
    // Never destroyed -- see BaseLogger::logPath_. This is the one the crash came through: an
    // engine worker logging its own "quit" after this object's derived part was gone.
    static EngineLogger* instance = [] {
        auto* logger = new EngineLogger();
        logger->setTraceLevel(TraceLevel::error, TraceLevel::info);
        return logger;
    }();

    return *instance;
}

EngineLogger& EngineLogger::engineLoggerPerEngine(const EngineLoggerId& id) {
    std::string loggerKey = id.engineId.value();
    
    // Thread-safe access to logger map
    std::scoped_lock lock(mapMutex_);
    
    // Check if logger already exists
    auto it = engineLoggers_.find(loggerKey);
    if (it != engineLoggers_.end()) {
        return *it->second;
    }
    
    // Create new logger instance
    auto logger = std::make_unique<EngineLogger>();
    logger->id_ = id;  // Store identity
    logger->setTraceLevel(TraceLevel::error, TraceLevel::info);
    
    auto& loggerRef = *logger;
    engineLoggers_[loggerKey] = std::move(logger);
    
    return loggerRef;
}

EngineLogger& EngineLogger::engineLoggerPerGame(const EngineLoggerId& id) {
    std::string loggerKey = id.gameId.value();
    
    // Thread-safe access to logger map
    std::scoped_lock lock(mapMutex_);
    
    // Check if logger already exists
    auto it = engineLoggers_.find(loggerKey);
    if (it != engineLoggers_.end()) {
        return *it->second;
    }
    
    // Create new logger instance
    auto logger = std::make_unique<EngineLogger>();
    logger->id_ = id;  // Store identity
    logger->setTraceLevel(TraceLevel::error, TraceLevel::info);
    
    auto& loggerRef = *logger;
    engineLoggers_[loggerKey] = std::move(logger);
    
    return loggerRef;
}

void EngineLogger::engineTerminated(const EngineLoggerId& loggerId) {

    // Buffer is always per engine (even in global strategy where multiple engines share one logger)
    std::scoped_lock bufferLock(engineLogBufferMutex_);
    if (loggerId.engineId) {
        engineLogBuffers_.erase(*loggerId.engineId);
    }  
    
    switch (logStrategy_) {
        case LogFileStrategy::global:
            // Strategy global means that we have one file for all engines but one buffer for each engine
            // Buffer is already removed, nothing else to do
            return;
            
        case LogFileStrategy::perEngine:
            // Per engine strategy means we have one logger instance per engine
            if (!id_.engineId.has_value()) {
                return;
            }
            {
                std::scoped_lock fileLock(loggingMutex_);
                if (fileStream_.is_open()) {
                    fileStream_.close();
                }
            }
            {
                auto loggerKey = id_.engineId.value();
                std::scoped_lock mapLock(mapMutex_);
                engineLoggers_.erase(loggerKey);
            }
            break;
            
        default:
            return;
    }
 
}

void EngineLogger::clearEngineLoggers() {
    // This is called only if the logging strategy changes. So no impact on engineLogBuffers_.
    std::scoped_lock lock(mapMutex_);
    engineLoggers_.clear();
}

void EngineLogger::withEngineLogBuffer(const std::string& engineId, 
                                   const std::function<void(const RingBuffer&)>& callback) {
    std::scoped_lock lock(engineLogBufferMutex_);
    auto it = engineLogBuffers_.find(engineId);
    if (it != engineLogBuffers_.end()) {
        callback(it->second);
    }
}

} // namespace QaplaTester
