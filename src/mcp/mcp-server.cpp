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
        params["data"] = JsonValue{ .data = std::string(message) };
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
        // Handle Content-Length header
        if (line.starts_with("Content-Length:")) {
            try {
                const size_t length = std::stoull(line.substr(15));
                std::string empty;
                std::getline(std::cin, empty); // Usually an empty line follows headers
                
                std::string content(length, '\0');
                if (std::cin.read(content.data(), static_cast<std::streamsize>(length))) {
                    std::string_view contentView = content;
                    return JsonHelper::parse(contentView);
                }
            } catch (...) {
                // Fallback to normal parsing
            }
            continue;
        }

        if (line.empty() && accumulated.empty()) {
            continue;
        }

        accumulated += line;
        
        // Simple heuristic for multi-line JSON: count braces/brackets
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

        if (openBraces > 0 && openBraces == closeBraces) {
            std::string_view jsonInputView = accumulated;
            auto result = JsonHelper::parse(jsonInputView);
            accumulated.clear();
            return result;
        }

        // If it's a single line and no braces (might be simple value), return it
        if (openBraces == 0 && !accumulated.empty()) {
            std::string_view jsonInputView = accumulated;
            auto result = JsonHelper::parse(jsonInputView);
            accumulated.clear();
            return result;
        }
        
        // Otherwise, keep accumulating lines
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
            .groups = {"sprt", "openings", "draw", "resign", "pgnoutput", "logging", "each"}
        },
        {
            .name = "turnier",
            .description = "Runs a tournament between engines",
            .groups = {"tournament", "openings", "draw", "resign", "pgnoutput", "logging", "each"}
        },
        {
            .name = "epd",
            .description = "Runs an EPD testset against engines",
            .groups = {"epd", "pgnoutput", "logging", "each"}
        },
        {
            .name = "spsa",
            .description = "Optimizes engine parameters using SPSA",
            .groups = {"spsa", "openings", "draw", "resign", "pgnoutput", "logging", "each"}
        }
    };

    const auto& engineConfigs = EngineWorkerFactory::getConfigManager().getAllConfigs();
    std::string confDescription = "Comma separated list of pre-configured engine names";
    if (!engineConfigs.empty()) {
        std::string names;
        for (const auto& config : engineConfigs) {
            if (!names.empty()) {
                names += ", ";
            }
            names += std::format("'{}'", config.getName());
        }
        confDescription += std::format(" (Available: {})", names);
    }

    for (const auto& info : toolsToRegister) {
        JsonValue::Object tool;
        tool["name"] = JsonValue{ .data = std::string(info.name) };
        tool["description"] = JsonValue{ .data = std::string(info.description) };
        
        JsonValue::Object inputSchema;
        inputSchema["type"] = JsonValue{ .data = std::string("object") };
        
        JsonValue::Object properties;
        
        // Always add engine parameters
        JsonValue::Object engines;
        engines["type"] = JsonValue{ .data = std::string("string") };
        engines["description"] = JsonValue{ .data = std::string("Comma separated list of engine executable paths") };
        properties["engines"] = JsonValue{ .data = engines };

        JsonValue::Object engineConf;
        engineConf["type"] = JsonValue{ .data = std::string("string") };
        engineConf["description"] = JsonValue{ .data = confDescription };
        properties["engine_conf"] = JsonValue{ .data = engineConf };

        for (const auto& group : info.groups) {
            addParametersFromGroup(group, properties);
        }
        
        inputSchema["properties"] = JsonValue{ .data = properties };
        
        // At least one of engines or engine_conf should be provided, 
        // but showing 'engines' as required is a good default hint.
        JsonValue::Array required;
        // required.push_back(JsonValue{ .data = std::string("engines") });
        inputSchema["required"] = JsonValue{ .data = required };
        
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
        
        auto configData = mapJsonToConfigData(arguments);
        
        // Ensure the section for the requested tool exists, even if no arguments were provided
        // This triggers the dispatcher to run the corresponding mode.
        if (name == "test" || name == "sprt" || name == "turnier" || name == "tournament" || name == "epd" || name == "spsa") {
            const std::string sectionName = (name == "turnier") ? "tournament" : name;
            if (!configData.getSectionList(sectionName).has_value()) {
                QaplaHelpers::IniFile::Section toolSection;
                toolSection.name = sectionName;
                configData.addSection(toolSection);
            }
        }

        // Use tool-specific log names for better resource categorization.
        // This will cause the loggers to create new files for this specific tool run.
        Logger::logBaseName_ = std::format("report-{}", name);
        EngineLogger::logBaseName_ = std::format("engine-{}", name);

        // Apply configuration - this performs parsing, validation and internal state sync
        // QaplaSettings will handle merging with cliConfigData and settingsfile.
        Settings::QaplaSettings::instance().applyConfig(configData, false);

        // Run dispatcher - this will decide what to run based on the settings
        AppReturnCode code = AppRunner::runDispatcher();

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

        JsonValue::Object textContent;
        textContent["type"] = JsonValue{ .data = std::string("text") };
        textContent["text"] = JsonValue{ .data = summary };
        content.push_back(JsonValue{ .data = textContent });
        
        // Treat codes >= 10 as errors if they are GeneralError, EngineError or EngineMissbehaviour
        // SPRT results (14, 15) are not errors.
        bool isError = (code == AppReturnCode::GeneralError || 
                        code == AppReturnCode::InvalidParameters || 
                        code == AppReturnCode::EngineError || 
                        code == AppReturnCode::EngineMissbehaviour);
        result["isError"] = JsonValue{ .data = isError };
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
QaplaHelpers::ConfigData McpServer::mapJsonToConfigData(const JsonValue::Object& arguments) {
    QaplaHelpers::ConfigData configData;
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section> groupedSections;

    for (const auto& [key, value] : arguments) {
        std::string valueStr;
        if (value.isString()) {
            valueStr = value.asString();
        } else if (value.isNumber()) {
            double d = value.asDouble();
            if (d == static_cast<double>(static_cast<long long>(d))) {
                valueStr = std::to_string(static_cast<long long>(d));
            } else {
                valueStr = std::format("{}", d);
            }
        } else if (value.isBool()) {
            valueStr = value.asBool() ? "true" : "false";
        }

        if (key == "engines") {
            // Special handling for engines: split by comma and create multiple [engine] sections
            std::stringstream ss(valueStr);
            std::string enginePath;
            while (std::getline(ss, enginePath, ',')) {
                enginePath = QaplaHelpers::trim(enginePath);
                if (!enginePath.empty()) {
                    QaplaHelpers::IniFile::Section engineSection;
                    engineSection.name = "engine";
                    engineSection.addEntry("cmd", enginePath);
                    // Name is usually derived from filename in setEngineConfig if missing
                    configData.addSection(engineSection);
                }
            }
        }
        else if (key == "engine_conf") {
            // Special handling for pre-configured engines
            std::stringstream ss(valueStr);
            std::string confName;
            while (std::getline(ss, confName, ',')) {
                confName = QaplaHelpers::trim(confName);
                if (!confName.empty()) {
                    QaplaHelpers::IniFile::Section engineSection;
                    engineSection.name = "engine";
                    engineSection.addEntry("conf", confName);
                    configData.addSection(engineSection);
                }
            }
        }
        else if (size_t underscorePos = key.find('_'); underscorePos != std::string::npos) {
            std::string sectionName = key.substr(0, underscorePos);
            std::string paramKey = key.substr(underscorePos + 1);
            
            groupedSections[sectionName].name = sectionName;
            groupedSections[sectionName].addEntry(paramKey, valueStr);
        }
        else {
            configData.addGlobalParameter(key, valueStr);
        }
    }
    
    for (const auto& [name, section] : groupedSections) {
        configData.addSection(section);
    }
    
    return configData;
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

    try {
        if (!BaseLogger::logPath_.empty() && std::filesystem::exists(BaseLogger::logPath_)) {
            for (const auto& entry : std::filesystem::directory_iterator(BaseLogger::logPath_)) {
                addResourceIfValid(entry, resources);
            }
        }
    } catch (...) { // NOLINT(bugprone-empty-catch)
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

    try {
        // Handle qapla://reports/<tool>/<filename>
        if (uri.starts_with("qapla://reports/")) {
            size_t lastSlash = uri.find_last_of('/');
            if (lastSlash != std::string::npos) {
                std::string filename = uri.substr(lastSlash + 1);
                std::filesystem::path path = std::filesystem::path(BaseLogger::logPath_) / filename;
                
                if (std::filesystem::exists(path)) {
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
    } catch (...) {} // NOLINT(bugprone-empty-catch)

    result["contents"] = JsonValue{ .data = contents };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
}

} // namespace QaplaTester::Mcp
