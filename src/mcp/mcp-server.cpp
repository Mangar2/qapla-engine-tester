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
#include "../base-elements/file-helper.h"
#include "../base-elements/base-logger.h"
#include "json-helper.h"
#include "mcp-converter.h"
#include "../cli/settings-definitions.h"
#include "../cli/settings-manager.h"
#include "../cli/qapla-settings.h"
#include "../cli/app-runner.h"
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
    QaplaConfiguration::EngineCapabilities::setMcpNotificationCallback([](const std::string& message) {
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
    bool isTestMode = false;
    if (auto mcpGroup = Settings::Manager::instance().getGroupInstance("mcp")) {
        isTestMode = mcpGroup->get<bool>("test");
    }

    AppReturnCode lastResult = AppReturnCode::NoError;
    while (true) {
        const auto message = readMessage();
        if (!message.has_value()) {
            break; // EOF or error
        }

        if (!message->isObject()) {
            continue;
        }

        const auto& jsonObject = message->asObject();
        std::string method;
        if (jsonObject.contains("method")) {
            method = jsonObject.at("method").asString();
        }

        lastResult = processMessage(jsonObject);

        // In test mode, terminate after the first non-handshake message (request or notification)
        if (isTestMode && !method.empty() && (method != "initialize") && (method != "notifications/initialized")) {
            break;
        }
    }

    return isTestMode ? lastResult : AppReturnCode::NoError;
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
        return callTool(jsonObject);
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
    else if (method == "exit") {
        return AppReturnCode::GeneralError; // Signal exit
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

    const auto toolsToRegister = std::vector<ToolInfo>{
        {
            .name = "test",
            .description = "Runs basic engine tests (startup, move generation, etc.)",
            .groups = {"test", "logging"}
        },
        {
            .name = "sprt",
            .description = "Runs a Sequential Probability Ratio Test (SPRT) between engines",
            .groups = {"sprt", "openings", "draw", "resign", "pgnoutput", "logging"}
        },
        {
            .name = "tournament",
            .description = "Runs a tournament between engines",
            .groups = {"tournament", "openings", "draw", "resign", "pgnoutput", "logging"}
        },
        {
            .name = "epd",
            .description = "Runs an EPD testset against engines",
            .groups = {"epd", "pgnoutput", "logging"}
        },
        {
            .name = "spsa",
            .description = "Optimizes engine parameters using SPSA",
            .groups = {"spsa", "spsavalue", "openings", "draw", "resign", "pgnoutput", "logging"}
        },
        {
            .name = "control",
            .description = "Control the execution of running tasks (concurrency, stop)",
            .groups = {}
        },
        {
            .name = "manage_engines",
            .description = "Manage engine configurations in the registry. Use this to list, add, copy, or update engines before running tasks.",
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
        tool["inputSchema"] = JsonValue{ .data = createInputSchema(info, registeredNames) };
        
        tools.push_back(JsonValue{ .data = tool });
    }

    resultData["tools"] = JsonValue{ .data = tools };
    responseBody["result"] = JsonValue{ .data = resultData };
    response.data = responseBody;

    sendMessage(response);
}

JsonValue::Object McpServer::createInputSchema(const ToolInfo& info, const std::string& registeredNames) {
    JsonValue::Object inputSchema;
    inputSchema["type"] = JsonValue{ .data = std::string("object") };
    
    JsonValue::Object properties;
    
    if (info.name == "read_report") {
        JsonValue::Object uri;
        uri["type"] = JsonValue{ .data = std::string("string") };
        uri["description"] = JsonValue{ .data = std::string("The URI or filename of the report to read (e.g. qapla://reports/sprt/report.log)") };
        properties["uri"] = JsonValue{ .data = uri };

        JsonValue::Array required;
        required.push_back(JsonValue{ .data = std::string("uri") });
        inputSchema["required"] = JsonValue{ .data = required };
    } else if (info.name == "control") {
        JsonValue::Object command;
        command["type"] = JsonValue{ .data = std::string("string") };
        command["enum"] = JsonValue{ .data = JsonValue::Array{ 
            JsonValue{ .data = std::string("status") }, 
            JsonValue{ .data = std::string("set_concurrency") }, 
            JsonValue{ .data = std::string("stop") }, 
            JsonValue{ .data = std::string("stop_nice") } 
        } };
        command["description"] = JsonValue{ .data = std::string("The operation to perform.") };
        properties["command"] = JsonValue{ .data = command };

        JsonValue::Object value;
        value["type"] = JsonValue{ .data = std::string("integer") };
        value["description"] = JsonValue{ .data = std::string("Value for the command (e.g. concurrency level).") };
        properties["value"] = JsonValue{ .data = value };

        JsonValue::Array required;
        required.push_back(JsonValue{ .data = std::string("command") });
        inputSchema["required"] = JsonValue{ .data = required };
    } else if (info.name == "manage_engines") {
        JsonValue::Object command;
        command["type"] = JsonValue{ .data = std::string("string") };
        command["enum"] = JsonValue{ .data = JsonValue::Array{ 
            JsonValue{ .data = std::string("list") }, 
            JsonValue{ .data = std::string("details") }, 
            JsonValue{ .data = std::string("add") }, 
            JsonValue{ .data = std::string("copy") }, 
            JsonValue{ .data = std::string("update") }, 
            JsonValue{ .data = std::string("update_all") } 
        } };
        command["description"] = JsonValue{ .data = std::string("The operation to perform on engines.") };
        properties["command"] = JsonValue{ .data = command };

        JsonValue::Object engine_name;
        engine_name["type"] = JsonValue{ .data = std::string("string") };
        engine_name["description"] = JsonValue{ .data = std::format("Primary engine name (Available: {}). No spaces allowed!", registeredNames) };
        properties["engine_name"] = JsonValue{ .data = engine_name };

        // Manually add engine parameters with engine_ prefix since the "engine" group is not unique
        const auto allEngineKeys = Settings::getEngineKeys();
        for (const auto& [key, def] : allEngineKeys) {
            // "conf" translates to --engine.conf which is used in CLI to reference an engine from ini,
            // but in MCP manage_engines we use engine_name directly.
            if (def.isHidden || key == "id" || key == "name" || key == "conf" ||
                key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
                continue;
            }
            
            JsonValue::Object prop;
            switch (def.type) {
                case Settings::ValueType::Bool: prop["type"] = JsonValue{ .data = std::string("boolean") }; break;
                case Settings::ValueType::Int:
                case Settings::ValueType::UInt: prop["type"] = JsonValue{ .data = std::string("integer") }; break;
                case Settings::ValueType::Float: prop["type"] = JsonValue{ .data = std::string("number") }; break;
                default: prop["type"] = JsonValue{ .data = std::string("string") }; break;
            }
            prop["description"] = JsonValue{ .data = def.longDescription.empty() ? def.description : def.longDescription };
            properties[std::format("engine_{}", key)] = JsonValue{ .data = prop };
        }

        JsonValue::Object copyName;
        copyName["type"] = JsonValue{ .data = std::string("string") };
        copyName["description"] = JsonValue{ .data = std::string("Target name when copying an engine.") };
        properties["engine_copyName"] = JsonValue{ .data = copyName };

        // Hint for dynamic UCI options
        JsonValue::Object optionProp;
        optionProp["type"] = JsonValue{ .data = std::string("string") };
        optionProp["description"] = JsonValue{ .data = std::string("Set any UCI option. Syntax: engine_option_<OptionName>=<Value>. Example: engine_option_Hash=128. Use the 'details' command to list available options for a specific engine.") };
        properties["engine_option_<name>"] = JsonValue{ .data = optionProp };

        JsonValue::Array required;
        required.push_back(JsonValue{ .data = std::string("command") });
        inputSchema["required"] = JsonValue{ .data = required };
    } else {
        // All task tools use a simple engine list
        JsonValue::Object engines;
        engines["type"] = JsonValue{ .data = std::string("string") };
        engines["description"] = JsonValue{ .data = std::format("Comma separated list of engine names from the registry (Available: {}). Engine names must not contain spaces.", registeredNames) };
        properties["engines"] = JsonValue{ .data = engines };

        // Global settings
        JsonValue::Object concurrency;
        concurrency["type"] = JsonValue{ .data = std::string("integer") };
        concurrency["description"] = JsonValue{ .data = std::string("Maximal number of in parallel running engines") };
        properties["concurrency"] = JsonValue{ .data = concurrency };

        JsonValue::Object rapid;
        rapid["type"] = JsonValue{ .data = std::string("boolean") };
        rapid["description"] = JsonValue{ .data = std::string("Enables rapid mode (suppresses engine info lines)") };
        properties["rapid"] = JsonValue{ .data = rapid };

        JsonValue::Object background;
        background["type"] = JsonValue{ .data = std::string("boolean") };
        background["description"] = JsonValue{ .data = std::string("If true, starts the task in background and returns immediately. Use 'control' tool to monitor.") };
        properties["mcp_background"] = JsonValue{ .data = background };

        if (info.name == "sprt") {
             JsonValue::Object resume;
             resume["type"] = JsonValue{ .data = std::string("boolean") };
             resume["description"] = JsonValue{ .data = std::string("If true, resumes sending results to the last used SPRT file. If false (default), creates a new timestamped file.") };
             properties["resume"] = JsonValue{ .data = resume };
        }

        for (const auto& group : info.groups) {
            addParametersFromGroup(group, properties);
        }
        
        JsonValue::Array required;
        required.push_back(JsonValue{ .data = std::string("engines") });
        inputSchema["required"] = JsonValue{ .data = required };
    }
    
    inputSchema["properties"] = JsonValue{ .data = properties };
    return inputSchema;
}

void McpServer::addParametersFromGroup(std::string_view groupName, JsonValue::Object& properties) {
    const auto& groupDefs = Settings::Manager::instance().getGroupDefinitions();
    const auto it = groupDefs.find(std::string(groupName));
    if (it == groupDefs.end()) {
        return;
    }

    if (!it->second.unique) {
        addArrayGroupSchema(std::string(groupName), it->second, properties);
    } else {
        addSingleGroupSchema(std::string(groupName), it->second, properties);
    }
}

void McpServer::addArrayGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties) {
    // Handle non-unique groups (like spsavalue) as an array of objects
    JsonValue::Object arrayProp;
    arrayProp["type"] = JsonValue{ .data = std::string("array") };
    
    JsonValue::Object items;
    items["type"] = JsonValue{ .data = std::string("object") };
    
    JsonValue::Object itemProperties;
    JsonValue::Array itemRequired;
    for (const auto& [key, keyDef] : def.keys) {
        if (keyDef.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }
        
        JsonValue::Object prop;
        switch (keyDef.type) {
            case Settings::ValueType::Bool: 
                prop["type"] = JsonValue{ .data = std::string("boolean") }; 
                break;
            case Settings::ValueType::Int:
            case Settings::ValueType::UInt: 
                prop["type"] = JsonValue{ .data = std::string("integer") }; 
                break;
            case Settings::ValueType::Float: 
                prop["type"] = JsonValue{ .data = std::string("number") }; 
                break;
            default: 
                prop["type"] = JsonValue{ .data = std::string("string") }; 
                break;
        }
        prop["description"] = JsonValue{ .data = keyDef.longDescription.empty() ? std::string(keyDef.description) : keyDef.longDescription };
        itemProperties[key] = JsonValue{ .data = prop };
        
        if (keyDef.isRequired) {
                itemRequired.push_back(JsonValue{ .data = key });
        }
    }
    items["properties"] = JsonValue{ .data = itemProperties };
    if (!itemRequired.empty()) {
        items["required"] = JsonValue{ .data = itemRequired };
    }
    arrayProp["items"] = JsonValue{ .data = items };
    arrayProp["description"] = JsonValue{ .data = def.longDescription.empty() ? std::string(def.description) : def.longDescription };
    
    properties[groupName] = JsonValue{ .data = arrayProp };
}

void McpServer::addSingleGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties) {
    for (const auto& [key, keyDef] : def.keys) {
        if (keyDef.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }
        
        JsonValue::Object prop;
        switch (keyDef.type) {
            case Settings::ValueType::Bool: 
                prop["type"] = JsonValue{ .data = std::string("boolean") }; 
                break;
            case Settings::ValueType::Int:
            case Settings::ValueType::UInt: 
                prop["type"] = JsonValue{ .data = std::string("integer") }; 
                break;
            case Settings::ValueType::Float: 
                prop["type"] = JsonValue{ .data = std::string("number") }; 
                break;
            default: 
                prop["type"] = JsonValue{ .data = std::string("string") }; 
                break;
        }
        prop["description"] = JsonValue{ .data = keyDef.longDescription.empty() ? keyDef.description : keyDef.longDescription };
        properties[std::format("{}_{}", groupName, key)] = JsonValue{ .data = prop };
    }
}

AppReturnCode McpServer::callTool(const JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("params") || !jsonObject.at("params").isObject()) {
        return AppReturnCode::NoError;
    }
    const auto& params = jsonObject.at("params").asObject();
    if (!params.contains("name")) {
        return AppReturnCode::NoError;
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
            content = handleManageEngines(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else {
            // Handle active list and execution
            JsonValue::Object toolArgs = arguments;
            setupActiveEngines(toolArgs);
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
    return returnCode;
}

QaplaHelpers::ConfigData McpServer::mapJsonToConfigData(const JsonValue::Object& arguments) {
    QaplaHelpers::ConfigData configData;
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section> otherGroupedSections;

    // Parameters
    for (const auto& [key, value] : arguments) {
        processParameter(key, value, otherGroupedSections, configData);
    }

    // Add sections to configData
    for (auto& [name, s] : otherGroupedSections) {
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
    return AppRunner::runDispatcher(background);
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

JsonValue::Array McpServer::handleManageEngines(const JsonValue::Object& arguments) {
    JsonValue::Array content;
    JsonValue::Object textContent;
    textContent["type"] = JsonValue{ .data = std::string("text") };

    const std::string command = arguments.at("command").asString();
    
    std::string result;
    if (command == "list") {
        result = listEngines();
    } else if (command == "details") {
        result = getEngineDetails(arguments);
    } else if (command == "add") {
        result = addOrUpdateEngine(arguments, false);
    } else if (command == "update") {
        result = addOrUpdateEngine(arguments, true);
    } else if (command == "copy") {
        result = copyEngine(arguments);
    } else if (command == "update_all") {
        result = updateAllEngines(arguments);
    } else {
        throw AppError::makeInvalidParameters(std::format("Unknown engine command '{}'.", command));
    }

    textContent["text"] = JsonValue{ .data = result };
    content.push_back(JsonValue{ .data = textContent });
    return content;
}

std::string McpServer::listEngines() {
    std::string list;
    const auto& manager = EngineWorkerFactory::getConfigManager();
    for (const auto& config : manager.getAllConfigs()) {
        if (!list.empty()) {
            list += ", ";
        }
        list += config.getName();
    }
    return std::format("Registered engines: {}", list.empty() ? "None" : list);
}

namespace {

std::string formatEngineConfiguration(const EngineConfig* config) {
    std::string details = "\nConfiguration:\n";
    if (!config->getDir().empty()) {
        details += std::format("  dir = {}\n", config->getDir());
    }
    if (!config->getArgs().empty()) {
        details += std::format("  args = {}\n", config->getArgs());
    }
    
    details += std::format("  tc = {}\n", QaplaTester::to_string(config->getTimeControl()));
    details += std::format("  restart = {}\n", QaplaTester::to_string(config->getRestartOption()));
    details += std::format("  ponder = {}\n", config->isPonderEnabled() ? "true" : "false");
    details += std::format("  gauntlet = {}\n", config->isGauntlet() ? "true" : "false");

    const auto options = config->getOptionValues();
    if (!options.empty()) {
        for (const auto& [key, value] : options) {
            details += std::format("  option.{} = {}\n", key, value);
        }
    }
    return details;
}

std::string formatSupportedOptions(const EngineConfig* config, const QaplaConfiguration::EngineCapabilities& capabilities) {
    std::string details;
    if (const auto cap = capabilities.getCapability(config->getCmd(), config->getProtocol())) {
        details += "\nSupported Options:\n";
        for (const auto& opt : cap->getSupportedOptions()) {
            details += std::format("  -- Name: {}\n", opt.name);
            details += std::format("     Type: {}\n", QaplaTester::EngineOption::to_string(opt.type));
            
            if (!opt.defaultValue.empty()) {
                details += std::format("     Default: {}\n", opt.defaultValue);
            }
            if (opt.min.has_value()) {
                details += std::format("     Min: {}\n", *opt.min);
            }
            if (opt.max.has_value()) {
                details += std::format("     Max: {}\n", *opt.max);
            }
            if (!opt.vars.empty()) {
                details += "     Vars: ";
                for (const auto& v : opt.vars) {
                    details += std::format("'{}' ", v);
                }
                details += "\n";
            }
        }
    }
    return details;
}

} // namespace

std::string McpServer::getEngineDetails(const JsonValue::Object& arguments) {
    if (!arguments.contains("engine_name")) {
        throw AppError::makeInvalidParameters("Engine 'engine_name' is required for 'details' command.");
    }
    const std::string name = arguments.at("engine_name").asString();
    const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
    if (config == nullptr) {
        throw AppError::makeInvalidParameters(std::format("Engine '{}' not found.", name));
    }

    std::string details = std::format("Details for engine '{}':\n", name);

    details += std::format("  Configured Name: {}\n", config->getName());
    details += std::format("  Reported Name:   {}\n", config->getReportedName());
    details += std::format("  Executable:      {}\n", config->getCmd());
    details += std::format("  Protocol:        {}\n", QaplaTester::to_string(config->getProtocol()));

    details += formatEngineConfiguration(config);
    details += formatSupportedOptions(config, capabilities_);

    return details;
}

std::string McpServer::addOrUpdateEngine(const JsonValue::Object& arguments, bool isUpdate) {
    if (!arguments.contains("engine_name") || arguments.at("engine_name").asString().empty()) {
        throw AppError::makeInvalidParameters("Engine 'engine_name' is required.");
    }
    if (!isUpdate && (!arguments.contains("engine_cmd") || arguments.at("engine_cmd").asString().empty())) {
        throw AppError::makeInvalidParameters("Engine 'engine_cmd' (path to executable) is required for adding an engine via MCP.");
    }
    const std::string name = arguments.at("engine_name").asString();

    if (name.find(' ') != std::string::npos) {
        throw AppError::makeInvalidParameters("Engine names must not contain spaces.");
    }

    auto& manager = EngineWorkerFactory::getConfigManagerMutable();
    EngineConfig* config = isUpdate ? manager.getConfigMutable(name) : nullptr;
    
    if (isUpdate && (config == nullptr)) {
        throw AppError::makeInvalidParameters(std::format("Engine '{}' not found for update.", name));
    }

    EngineConfig newConfig;
    if (config != nullptr) {
        newConfig = *config;
    } else {
        newConfig.setName(name);
    }

    // Collect all engine_ parameters
    Settings::ValueMap options;
    for (const auto& [key, value] : arguments) {
        if (key.starts_with("engine_")) {
            std::string paramKey = key.substr(7);
            if (paramKey.starts_with("option_")) {
                paramKey = "option." + paramKey.substr(7);
            }
            options[paramKey] = convertJsonToEngineSetting(paramKey, value);
        }
    }

    newConfig.setCommandLineOptions(options, true);
    if (!isUpdate) {
        manager.addConfig(newConfig);
        capabilities_.autoDetect();
        return std::format("Engine '{}' added successfully.", name);
    } 
    
    *config = newConfig;
    capabilities_.autoDetect();
    return std::format("Engine '{}' updated successfully.", name);
}

std::string McpServer::copyEngine(const JsonValue::Object& arguments) {
    if (!arguments.contains("engine_name") || !arguments.contains("engine_copyName")) {
        throw AppError::makeInvalidParameters("'engine_name' and 'engine_copyName' are required for 'copy' command.");
    }
    const std::string name = arguments.at("engine_name").asString();
    const std::string newName = arguments.at("engine_copyName").asString();
    
    if (newName.find(' ') != std::string::npos) {
        throw AppError::makeInvalidParameters("Engine names must not contain spaces.");
    }

    auto& manager = EngineWorkerFactory::getConfigManagerMutable();
    const auto* config = manager.getConfig(name);
    if (config == nullptr) {
        throw AppError::makeInvalidParameters(std::format("Source engine '{}' not found.", name));
    }

    EngineConfig copy = *config;
    copy.setName(newName);
    manager.addConfig(copy);
    return std::format("Engine '{}' copied to '{}' successfully.", name, newName);
}

std::string McpServer::updateAllEngines(const JsonValue::Object& arguments) {
    Settings::ValueMap options;
    for (const auto& [key, value] : arguments) {
        if (key.starts_with("engine_")) {
            std::string paramKey = key.substr(7);
            if (paramKey.starts_with("option_")) {
                paramKey = "option." + paramKey.substr(7);
            }
            options[paramKey] = convertJsonToEngineSetting(paramKey, value);
        }
    }

    if (options.empty()) {
            throw AppError::makeInvalidParameters("No parameters provided to update all engines.");
    }

    for (auto& config : EngineWorkerFactory::getConfigManagerMutable().getAllConfigsMutable()) {
        config.setCommandLineOptions(options, true);
    }
    capabilities_.autoDetect();
    return "All registered engines updated successfully.";
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

void McpServer::setupActiveEngines(const JsonValue::Object& arguments) {
    if (!arguments.contains("engines")) {
        return;
    }

    const std::string engineList = arguments.at("engines").asString();
    auto& activeEngines = EngineWorkerFactory::getActiveEnginesMutable();
    activeEngines.clear();
    
    std::stringstream ss(engineList);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
            std::string name = QaplaHelpers::trim(segment); 
            if (name.empty()) {
                continue;
            }
            if (name.find(' ') != std::string::npos) {
            throw AppError::makeInvalidParameters(std::format(
                "Invalid engine name '{}'. Engine names must not contain spaces. "
                "Please rename this engine in the registry using a name without spaces.", name));
            }
            
            const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
            if (config != nullptr) {
            activeEngines.push_back(*config);
            } else {
            throw AppError::makeInvalidParameters(std::format("Engine '{}' not found in registry.", name));
            }
    }
    
    EngineWorkerFactory::assignUniqueDisplayNames();
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
    }

    auto configData = mapJsonToConfigData(toolArgs);
    returnCode = executeRunnerTool(configData, background);

    JsonValue::Array content;
    JsonValue::Object textContent;

    textContent["type"] = JsonValue{ .data = std::string("text") };
    textContent["text"] = JsonValue{ .data = formatRunSummary(name, returnCode) };
    content.push_back(JsonValue{ .data = textContent });
    return content;
}

} // namespace QaplaTester::Mcp
