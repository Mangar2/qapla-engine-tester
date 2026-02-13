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
#include "../base-elements/ini-file.h"
#include "../engine-handling/engine-capabilities.h"
#include <filesystem>
#include <string_view>
#include <optional>

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
     * @brief Lists available resources.
     * @param requestId The message ID.
     */
    static void listResources(const JsonValue& requestId);

    /**
     * @brief Reads a resource.
     * @param jsonObject The request object.
     */
    static void readResource(const JsonValue::Object& jsonObject);

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
     * @brief Sends a notification message to the client.
     * @param method The JSON-RPC method name.
     * @param params The parameters object.
     */
    static void sendNotification(const std::string& method, const JsonValue::Object& params);

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
     * @brief Maps JSON tool arguments to ConfigData using an underscore naming convention.
     * @param arguments The JSON arguments from a tool call.
     * @param defaultId Optional default ID to assign to all sections.
     * @return ConfigData object prepared for QaplaSettings.
     */
    [[nodiscard]] static QaplaHelpers::ConfigData mapJsonToConfigData(
        const JsonValue::Object& arguments, const std::string& defaultId = "");

    /**
     * @brief Executes a tool call.
     * @param jsonObject The request object.
     */
    static void callTool(const JsonValue::Object& jsonObject);

    /**
     * @brief Extracts the tool name from a log filename.
     * @param filename The filename to analyze.
     * @return The tool name or "other".
     */
    [[nodiscard]] static std::string extractToolName(std::string_view filename);

    /**
     * @brief Adds a resource description if the file is valid.
     * @param entry The directory entry to check.
     * @param resources The target resource array.
     */
    static void addResourceIfValid(const std::filesystem::directory_entry& entry, JsonValue::Array& resources);

    /**
     * @brief Tries to read a message based on Content-Length header.
     * @param line The current header line.
     * @return Optional JSON value if header was found and content read.
     */
    [[nodiscard]] static std::optional<JsonValue> tryReadByContentLength(const std::string& line);

    /**
     * @brief Tries to read a message by counting braces in accumulated buffer.
     * @param accumulated The buffered input.
     * @return Optional JSON value if a complete object/array was formed.
     */
    [[nodiscard]] static std::optional<JsonValue> tryReadByBraceCounting(std::string& accumulated);

    /**
     * @brief Handles the control tool.
     * @param arguments The tool arguments.
     * @return Result content array.
     */
    static JsonValue::Array handleControlTool(const JsonValue::Object& arguments);

    /**
     * @brief Handles the read_report tool.
     * @param arguments The tool arguments.
     * @return Result content array.
     */
    static JsonValue::Array handleReadReport(const JsonValue::Object& arguments);

    /**
     * @brief Executes a tool that uses the AppRunner dispatcher.
     * @param configData The prepared configuration data.
     * @param background If true, execution continues in background.
     * @return Tool return code.
     */
    static AppReturnCode executeRunnerTool(QaplaHelpers::ConfigData& configData, bool background = false);

    /**
     * @brief Formats the summary text for a tool execution.
     * @param name The tool name.
     * @param code The return code.
     * @return Execution summary string.
     */
    [[nodiscard]] static std::string formatRunSummary(const std::string& name, AppReturnCode code);

    /**
     * @brief Processes a single tool parameter and routes it to the correct section.
     * @param key The parameter key.
     * @param value The parameter value.
     * @param otherGroupedSections Map of other section names to sections.
     * @param configData The target config data for global parameters.
     */
    static void processParameter(const std::string& key, const JsonValue& value,
        std::unordered_map<std::string, QaplaHelpers::IniFile::Section>& otherGroupedSections,
        QaplaHelpers::ConfigData& configData);

    /**
     * @brief Helpers for callTool to reduce complexity.
     */
    static JsonValue::Array runRunnerTool(const std::string& name, JsonValue::Object& arguments, AppReturnCode& returnCode);

    static void mergeGlobalConfig(QaplaHelpers::ConfigData& target, const QaplaHelpers::ConfigData& source);
    static std::pair<std::string, std::string> getTaskConfigInfo(const std::string& name);
    static void prepareTaskFile(const std::string& name, JsonValue::Object& toolArgs);

    /**
     * @brief Handles the adjudication tool.
     * @param arguments The tool arguments.
     * @return Result content array.
     */
    static JsonValue::Array handleAdjudicateTool(const JsonValue::Object& arguments);

    inline static QaplaConfiguration::EngineCapabilities capabilities_;
    inline static QaplaHelpers::ConfigData globalAdjudicationConfig_;
    static inline std::string lastSprtFile_;
    static inline std::string lastTournamentFile_;
};

} // namespace QaplaTester::Mcp
