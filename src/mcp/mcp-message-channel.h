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

#include "../base-elements/json-helper.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace QaplaTester::Mcp {

/**
 * @brief Defines the transport mode used by the MCP message channel.
 */
enum class McpMessageChannelType {
    /**
     * @brief Uses standard input/output based communication.
     */
    cli,

    /**
     * @brief Uses network/socket based communication.
     */
    net
};

/**
 * @brief Encapsulates MCP message input/output for different transport modes.
 */
class McpMessageChannel {
public:
    /**
     * @brief Creates a message channel with the selected transport type.
     * @param type Initial channel transport mode.
     */
    explicit McpMessageChannel(McpMessageChannelType type = McpMessageChannelType::cli);

    /**
     * @brief Updates the transport type used for subsequent communication.
     * @param type New channel transport mode.
     */
    void setType(McpMessageChannelType type);

    /**
     * @brief Returns the currently active transport type.
     * @return Current channel transport mode.
     */
    [[nodiscard]] McpMessageChannelType getType() const;

    /**
     * @brief Reads one MCP message from the configured transport.
     * @return Parsed JSON message, or empty optional on end of input.
     */
    [[nodiscard]] std::optional<JsonValue> readMessage();

    /**
     * @brief Sends one MCP message through the configured transport.
     * @param message JSON payload to transmit.
     */
    void sendMessage(const JsonValue& message) const;

private:
    /**
     * @brief Reads one message from CLI input.
     * @return Parsed JSON message, or empty optional on end of input.
     */
    [[nodiscard]] std::optional<JsonValue> readCliMessage();

    /**
     * @brief Tries to read a message body via Content-Length framing.
     * @param line Current input line that may contain a Content-Length header.
     * @return Parsed JSON message when framing is complete, otherwise empty optional.
     */
    [[nodiscard]] static std::optional<JsonValue> tryReadByContentLength(const std::string& line);

    /**
     * @brief Tries to parse a complete JSON message from an accumulated buffer.
     * @param accumulated Buffer with pending input data.
     * @return Parsed JSON message when complete JSON is available, otherwise empty optional.
     */
    [[nodiscard]] static std::optional<JsonValue> tryReadByBraceCounting(std::string& accumulated);

    /**
     * @brief Sends one message over CLI output.
     * @param message JSON payload to transmit.
     */
    static void sendCliMessage(const JsonValue& message);

    /**
     * @brief Creates an exception for transport types that are not implemented.
     * @param type Transport mode that triggered the unsupported operation.
     * @return Runtime error describing the unsupported channel type.
     */
    [[nodiscard]] static std::runtime_error createUnsupportedTypeError(McpMessageChannelType type);

    std::string accumulated_;
    McpMessageChannelType type_;
};

} // namespace QaplaTester::Mcp
