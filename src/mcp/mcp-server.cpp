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
#include "json-helper.h"
#include "../cli/settings-manager.h"
#include "../cli/qapla-settings.h"
#include "../cli/app-runner.h"
#include "../engine-handling/engine-worker-factory.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace QaplaTester::Mcp {

void McpServer::initialize() {
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
                params["data"] = JsonValue{ .data = std::string(message) };
            }
        } else {
            params["data"] = JsonValue{ .data = std::string(message) };
        }
        
        sendNotification("notifications/message", params);
    });

    // Default MCP trace level to result
    Logger::reportLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::result);
}

AppReturnCode McpServer::run() {
    while (true) {
        const auto message = readMessage();
        if (!message.has_value()) {
            break; // EOF or error
        }

        if (!message->isObject()) {
            continue;
        }

        if (processMessage(message->asObject()) != AppReturnCode::NoError) {
            return AppReturnCode::NoError;
        }
    }

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

    struct ToolInfo {
        std::string_view name;
        std::string_view description;
        std::vector<std::string_view> groups;
    };

    const auto toolsToRegister = std::vector<ToolInfo>{
        {
            .name = "test",
            .description = "Runs basic engine tests (startup, move generation, etc.)",
            .groups = {"test", "logging", "each"}
        },
        {
            .name = "sprt",
            .description = "Runs a Sequential Probability Ratio Test (SPRT) between engines",
            .groups = {"sprt", "openings", "draw", "resign", "pgnoutput", "logging"}
        },
        {
            .name = "turnier",
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
            .groups = {"spsa", "openings", "draw", "resign", "pgnoutput", "logging"}
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

    for (const auto& info : toolsToRegister) {
        JsonValue::Object tool;
        tool["name"] = JsonValue{ .data = std::string(info.name) };
        tool["description"] = JsonValue{ .data = std::string(info.description) };
        
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

            JsonValue::Object name;
            name["type"] = JsonValue{ .data = std::string("string") };
            name["description"] = JsonValue{ .data = std::format("Engine name (Available: {})", registeredNames) };
            properties["name"] = JsonValue{ .data = name };

            JsonValue::Object newName;
            newName["type"] = JsonValue{ .data = std::string("string") };
            newName["description"] = JsonValue{ .data = std::string("New name when copying or adding an engine.") };
            properties["newName"] = JsonValue{ .data = newName };

            // Allow setting any engine parameter in add/update
            addParametersFromGroup("engine", properties);

            JsonValue::Array required;
            required.push_back(JsonValue{ .data = std::string("command") });
            inputSchema["required"] = JsonValue{ .data = required };
        } else {
            // All task tools use a simple engine list
            JsonValue::Object engines;
            engines["type"] = JsonValue{ .data = std::string("string") };
            engines["description"] = JsonValue{ .data = std::format("Comma separated list of engine names from the registry (Available: {})", registeredNames) };
            properties["engines"] = JsonValue{ .data = engines };

            for (const auto& group : info.groups) {
                addParametersFromGroup(group, properties);
            }
            
            JsonValue::Array required;
            if (info.name != "read_report") {
                required.push_back(JsonValue{ .data = std::string("engines") });
            }
            inputSchema["required"] = JsonValue{ .data = required };
        }
        
        inputSchema["properties"] = JsonValue{ .data = properties };
        
        tool["inputSchema"] = JsonValue{ .data = inputSchema };
        tools.push_back(JsonValue{ .data = tool });
    }

    resultData["tools"] = JsonValue{ .data = tools };
    responseBody["result"] = JsonValue{ .data = resultData };
    response.data = responseBody;

    sendMessage(response);
}

void McpServer::addParametersFromGroup(std::string_view groupName, JsonValue::Object& properties) {
    const auto& groupDefs = Settings::Manager::instance().getGroupDefinitions();
    const auto it = groupDefs.find(std::string(groupName));
    if (it == groupDefs.end()) {
        return;
    }

    for (const auto& [key, def] : it->second.keys) {
        if (def.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }
        
        JsonValue::Object prop;
        switch (def.type) {
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
        prop["description"] = JsonValue{ .data = def.description };
        properties[std::format("{}_{}", groupName, key)] = JsonValue{ .data = prop };
    }
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

    try {
        const auto& arguments = params.at("arguments").asObject();

        if (name == "read_report") {
            content = handleReadReport(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else if (name == "manage_engines") {
            content = handleManageEngines(arguments);
            result["isError"] = JsonValue{ .data = false };
        } else {
            auto configData = mapJsonToConfigData(arguments);
            const AppReturnCode code = executeRunnerTool(name, configData);

            JsonValue::Object textContent;
            textContent["type"] = JsonValue{ .data = std::string("text") };
            textContent["text"] = JsonValue{ .data = formatRunSummary(name, code) };
            content.push_back(JsonValue{ .data = textContent });
            
            result["isError"] = JsonValue{ .data = (code == AppReturnCode::GeneralError || 
                                                  code == AppReturnCode::InvalidParameters || 
                                                  code == AppReturnCode::EngineError || 
                                                  code == AppReturnCode::EngineMissbehaviour) };
        }
    } catch (const std::exception& e) {
        JsonValue::Object errorContent;
        errorContent["type"] = JsonValue{ .data = std::string("text") };
        errorContent["text"] = JsonValue{ .data = std::format("Error executing tool '{}': {}", name, e.what()) };
        content.push_back(JsonValue{ .data = errorContent });
        result["isError"] = JsonValue{ .data = true };
    }

    result["content"] = JsonValue{ .data = content };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
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

    const std::string valStr = valueToString(value);

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

    for (const char character : accumulated) {
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
    }

    if ((openBraces > 0 && openBraces == closeBraces) || (openBraces == 0 && !accumulated.empty())) {
        std::string_view jsonInputView = accumulated;
        auto result = JsonHelper::parse(jsonInputView);
        accumulated.clear();
        return result;
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

AppReturnCode McpServer::executeRunnerTool(const std::string& name, QaplaHelpers::ConfigData& configData) {
    // Ensure the section for the requested tool exists
    const std::string sectionName = (name == "turnier") ? "tournament" : name;
    if (!configData.getSectionList(sectionName).has_value()) {
        QaplaHelpers::IniFile::Section toolSection;
        toolSection.name = sectionName;
        configData.addSection(toolSection);
    }

    // Apply configuration
    Settings::QaplaSettings::instance().applyConfig(configData);

    // Tool-specific log names applied AFTER config (which might have cleared them)
    Settings::QaplaSettings::instance().applyLoggerConfig(std::format("report-{}", name));
    EngineLogger::logBaseName_ = std::format("engine-{}", name);

    // Run dispatcher
    return AppRunner::runDispatcher();
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

std::string McpServer::getEngineDetails(const JsonValue::Object& arguments) {
    if (!arguments.contains("name")) {
        throw AppError::makeInvalidParameters("Engine 'name' is required for 'details' command.");
    }
    const std::string name = arguments.at("name").asString();
    const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
    if (config == nullptr) {
        throw AppError::makeInvalidParameters(std::format("Engine '{}' not found.", name));
    }

    EngineConfiguration engineConfig;
    engineConfig.config = *config;
    auto section = EngineConfigFile::toSection(engineConfig, "");
    
    std::string details = std::format("Details for engine '{}':\n", name);
    for (const auto& [key, value] : section.entries) {
        details += std::format("  {} = {}\n", key, value);
    }
    return details;
}

std::string McpServer::addOrUpdateEngine(const JsonValue::Object& arguments, bool isUpdate) {
    if (!arguments.contains("name")) {
        throw AppError::makeInvalidParameters("Engine 'name' is required.");
    }
    const std::string name = arguments.at("name").asString();
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
            options[paramKey] = valueToString(value);
        }
    }

    newConfig.setCommandLineOptions(options, true);
    if (!isUpdate) {
        manager.addConfig(newConfig);
        return std::format("Engine '{}' added successfully.", name);
    } 
    
    *config = newConfig;
    return std::format("Engine '{}' updated successfully.", name);
}

std::string McpServer::copyEngine(const JsonValue::Object& arguments) {
    if (!arguments.contains("name") || !arguments.contains("newName")) {
        throw AppError::makeInvalidParameters("'name' and 'newName' are required for 'copy' command.");
    }
    const std::string name = arguments.at("name").asString();
    const std::string newName = arguments.at("newName").asString();
    
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
            options[paramKey] = valueToString(value);
        }
    }

    if (options.empty()) {
            throw AppError::makeInvalidParameters("No parameters provided to update all engines.");
    }

    for (auto& config : EngineWorkerFactory::getConfigManagerMutable().getAllConfigsMutable()) {
        config.setCommandLineOptions(options, true);
    }
    return "All registered engines updated successfully.";
}

std::string McpServer::valueToString(const JsonValue& value) {
    if (value.isString()) {
        return value.asString();
    }
    if (value.isNumber()) {
        double d = value.asDouble();
        if (d == static_cast<double>(static_cast<long long>(d))) {
            return std::to_string(static_cast<long long>(d));
        }
        return std::format("{}", d);
    }
    if (value.isBool()) {
        return value.asBool() ? "true" : "false";
    }
    return "";
}

} // namespace QaplaTester::Mcp
