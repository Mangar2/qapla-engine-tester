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

#include "mcp-message-channel.h"

#include "../base-elements/string-helper.h"

#include <cstdint>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace QaplaTester::Mcp {

namespace {

std::mutex cliOutputMutex;

void configureCliMcpBinaryStdio() {
#ifdef _WIN32
    // Why: In text mode Windows rewrites '\n' to "\r\n". MCP framing requires exact bytes
    // for "Content-Length" headers and "\r\n\r\n" separators, otherwise frames can become
    // "\r\r\n" and clients fail to parse or split messages.
    static bool binaryConfigured = false;
    if (binaryConfigured) {
        return;
    }
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    binaryConfigured = true;
#endif
}

}

McpMessageChannel::McpMessageChannel(McpMessageChannelType type)
    : type_(type) {
    if (type_ == McpMessageChannelType::cli) {
        configureCliMcpBinaryStdio();
    }
}

void McpMessageChannel::setType(McpMessageChannelType type) {
    type_ = type;
}

McpMessageChannelType McpMessageChannel::getType() const {
    return type_;
}

std::optional<JsonValue> McpMessageChannel::readMessage() {
    if (type_ == McpMessageChannelType::cli) {
        return readCliMessage();
    }

    throw createUnsupportedTypeError(type_);
}

void McpMessageChannel::sendMessage(const JsonValue& message) const {
    if (type_ == McpMessageChannelType::cli) {
        sendCliMessage(message);
        return;
    }

    throw createUnsupportedTypeError(type_);
}

std::optional<JsonValue> McpMessageChannel::readCliMessage() {
    if (auto value = tryReadByBraceCounting(accumulated_); value.has_value()) {
        return value;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (const auto value = tryReadByContentLength(line); value.has_value()) {
            return value;
        }

        if (line.empty() && accumulated_.empty()) {
            continue;
        }

        accumulated_ += line;

        if (const auto value = tryReadByBraceCounting(accumulated_); value.has_value()) {
            return value;
        }
    }

    return std::nullopt;
}

std::optional<JsonValue> McpMessageChannel::tryReadByContentLength(const std::string& line) {
    const auto headerLine = QaplaHelpers::trim(line);
    if (!headerLine.starts_with("Content-Length:")) {
        return std::nullopt;
    }

    const auto lengthOpt = QaplaHelpers::to_unsigned_int<uint64_t>(QaplaHelpers::trim(headerLine.substr(15)));
    if (!lengthOpt.has_value()) {
        return std::nullopt;
    }

    const auto length = static_cast<size_t>(*lengthOpt);
    std::string emptyLine;
    std::getline(std::cin, emptyLine);

    std::string content(length, '\0');
    std::cin.read(content.data(), static_cast<std::streamsize>(length));
    if (std::cin.gcount() == static_cast<std::streamsize>(length)) {
        std::string_view contentView = content;
        return JsonHelper::parse(contentView);
    }

    return std::nullopt;
}

std::optional<JsonValue> McpMessageChannel::tryReadByBraceCounting(std::string& accumulated) {
    size_t openBraces = 0;
    size_t closeBraces = 0;
    bool inString = false;
    bool escaped = false;
    size_t currentPos = 0;

    for (const char character : accumulated) {
        currentPos++;
        if (character == '"' && !escaped) {
            inString = !inString;
        } else if (!inString) {
            if (character == '{' || character == '[') {
                openBraces++;
            } else if (character == '}' || character == ']') {
                closeBraces++;
            }
        }
        escaped = (character == '\\' && !escaped);

        if (openBraces > 0 && openBraces == closeBraces) {
            std::string_view jsonInputView(accumulated.data(), currentPos);
            try {
                auto result = JsonHelper::parse(jsonInputView);
                accumulated.erase(0, currentPos);
                return result;
            } catch (...) { // NOLINT(bugprone-empty-catch)
            }
        }
    }

    if (openBraces == 0 && !accumulated.empty()) {
        std::string_view jsonInputView = accumulated;
        try {
            auto result = JsonHelper::parse(jsonInputView);
            accumulated.clear();
            return result;
        } catch (...) { // NOLINT(bugprone-empty-catch)
        }
    }

    return std::nullopt;
}

void McpMessageChannel::sendCliMessage(const JsonValue& message) {
    std::lock_guard lock(cliOutputMutex);
    std::cout << JsonHelper::serialize(message) << "\n" << std::flush;
}

std::runtime_error McpMessageChannel::createUnsupportedTypeError(McpMessageChannelType type) {
    if (type == McpMessageChannelType::net) {
        return std::runtime_error("MCP net transport is not implemented yet.");
    }
    return std::runtime_error("MCP message channel type is not supported.");
}

} // namespace QaplaTester::Mcp
