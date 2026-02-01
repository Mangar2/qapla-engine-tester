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
#include "../cli/app-runner.h"
#include "../engine-handling/engine-worker-factory.h"
#include <iostream>

namespace QaplaTester::Mcp {

void McpServer::initialize() {
    silenceLoggers();
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
        properties["numGames"] = JsonValue{ .data = numGames };
        
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

    if (name == "test-engines") {
        const auto& args = params.at("arguments").asObject();
        const std::string& enginesStr = args.at("engines").asString();
        
        // Split engines by comma
        std::vector<std::string> enginePaths;
        size_t start = 0;
        size_t end = enginesStr.find(',');
        while (end != std::string::npos) {
            enginePaths.push_back(enginesStr.substr(start, end - start));
            start = end + 1;
            end = enginesStr.find(',', start);
        }
        enginePaths.push_back(enginesStr.substr(start));

        // Setup engines in Factory
        auto& activeEngines = EngineWorkerFactory::getActiveEnginesMutable();
        activeEngines.clear();
        for (const auto& path : enginePaths) {
            EngineConfig config;
            config.setCmd(path);
            config.setName(std::filesystem::path(path).filename().string());
            activeEngines.push_back(config);
        }

        // Set numGames if provided
        if (args.contains("numGames")) {
            QaplaHelpers::ConfigData testConfig;
            QaplaHelpers::IniFile::Section testSection;
            testSection.name = "test";
            testSection.addEntry("numgames", std::to_string(static_cast<int>(args.at("numGames").asDouble())));
            testConfig.addSection(testSection);
            Settings::Manager::instance().parseInput(testConfig, true);
        }

        // Run tests
        AppReturnCode code = AppReturnCode::NoError;
        if (auto test = Settings::Manager::instance().getGroupInstance("test")) {
            code = AppRunner::runTest(*test, code);
        } else {
            // If still no test group, create a minimal one via parseInput then get it
            QaplaHelpers::ConfigData testConfig;
            QaplaHelpers::IniFile::Section testSection;
            testSection.name = "test";
            testConfig.addSection(testSection);
            Settings::Manager::instance().parseInput(testConfig, true);
            
            if (auto forcedTest = Settings::Manager::instance().getGroupInstance("test")) {
                code = AppRunner::runTest(*forcedTest, code);
            } else {
                code = AppReturnCode::InvalidParameters;
            }
        }

        JsonValue::Object textContent;
        textContent["type"] = JsonValue{ .data = std::string("text") };
        textContent["text"] = JsonValue{ .data = std::format("Tests completed with code: {}", static_cast<int>(code)) };
        content.push_back(JsonValue{ .data = textContent });
        result["isError"] = JsonValue{ .data = (code != AppReturnCode::NoError) };
    } else {
        JsonValue::Object errorContent;
        errorContent["type"] = JsonValue{ .data = std::string("text") };
        errorContent["text"] = JsonValue{ .data = std::format("Unknown tool: {}", name) };
        content.push_back(JsonValue{ .data = errorContent });
        result["isError"] = JsonValue{ .data = true };
    }

    result["content"] = JsonValue{ .data = content };
    responseBody["result"] = JsonValue{ .data = result };
    response.data = responseBody;

    sendMessage(response);
}

void McpServer::silenceLoggers() {
    QaplaHelpers::ConfigData mcpConfig;
    QaplaHelpers::IniFile::Section loggingSection;
    loggingSection.name = "logging";
    loggingSection.addEntry("trace", "none");
    loggingSection.addEntry("engine", "false");
    mcpConfig.addSection(loggingSection);

    Settings::Manager::instance().parseInput(mcpConfig, true);
}

} // namespace QaplaTester::Mcp
