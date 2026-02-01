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

#include "json-helper.h"
#include "../base-elements/app-error.h"

namespace QaplaTester::Mcp {

/**
 * @brief Controller for Model Context Protocol (MCP) server operations.
 */
class McpServer {
public:
    /**
     * @brief Initializes the MCP server mode.
     */
    static void initialize();

    /**
     * @brief Runs the MCP server loop.
     * @return Application return code.
     */
    [[nodiscard]] static AppReturnCode run();

private:
    /**
     * @brief Silences loggers to prevent CLI output during MCP sessions.
     */
    static void silenceLoggers();

    /**
     * @brief Reads a single JSON-RPC message from stdin.
     * @return Optional JSON value, nullopt on EOF.
     */
    [[nodiscard]] static std::optional<JsonValue> readMessage();

    /**
     * @brief Sends a JSON-RPC message to stdout.
     * @param message The message to send.
     */
    static void sendMessage(const JsonValue& message);

    /**
     * @brief Processes a single JSON-RPC request/notification.
     * @param jsonObject The parsed JSON-RPC object.
     * @return Application return code if the server should exit, NoError otherwise.
     */
    static AppReturnCode processMessage(const JsonValue::Object& jsonObject);

    /**
     * @brief Sends the tools/list result.
     * @param requestId The ID of the request.
     */
    static void listTools(const JsonValue& requestId);

    /**
     * @brief Executes a tool call.
     * @param jsonObject The request object.
     */
    static void callTool(const JsonValue::Object& jsonObject);
};

} // namespace QaplaTester::Mcp
