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

#include "mcp-server.h"
#include "mcp-schema-builder.h"
#include "mcp-engine-tool.h"
#include "json-helper.h"
#include "mcp-converter.h"
#include "settings-reporter.h"

#include "../base-elements/file-helper.h"
#include "../sprt/sprt-tournament-file.h"
#include "../tournament/tournament-file.h"
#include "../base-elements/base-logger.h"
#include "../cli/settings-manager.h"
#include "../cli/qapla-settings.h"
#include "../cli/app-runner.h"
#include "../cli/task-types.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../game-manager/game-manager-pool.h"

#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace QaplaTester::Mcp {

void McpServer::initialize() {
    AppError::setDefaultInvalidParameterUserHint("Please refer to the tool definition schema for supported parameters.");
    silenceLoggers();

    // Set up MCP logging callback for report logger
    Logger::reportLogger().setMcpCallback([](std::string_view message, std::string_view toolName) {
        JsonValue::Object params;
        params["level"] = JsonValue{ .data = std::string("info") };
        params["logger"] = JsonValue{ .data = std::string(toolName.empty() ? "qapla" : toolName) };
        
        // Try to parse message as JSON if it looks like it
        std::string_view msgTrimmed = message;
        while (!msgTrimmed.empty() && std::isspace(msgTrimmed.front())) {
            msgTrimmed.remove_prefix(1);
        }
        
        if (!msgTrimmed.empty() && (msgTrimmed.front() == '{' || msgTrimmed.front() == '[')) {
            try {
                params["data"] = JsonHelper::parse(msgTrimmed);
            } catch (...) {
                // Ignore parse errors, treat as plain text
                params["data"] = JsonValue{ .data = std::string(message) };
            }
        } else {
            params["data"] = JsonValue{ .data = std::string(message) };
        }
        
        sendNotification("notifications/message", params);
    });

    // Set up MCP notification callback for engine autodetection
    QaplaConfiguration::EngineCapabilities::setNotificationCallback(
        [](const std::string& message, [[maybe_unused]] const std::string& type) {
        JsonValue::Object params;
        params["level"] = JsonValue{ .data = std::string("info") };
        params["logger"] = JsonValue{ .data = std::string("autodetect") };
        params["data"] = JsonValue{ .data = message };
        sendNotification("notifications/message", params);
    });

    // Default MCP trace level to result
    Logger::reportLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::result);
}

AppReturnCode McpServer::run() {
    capabilities_.autoDetect();
    
    while (true) {
        const auto message = readMessage();
        if (!message.has_value()) {
            break; // EOF or error
        }

        if (!message->isObject()) {
            continue;
        }

        const auto& jsonObject = message->asObject();
        if (processMessage(jsonObject) != AppReturnCode::NoError) {
            break;
        }
    }
    capabilities_.shutdown();
    return AppReturnCode::NoError;
}

AppReturnCode McpServer::processMessage(const JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("method")) {
        return AppReturnCode::NoError;
    }

    const std::string& method = jsonObject.at("method").asString();

    if (method == "initialize") {
        // Respond to handshake
        JsonValue response;
        JsonValue::Object responseBody;
        responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
        if (jsonObject.contains("id")) {
            responseBody["id"] = jsonObject.at("id");
        }

        JsonValue::Object resultData;
        resultData["protocolVersion"] = JsonValue{ .data = std::string("2024-11-05") };

        JsonValue::Object capabilities;
        capabilities["tools"] = JsonValue{ .data = JsonValue::Object() }; 
        capabilities["resources"] = JsonValue{ .data = JsonValue::Object() };
        resultData["capabilities"] = JsonValue{ .data = capabilities };

        JsonValue::Object serverInformation;
        serverInformation["name"] = JsonValue{ .data = std::string("Qapla Engine Tester") };
        serverInformation["version"] = JsonValue{ .data = std::string("0.5.0") };
        resultData["serverInfo"] = JsonValue{ .data = serverInformation };

        responseBody["result"] = JsonValue{ .data = resultData };
        response.data = responseBody;

        sendMessage(response);
    }
    else if (method == "tools/list") {
        if (jsonObject.contains("id")) {
            listTools(jsonObject.at("id"));
        }
    }
    else if (method == "tools/call") {
        callTool(jsonObject);
    }
    else if (method == "resources/list") {
        if (jsonObject.contains("id")) {
            listResources(jsonObject.at("id"));
        }
    }
    else if (method == "resources/read") {
        readResource(jsonObject);
    }
    else if (method == "notifications/initialized") {
        // Client confirmed initialization
    }

    return AppReturnCode::NoError;
}

