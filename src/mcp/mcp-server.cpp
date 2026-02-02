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
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>

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
    std::string inputLine;
    if (!std::getline(std::cin, inputLine)) {
        return std::nullopt;
    }
    if (inputLine.empty()) {
        return readMessage(); // Skip empty lines
    }
    std::string_view jsonInputView = inputLine;
    return JsonHelper::parse(jsonInputView);
}

void McpServer::listTools(const JsonValue& requestId) {
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    responseBody["id"] = requestId;

    JsonValue::Object resultData;
    JsonValue::Array tools;

    // Tool: test-engines
    {
        JsonValue::Object tool;
        tool["name"] = JsonValue{ .data = std::string("test-engines") };
        tool["description"] = JsonValue{ .data = std::string("Runs basic engine tests (startup, move generation, etc.)") };
        
        JsonValue::Object inputSchema;
        inputSchema["type"] = JsonValue{ .data = std::string("object") };
        
        JsonValue::Object properties;
        JsonValue::Object engines;
        engines["type"] = JsonValue{ .data = std::string("string") };
        engines["description"] = JsonValue{ .data = std::string("Comma separated list of engine names to test") };
        properties["engines"] = JsonValue{ .data = engines };

        JsonValue::Object numGames;
        numGames["type"] = JsonValue{ .data = std::string("integer") };
        numGames["description"] = JsonValue{ .data = std::string("Number of games to run (default: 1)") };
        properties["test_numgames"] = JsonValue{ .data = numGames };
        
        inputSchema["properties"] = JsonValue{ .data = properties };
        JsonValue::Array required;
        required.push_back(JsonValue{ .data = std::string("engines") });
        inputSchema["required"] = JsonValue{ .data = required };
        
        tool["inputSchema"] = JsonValue{ .data = inputSchema };
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

    try {
        const auto& arguments = params.at("arguments").asObject();
        auto configData = mapJsonToConfigData(arguments);
        
        // Always ensure MCP mode is active and CLI output is suppressed
        configData.addGlobalParameter("mcp", "true");
        
        QaplaHelpers::IniFile::Section loggingSection;
        loggingSection.name = "logging";
        loggingSection.addEntry("trace", "none");
        configData.addSection(loggingSection);

        // Apply configuration - this performs parsing, validation and internal state sync
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
                if (!enginePath.empty()) {
                    QaplaHelpers::IniFile::Section engineSection;
                    engineSection.name = "engine";
                    engineSection.addEntry("cmd", enginePath);
                    // Name is usually derived from filename in setEngineConfig if missing
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

void McpServer::listResources(const JsonValue& id) {
    JsonValue response;
    JsonValue::Object responseBody;
    responseBody["jsonrpc"] = JsonValue{ .data = std::string("2.0") };
    responseBody["id"] = id;

    JsonValue::Object resultData;
    JsonValue::Array resources;

    try {
        if (std::filesystem::exists("log")) {
            for (const auto& entry : std::filesystem::directory_iterator("log")) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    if (ext == ".log" || ext == ".pgn") {
                        JsonValue::Object resource;
                        resource["uri"] = JsonValue{ .data = std::format("file:///log/{}", entry.path().filename().string()) };
                        resource["name"] = JsonValue{ .data = entry.path().filename().string() };
                        resource["mimeType"] = JsonValue{ .data = (ext == ".pgn" ? std::string("text/x-chess-pgn") : std::string("text/plain")) };
                        resources.push_back(JsonValue{ .data = resource });
                    }
                }
            }
        }
    } catch (...) {} // NOLINT(bugprone-empty-catch)

    resultData["resources"] = JsonValue{ .data = resources };
    responseBody["result"] = JsonValue{ .data = resultData };
    response.data = responseBody;

    sendMessage(response);
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
        // Basic check to ensure it's in the log folder
        if (uri.starts_with("file:///log/")) {
            std::string filename = uri.substr(12);
            std::filesystem::path path = std::filesystem::path("log") / filename;
            
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
    } catch (...) {} // NOLINT(bugprone-empty-catch)

    result["contents"] = JsonValue{ .data = contents };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
}

} // namespace QaplaTester::Mcp
