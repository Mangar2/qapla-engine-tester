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
#include "file-helper.h"
#include "json-helper.h"

#include <iostream>

namespace {

[[nodiscard]] std::string toMcpTextPayload(std::string_view message) {
    QaplaTester::Mcp::JsonValue::Object payloadObject;
    payloadObject["type"] = QaplaTester::Mcp::JsonHelper::makeString("text");
    payloadObject["message"] = QaplaTester::Mcp::JsonHelper::makeString(message);
    const auto payload = QaplaTester::Mcp::JsonHelper::makeObject(std::move(payloadObject));
    return QaplaTester::Mcp::JsonHelper::serialize(payload);
}

[[nodiscard]] std::string toMcpStatusPayload(std::string_view message, std::string_view toolName, bool overwrite) {
    QaplaTester::Mcp::JsonValue::Object payloadObject;
    payloadObject["type"] = QaplaTester::Mcp::JsonHelper::makeString("status");
    payloadObject["message"] = QaplaTester::Mcp::JsonHelper::makeString(message);
    payloadObject["tool"] = QaplaTester::Mcp::JsonHelper::makeString(toolName);
    payloadObject["overwrite"] = QaplaTester::Mcp::JsonHelper::makeBool(overwrite);
    const auto payload = QaplaTester::Mcp::JsonHelper::makeObject(std::move(payloadObject));
    return QaplaTester::Mcp::JsonHelper::serialize(payload);
}

} // namespace

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

    filename_ = QaplaHelpers::generateTimestampedFilename(basename, logPath, "log");
    openedBasename_ = basename;
    openedLogPath_ = logPath;
    fileStream_.open(filename_, std::ios::app);

    if (fileStream_.is_open()) {
        onFileOpened();
    }
}

std::string BaseLogger::getFilename() {
    std::scoped_lock lock(loggingMutex_);
    ensureFileOpen(logPath_);
    return filename_;
}

void BaseLogger::startNewRunLogFile() {
    std::scoped_lock lock(loggingMutex_);
    if (fileStream_.is_open()) {
        fileStream_.close();
    }

    filename_.clear();
    openedBasename_.clear();
    openedLogPath_.clear();
}

void BaseLogger::logCli(std::string_view message, TraceLevel level) {
    std::scoped_lock lock(loggingMutex_);
    if (level <= cliThreshold_) {
        std::cout << message << "\n" << std::flush;
    }
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
        const auto mcpPayload = toMcpTextPayload(message);
        mcpCallback_(mcpPayload, "");
    }

    if (level <= cliThreshold_) {
        std::cout << message << "\n" << std::flush;
    }
}

void BaseLogger::logTable(std::string_view tableName, const Table& table, TraceLevel level) {
    std::scoped_lock lock(loggingMutex_);

    std::string tableText;
    if (level <= fileThreshold_ || level <= cliThreshold_) {
        tableText = TableFormat::toText(table);
    }

    if (level <= fileThreshold_) {
        ensureFileOpen(logPath_);
        if (fileStream_.is_open()) {
            fileStream_ << tableText << "\n" << std::flush;
        }
    }

    if (level <= mcpThreshold_ && mcpCallback_) {
        const auto payload = TableFormat::toJson(tableName, table);
        mcpCallback_(payload, "");
    }

    if (level <= cliThreshold_) {
        std::cout << tableText << "\n" << std::flush;
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
        const auto mcpPayload = toMcpStatusPayload(message, toolName, overwrite);
        mcpCallback_(mcpPayload, toolName);
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