void McpServer::sendMessage(const JsonValue& message) {
    std::cout << JsonHelper::serialize(message) << std::endl;
}

void McpServer::sendNotification(const std::string& method, const JsonValue::Object& params) {
    JsonValue notification;
    JsonValue::Object body;
    body["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    body["method"] = JsonValue{ .data = method };
    body["params"] = JsonValue{ .data = params };
    notification.data = body;
    sendMessage(notification);
}

std::optional<JsonValue> McpServer::readMessage() {
    static std::string accumulated;
    
    // Check if we already have a full message in accumulated (from previous over-read)
    if (auto val = tryReadByBraceCounting(accumulated)) {
        return val;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (const auto val = tryReadByContentLength(line)) {
            return val;
        }

        if (line.empty() && accumulated.empty()) {
            continue;
        }

        accumulated += line;
        
        if (const auto val = tryReadByBraceCounting(accumulated)) {
            return val;
        }
    }

    return std::nullopt;
}

void McpServer::listTools(const JsonValue& requestId) {
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    responseBody["id"] = requestId;

    JsonValue::Object resultData;
    JsonValue::Array tools;

    const auto toolsToRegister = std::vector<McpSchemaBuilder::ToolInfo>{
        {
            .name = "test",
            .description = "Runs basic engine tests (startup, move generation, etc.)",
            .groups = {"test", "logging"}
        },
        {
            .name = "sprt",
            .description = "Runs a Sequential Probability Ratio Test (SPRT) between engines",
            .groups = {"sprt", "openings", "pgnoutput", "logging"}
        },
        {
            .name = "tournament",
            .description = "Runs a tournament between engines",
            .groups = {"tournament", "openings", "pgnoutput", "logging"}
        },
        {
            .name = "epd",
            .description = "Runs an EPD testset against engines",
            .groups = {"epd", "pgnoutput", "logging"}
        },
        {
            .name = "spsa",
            .description = "Optimizes engine parameters using SPSA",
            .groups = {"spsa", "spsavalue", "openings", "pgnoutput", "logging"}
        },
        {
            .name = "adjudicate",
            .description = "Configures global adjudication settings (Draw and Resign) for all tournaments.",
            .groups = {"draw", "resign"}
        },
        {
            .name = "control",
            .description = "Control the execution of running tasks (concurrency, stop)",
            .groups = {}
        },
        {
            .name = "manage_engines",
            .description = "Manage engine configurations in the registry. Use this to manage the "
                        "engine registry. Supports individual management (list, details, add, "
                        "copy, update, delete) and bulk operations (update_all) for global settings "
                        "like time control or UCI options.",
            .groups = {}
        },
        {
            .name = "list_settings",
            .description = "Lists all current settings (globals and task groups like sprt, tournament, etc.) excluding engines. "
                        "Reports default values, mandatory status, and whether a setting is missing.",
            .groups = {}
        },
        {
            .name = "read_report",
            .description = "Reads the content of a specific report log or PGN file",
            .groups = {}
        }
    };

    const auto& engineConfigs = EngineWorkerFactory::getConfigManager().getAllConfigs();
    std::string registeredNames;
    if (!engineConfigs.empty()) {
        for (const auto& config : engineConfigs) {
            if (!registeredNames.empty()) {
                registeredNames += ", ";
            }
            registeredNames += std::format("'{}'", config.getName());
        }
    }

    const auto& groupDefs = Settings::Manager::instance().getGroupDefinitions();

    for (const auto& info : toolsToRegister) {
        JsonValue::Object tool;
        tool["name"] = JsonValue{ .data = std::string(info.name) };
        
        // Use longDescription from main group if available
        std::string description(info.description);
        if (const auto it = groupDefs.find(std::string(info.name)); it != groupDefs.end() && !it->second.longDescription.empty()) {
            description = it->second.longDescription;
        }
        tool["description"] = JsonValue{ .data = description };
        tool["inputSchema"] = JsonValue{ .data = McpSchemaBuilder::createInputSchema(info, registeredNames) };
        
        tools.push_back(JsonValue{ .data = tool });
    }

    resultData["tools"] = JsonValue{ .data = tools };
    responseBody["result"] = JsonValue{ .data = resultData };
    response.data = responseBody;

    sendMessage(response);
}

void McpServer::callTool(const JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("params") || !jsonObject.at("params").isObject()) {
        return;
    }
    const auto& params = jsonObject.at("params").asObject();
    if (!params.contains("name")) {
        return;
    }

    const std::string& name = params.at("name").asString();
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    if (jsonObject.contains("id")) {
        responseBody["id"] = jsonObject.at("id");
    }

    JsonValue::Object result;
    JsonValue::Array content;
    AppReturnCode returnCode = AppReturnCode::NoError;

    try {
        const auto& arguments = params.at("arguments").asObject();

        if (name == "read_report") {
            content = handleReadReport(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else if (name == "control") {
            content = handleControlTool(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else if (name == "manage_engines") {
            content = McpEngineTool::handleManageEngines(arguments, capabilities_);
            result["isError"] = JsonValue{ .data = false };
        } else if (name == "adjudicate") {
            content = handleAdjudicateTool(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else if (name == "list_settings") {
            content = handleListSettings(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else {
            // Handle active list and execution
            JsonValue::Object toolArgs = arguments;
            const Cli::TaskType taskType = Cli::TaskType::All;

            McpEngineTool::setupActiveEngines(toolArgs, taskType, capabilities_);
            content = runRunnerTool(name, toolArgs, returnCode);
            
            result["isError"] = JsonValue{ .data = (returnCode == AppReturnCode::GeneralError || 
                                                  returnCode == AppReturnCode::InvalidParameters || 
                                                  returnCode == AppReturnCode::EngineError || 
                                                  returnCode == AppReturnCode::EngineMissbehaviour) };
        }
    } catch (const std::exception& e) {
        JsonValue::Object errorContent;
        errorContent["type"] = JsonValue{ .data = std::string("text") };
        errorContent["text"] = JsonValue{ .data = std::format("Error executing tool '{}': {}", name, e.what()) };
        content.push_back(JsonValue{ .data = errorContent });
        result["isError"] = JsonValue{ .data = true };
        returnCode = AppReturnCode::GeneralError;
    }

    result["content"] = JsonValue{ .data = content };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
}

QaplaHelpers::ConfigData McpServer::mapJsonToConfigData(
    const JsonValue::Object& arguments, const std::string& defaultId) {
    QaplaHelpers::ConfigData configData;
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section> otherGroupedSections;

    // Parameters
    for (const auto& [key, value] : arguments) {
        processParameter(key, value, otherGroupedSections, configData);
    }

    // Add sections to configData
    for (auto& [name, s] : otherGroupedSections) {
        // Ensure every configuration group has its ID set to ensure persistence
        bool hasId = false;
        for (const auto& [key, val] : s.entries) {
            if (key == "id") {
                hasId = true;
                break;
            }
        }

        if (!hasId) {
            s.addEntry("id", defaultId.empty() ? name : defaultId);
        }
        
        configData.addSection(s);
    }

    return configData;
}

void McpServer::processParameter(const std::string& key, const JsonValue& value,
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section>& otherGroupedSections,
    QaplaHelpers::ConfigData& configData) {

    if (value.isArray()) {
        for (const auto& item : value.asArray()) {
            if (item.isObject()) {
                QaplaHelpers::IniFile::Section s;
                s.name = key;
                for (const auto& [propKey, propVal] : item.asObject()) {
                    s.addEntry(propKey, validateAndToString(propKey, propVal));
                }
                configData.addSection(s);
            }
        }
        return;
    }

    const std::string valStr = validateAndToString(key, value);

    if (const size_t underscorePos = key.find('_'); underscorePos != std::string::npos) {
        const std::string sectionName = key.substr(0, underscorePos);
        std::string paramKey = key.substr(underscorePos + 1);

        if (paramKey.starts_with("option_")) {
            paramKey = "option." + paramKey.substr(7);
        }

        otherGroupedSections[sectionName].name = sectionName;
        otherGroupedSections[sectionName].addEntry(paramKey, valStr);
    } else {
        configData.addGlobalParameter(key, valStr);
    }
}

void McpServer::silenceLoggers() {
    QaplaHelpers::ConfigData mcpConfig;
    QaplaHelpers::IniFile::Section loggingSection;
    loggingSection.name = "logging";
    loggingSection.addEntry("trace", "none");
    loggingSection.addEntry("mcp", "result");
    loggingSection.addEntry("engine", "false");
    mcpConfig.addSection(loggingSection);

    Logger::reportLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::result);
    EngineLogger::engineLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::error);

    Settings::Manager::instance().parseInput(mcpConfig, true);
}

void McpServer::listResources(const JsonValue& requestId) {
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    responseBody["id"] = requestId;

    JsonValue::Object resultData;
    JsonValue::Array resources;

    std::error_code ec;
    if (!BaseLogger::logPath_.empty() && std::filesystem::exists(BaseLogger::logPath_, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(BaseLogger::logPath_, ec)) {
            if (ec) {
                break;
            }
            addResourceIfValid(entry, resources);
        }
    }

    resultData["resources"] = JsonValue{ .data = resources };
    responseBody["result"] = JsonValue{ .data = resultData };
    response.data = responseBody;

    sendMessage(response);
}

void McpServer::addResourceIfValid(const std::filesystem::directory_entry& entry, JsonValue::Array& resources) {
    if (!entry.is_regular_file()) {
        return;
    }

    const auto filename = entry.path().filename().string();
    const auto extension = entry.path().extension().string();

    if (extension != ".log" && extension != ".pgn") {
        return;
    }

    const std::string tool = extractToolName(filename);
    const bool isPgn = (extension == ".pgn");

    JsonValue::Object resource;
    resource["uri"] = JsonValue{ .data = std::format("qapla://reports/{}/{}", tool, filename) };
    resource["name"] = JsonValue{ .data = filename };
    resource["description"] = JsonValue{ .data = std::format("{} result for tool {}", isPgn ? "PGN" : "Log", tool) };
    resource["mimeType"] = JsonValue{ .data = (isPgn ? std::string("text/x-chess-pgn") : std::string("text/plain")) };
    resources.push_back(JsonValue{ .data = resource });
}

JsonValue::Array McpServer::handleListSettings(const JsonValue::Object& arguments) {
    std::optional<std::vector<std::string>> groups;

    if (arguments.contains("groups") && arguments.at("groups").isArray()) {
        const auto& groupArray = arguments.at("groups").asArray();
        std::vector<std::string> groupList;
        for (const auto& g : groupArray) {
            if (g.isString()) {
                groupList.push_back(g.asString());
            }
        }
        groups = groupList;
    }

    // Currently exposing limited columns parametrization or just default if not specified
    // But requirement says: "welche werte je parameter angezeigt werden"
    // So let's check for columns argument
    std::optional<std::vector<SettingsReporter::Column>> columns;
    // Parsing columns from string names would be needed here if we expose it fully to JSON
    // For now assuming default columns for list_settings unless specialized logic is added.
    
    std::string report = SettingsReporter::generateReport(groups, columns);

    JsonValue::Array content;
    JsonValue::Object contentObj;
    contentObj["type"] = JsonValue{ .data = std::string("text") };
    contentObj["text"] = JsonValue{ .data = report };
    content.push_back(JsonValue{ .data = contentObj });
    return content;
}

std::string McpServer::extractToolName(std::string_view filename) {
    constexpr std::array prefixes = { 
        std::string_view("report-"), 
        std::string_view("engine-"),
        std::string_view("sprt-"),
        std::string_view("tournament-"),
        std::string_view("epd-"),
        std::string_view("spsa-")
    };

    for (const auto& prefix : prefixes) {
        if (!filename.starts_with(prefix)) {
            continue;
        }

        if (prefix == "report-") {
            const auto substring = filename.substr(prefix.length());
            const size_t dash = substring.find('-');

            // If a dash exists and it's not starting with a digit, use it as tool name
            if (dash != std::string_view::npos && (substring.length() > 0 && isdigit(static_cast<unsigned char>(substring[0])) == 0)) {
                return std::string(substring.substr(0, dash));
            }
        }

        return std::string(prefix.substr(0, prefix.length() - 1));
    }

    return "other";
}

void McpServer::readResource(const JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("params") || !jsonObject.at("params").isObject()) {
        return;
    }
    const auto& params = jsonObject.at("params").asObject();
    if (!params.contains("uri")) {
        return;
    }

    const std::string& uri = params.at("uri").asString();
    
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    if (jsonObject.contains("id")) {
        responseBody["id"] = jsonObject.at("id");
    }

    JsonValue::Object result;
    JsonValue::Array contents;

    // Handle qapla://reports/<tool>/<filename>
    if (uri.starts_with("qapla://reports/")) {
        size_t lastSlash = uri.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string filename = uri.substr(lastSlash + 1);
            std::filesystem::path path = std::filesystem::path(BaseLogger::logPath_) / filename;
            
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                std::ifstream file(path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    
                    JsonValue::Object content;
                    content["uri"] = JsonValue{ .data = uri };
                    content["text"] = JsonValue{ .data = buffer.str() };
                    contents.push_back(JsonValue{ .data = content });
                }
            }
        }
    }

    result["contents"] = JsonValue{ .data = contents };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
}

std::optional<JsonValue> McpServer::tryReadByContentLength(const std::string& line) {
    if (!line.starts_with("Content-Length:")) {
        return std::nullopt;
    }

    const auto lengthOpt = QaplaHelpers::to_unsigned_int<uint64_t>(line.substr(15));
    if (!lengthOpt) {
        return std::nullopt;
    }

    const auto length = static_cast<size_t>(*lengthOpt);
    std::string empty;
    std::getline(std::cin, empty); // usually an empty line follows headers
    
    std::string content(length, '\0');
    if (std::cin.read(content.data(), static_cast<std::streamsize>(length))) {
        std::string_view contentView = content;
        return JsonHelper::parse(contentView);
    }

    return std::nullopt;
}

std::optional<JsonValue> McpServer::tryReadByBraceCounting(std::string& accumulated) {
    size_t openBraces = 0;
    size_t closeBraces = 0;
    bool inString = false;
    bool escaped = false;
    size_t pos = 0;

    for (const char character : accumulated) {
        pos++;
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
            std::string_view jsonInputView(accumulated.data(), pos);
            try {
                auto result = JsonHelper::parse(jsonInputView);
                accumulated.erase(0, pos);
                return result;
            } catch (...) { // NOLINT(bugprone-empty-catch)
                // If it fails, maybe it wasn't a complete JSON yet after all (e.g. malformed)
                // continue searching
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
            // Ignore parse errors at the end of stream
        }
    }

    return std::nullopt;
}

JsonValue::Array McpServer::handleReadReport(const JsonValue::Object& arguments) {
    if (!arguments.contains("uri")) {
        throw std::runtime_error("Could not read report file: URI missing.");
    }

    const std::string& uri = arguments.at("uri").asString();
    std::string filename;
    if (uri.starts_with("qapla://reports/")) {
        size_t lastSlash = uri.find_last_of('/');
        if (lastSlash != std::string::npos) {
            filename = uri.substr(lastSlash + 1);
        }
    } else {
        filename = uri;
    }

    if (!filename.empty()) {
        std::filesystem::path path = std::filesystem::path(BaseLogger::logPath_) / filename;
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                
                JsonValue::Object textContent;
                textContent["type"] = JsonValue{ .data = std::string("text") };
                textContent["text"] = JsonValue{ .data = buffer.str() };
                
                JsonValue::Array content;
                content.push_back(JsonValue{ .data = textContent });
                return content;
            }
        }
    }
    throw std::runtime_error("Could not read report file: file not found.");
}

AppReturnCode McpServer::executeRunnerTool(QaplaHelpers::ConfigData& configData, bool background) {

    Settings::QaplaSettings::instance().applyConfig(configData);

    // Run dispatcher
    return AppRunner::instance().runDispatcher(background);
}

std::string McpServer::formatRunSummary(const std::string& name, AppReturnCode code) {
    std::string summary = std::format("Tool '{}' finished. Result: ", name);
    switch (code) {
        case AppReturnCode::NoError: summary += "Success"; break;
        case AppReturnCode::GeneralError: summary += "General Error"; break;
        case AppReturnCode::InvalidParameters: summary += "Invalid Parameters"; break;
        case AppReturnCode::EngineError: summary += "Engine Error (crash or illegal moves)"; break;
        case AppReturnCode::EngineMissbehaviour: summary += "Engine Misbehavior (hang or protocol violation)"; break;
        case AppReturnCode::EngineNote: summary += "Completed with engine notes"; break;
        case AppReturnCode::MissedTarget: summary += "EPD target threshold not reached"; break;
        case AppReturnCode::H1Accepted: summary += "SPRT H1 accepted (stronger engine)"; break;
        case AppReturnCode::H0Accepted: summary += "SPRT H0 accepted (no significant difference)"; break;
        case AppReturnCode::UndefinedResult: summary += "SPRT result undecided"; break;
        default: summary += std::to_string(static_cast<int>(code)); break;
    }

    const std::string reportFilename = std::filesystem::path(Logger::reportLogger().getFilename()).filename().string();
    if (!reportFilename.empty()) {
        summary += std::format("\nReport Log Resource: qapla://reports/{}/{}", name, reportFilename);
    }
    return summary;
}

JsonValue::Array McpServer::handleAdjudicateTool(const JsonValue::Object& arguments) {
    JsonValue::Array content;
    JsonValue::Object textContent;
    textContent["type"] = JsonValue{ .data = std::string("text") };

    // Update global adjudication config
    auto inputConfig = mapJsonToConfigData(arguments);
    
    const auto sectionNames = inputConfig.getAllSectionNames();
    for (const auto& name : sectionNames) {
        if (const auto sectionMap = inputConfig.getSectionMap(name)) {
            for (const auto& [id, sections] : *sectionMap) {
                // Since ConfigData duplicates on add, we replace the section list entirely for this ID
                // to avoid accumulating duplicates over multiple calls.
                globalAdjudicationConfig_.setSectionList(name, id, sections);
            }
        }
    }
    
    textContent["text"] = JsonValue{ .data = std::string("Global adjudication settings updated.") };
    content.push_back(JsonValue{ .data = textContent });
    return content;
}

JsonValue::Array McpServer::handleControlTool(const JsonValue::Object& arguments) {
    JsonValue::Array content;
    JsonValue::Object textContent;
    textContent["type"] = JsonValue{ .data = std::string("text") };

    const std::string command = arguments.at("command").asString();
    
    std::string result;
    GameManagerPool& pool = GameManagerPool::getInstance();

    if (command == "status") {
        result = std::format("Running games: {}", pool.runningGameCount());
    } else if (command == "set_concurrency") {
        if (!arguments.contains("value") || !arguments.at("value").isNumber()) {
             throw AppError::makeInvalidParameters("Integer value required for set_concurrency.");
        }
        int value = static_cast<int>(arguments.at("value").asDouble());
        pool.setConcurrency(value, true, true);
        result = std::format("Concurrency set to {}.", value);
    } else if (command == "stop") {
        pool.stopAll();
        pool.waitForTask();
        result = "All tasks stopped.";
    } else if (command == "stop_nice") {
        pool.setConcurrency(0, true, false);
        pool.waitForTask();
        result = "All tasks stopped gracefully.";
    } else {
         throw AppError::makeInvalidParameters(std::format("Unknown control command '{}'.", command));
    }

    textContent["text"] = JsonValue{ .data = result };
    content.push_back(JsonValue{ .data = textContent });
    return content;
}

void McpServer::mergeGlobalConfig(QaplaHelpers::ConfigData& target, const QaplaHelpers::ConfigData& source) {
    const auto sectionNames = source.getAllSectionNames();
    for (const auto& name : sectionNames) {
        if (const auto sectionMap = source.getSectionMap(name)) {
            for (const auto& [id, sections] : *sectionMap) {
                for (const auto& section : sections) {
                     target.addSection(section);
                }
            }
        }
    }
    const auto& globalParams = source.getGlobalParameters();
    for(const auto& [k, v] : globalParams) {
        target.addGlobalParameter(k, v);
    }
}

std::pair<std::string, std::string> McpServer::getTaskConfigInfo(const std::string& name) {
    if (name == "sprt") {
        return {SprtTournamentFile::id, "sprt-report"};
    } 
    if (name == "tournament") {
        return {TournamentFile::id, "tournament-report"};
    } 
    if (name == "epd") {
        return {"", "epd-report"};
    } 
    if (name == "test") {
        return {"", "test-report"};
    } 
    if (name == "spsa") {
        return {"", "spsa-report"};
    } 
    if (name == "control") {
        return {"", "control-report"};
    }
    return {"", "report"};
}

void McpServer::prepareTaskFile(const std::string& name, JsonValue::Object& toolArgs) {
    if (name == "sprt") {
        bool resume = false;
        if (toolArgs.contains("resume")) {
            resume = toolArgs.at("resume").asBool();
            toolArgs.erase("resume");
        }

        std::string filename;
        if (resume && !lastSprtFile_.empty()) {
            filename = lastSprtFile_;
        } else {
             filename = QaplaHelpers::generateTimestampedFilename("sprt-tournament", BaseLogger::logPath_, "qsprt");
             lastSprtFile_ = filename;
        }
        toolArgs["sprt_file"] = JsonValue{ .data = filename };
    } else if (name == "tournament") {
        bool resume = false;
        if (toolArgs.contains("resume")) {
            resume = toolArgs.at("resume").asBool();
            toolArgs.erase("resume");
        }

        std::string filename;
        if (resume && !lastTournamentFile_.empty()) {
            filename = lastTournamentFile_;
        } else {
             filename = QaplaHelpers::generateTimestampedFilename("tournament-outcome", BaseLogger::logPath_, "qtour");
             lastTournamentFile_ = filename;
        }
        toolArgs["tournament_file"] = JsonValue{ .data = filename };
    }
}

JsonValue::Array McpServer::runRunnerTool(const std::string& name, JsonValue::Object& arguments, AppReturnCode& returnCode) {
    JsonValue::Object toolArgs = arguments;
    bool background = false;
    
    // Check for background execution parameter
    if (toolArgs.contains("mcp_background")) {
        background = toolArgs.at("mcp_background").asBool();
        toolArgs.erase("mcp_background"); // Remove so it doesn't fail settings parsing
    } else if (toolArgs.contains("background")) {
        background = toolArgs.at("background").asBool();
        toolArgs.erase("background");
    }
    
    if (toolArgs.contains("engines")) {
        toolArgs.erase("engines");
    }

    prepareTaskFile(name, toolArgs);

    auto [configId, reportBaseName] = getTaskConfigInfo(name);

    Logger::logBaseName_ = reportBaseName;
    
    auto paramsConfig = mapJsonToConfigData(toolArgs, configId);
    QaplaHelpers::ConfigData configData = globalAdjudicationConfig_;
    
    // Copy paramsConfig into configData
    mergeGlobalConfig(configData, paramsConfig);
    
    returnCode = executeRunnerTool(configData, background);

    JsonValue::Array content;
    JsonValue::Object textContent;

    textContent["type"] = JsonValue{ .data = std::string("text") };

    // Determine relevant groups for this tool to show configuration used
    std::vector<std::string> reportGroups = { "global" };
    if (name == "sprt") {
        reportGroups.insert(reportGroups.end(), { "sprt", "openings", "pgnoutput" });
    } else if (name == "tournament") {
        reportGroups.insert(reportGroups.end(), { "tournament", "openings", "pgnoutput" });
    } else if (name == "epd") {
        reportGroups.insert(reportGroups.end(), { "epd", "pgnoutput" });
    } else if (name == "spsa") {
        reportGroups.insert(reportGroups.end(), { "spsa", "spsavalue", "openings", "pgnoutput" });
    } else if (name == "test") {
        reportGroups.insert(reportGroups.end(), { "test" });
    }

    // Show limited columns for run summary (Name and Value mainly)
    std::vector<SettingsReporter::Column> reportColumns = {
        SettingsReporter::Column::FullName,
        SettingsReporter::Column::Value
    };

    std::string settingsReport = SettingsReporter::generateReport(reportGroups, reportColumns);

    if (background) {
        std::string summary = std::format("Tool '{}' started in background.", name);
        summary += std::format("\nReport Log might be available at: qapla://reports/{}/<timestamped_file>", name);
        // Note: We cannot provide exact filename as it is generated by the background thread.
        textContent["text"] = JsonValue{ .data = summary + "\n\n" + settingsReport };
    } else {
        std::string runSummary = formatRunSummary(name, returnCode);
        textContent["text"] = JsonValue{ .data = runSummary + "\n\n" + settingsReport };
    }

    content.push_back(JsonValue{ .data = textContent });
    return content;
}

} // namespace QaplaTester::Mcp
