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
#include "job-scheduler.h"
#include "mcp-background-tools.h"
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

#include <sstream>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace QaplaTester::Mcp {

namespace {
    [[nodiscard]] std::string getToolPrefix() {
        const auto mcpGroup = Settings::Manager::instance().getGroupInstance("mcp");
        if (!mcpGroup.has_value()) {
            return "";
        }
        return mcpGroup->get<std::string>("prefix");
    }

    [[nodiscard]] std::string toPublishedToolName(std::string_view canonicalToolName) {
        const auto toolPrefix = getToolPrefix();
        if (toolPrefix.empty()) {
            return std::string(canonicalToolName);
        }
        return std::format("{}_{}", toolPrefix, canonicalToolName);
    }

    [[nodiscard]] std::string toCanonicalToolName(std::string_view requestedToolName) {
        const auto toolPrefix = getToolPrefix();
        if (toolPrefix.empty()) {
            return std::string(requestedToolName);
        }

        const auto requiredPrefix = std::format("{}_", toolPrefix);
        if (requestedToolName.starts_with(requiredPrefix)) {
            return std::string(requestedToolName.substr(requiredPrefix.length()));
        }

        return std::string(requestedToolName);
    }
}

void McpServer::initialize() {
    AppError::setDefaultInvalidParameterUserHint("Please refer to the tool definition schema for supported parameters.");
    silenceLoggers();

    // Set up MCP logging callback for report logger
    Logger::reportLogger().setMcpCallback([](Json::JsonValue payload, std::string_view toolName) {
        auto params = Json::JsonValue::object();
        params["level"] = "info";
        params["logger"] = toolName.empty() ? std::string("qapla") : std::string(toolName);
        params["data"] = std::move(payload);
        sendNotification("notifications/message", params);
    });

    // Set up MCP notification callback for engine autodetection
    QaplaConfiguration::EngineCapabilities::setNotificationCallback(
        [](const std::string& message, [[maybe_unused]] const std::string& type) {
        auto params = Json::JsonValue::object();
        params["level"] = "info";
        params["logger"] = "autodetect";
        params["data"] = message;
        sendNotification("notifications/message", params);
    });

    // Default MCP trace level to result
    Logger::reportLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::result);

    JobScheduler::instance().configure(
        [](const QueueJob& queuedJob) {
            McpEngineTool::setupActiveEngines(queuedJob.executionArguments, Cli::TaskType::All, capabilities_);
            Logger::logBaseName_ = queuedJob.reportBaseName;
            auto configData = queuedJob.configData;
            return executeRunnerTool(configData, false, Cli::getTaskType(queuedJob.toolName));
        },
        [](bool niceStop) {
            AppRunner::stop(niceStop);
        });
    JobScheduler::instance().start();
}

AppReturnCode McpServer::run() {

    while (true) {
        const auto message = messageChannel_.readMessage();
        if (!message.has_value()) {
            break; // EOF or error
        }

        if (!message->is_object()) {
            continue;
        }

        if (processMessage(message->as_object()) != AppReturnCode::NoError) {
            break;
        }
    }
    JobScheduler::instance().stop();
    capabilities_.shutdown();
    return AppReturnCode::NoError;
}

AppReturnCode McpServer::processMessage(const Json::JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("method")) {
        return AppReturnCode::NoError;
    }

    const std::string& method = jsonObject.at("method").as_string();

    if (method == "initialize") {
        // Respond to handshake
        auto response = Json::JsonValue::object();
        response["jsonrpc"] = "2.0";
        if (jsonObject.contains("id")) {
            response["id"] = jsonObject.at("id");
        }

        auto& result = response["result"] = Json::JsonValue::object();
        result["protocolVersion"] = "2024-11-05";

        auto& capabilities = result["capabilities"] = Json::JsonValue::object();
        capabilities["tools"] = Json::JsonValue::object();
        capabilities["resources"] = Json::JsonValue::object();

        auto& serverInformation = result["serverInfo"] = Json::JsonValue::object();
        serverInformation["name"] = "Qapla Engine Tester";
        serverInformation["version"] = "0.5.0";

        capabilities_.autoDetect();

        messageChannel_.sendMessage(response);
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

void McpServer::sendNotification(const std::string& method, const Json::JsonValue& params) {
    auto notification = Json::JsonValue::object();
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    notification["params"] = params;
    messageChannel_.sendMessage(notification);
}

void McpServer::listTools(const Json::JsonValue& requestId) {
    auto response = Json::JsonValue::object();
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;

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
            .name = "clop",
            .description = "Optimizes engine parameters using CLOP",
            .groups = {"clop", "clopvalue", "openings", "pgnoutput", "logging"}
        },
        {
            .name = "set_logging",
            .description = "Dynamically configures logging parameters. "
                           "SCOPE: "
                           "1. 'logging_path': Sets target directory for new logs. "
                           "2. 'logging_mcp': Sets MCP diagnostic verbosity (none|result|all). "
                           "3. 'logging_engine': (Bool) Toggles logging of CRITICAL engine events (crashes, illegal moves, disconnects) only. "
                           "CONSTRAINTS: "
                           "- Does NOT control raw UCI/XBoard protocol tracing (Use 'manage_engines' -> 'engine_trace'). "
                           "- Engine protocol traffic is NOT emitted via MCP (stored on disk only).",
            .groups = {"logging"}
        },
        {
            .name = "adjudicate",
            .description = "Configures global adjudication settings (Draw and Resign) for all tournaments.",
            .groups = {"draw", "resign"}
        },
        {
            .name = "control",
            .description = "Control running tasks and queued jobs (status, concurrency, stop, cancel, clear, list_results, clear_results). Queue-capable tools: sprt, tournament, epd, spsa, clop, test.",
            .groups = {}
        },
        {
            .name = "manage_engines",
            .description = "Manage engine configurations in the registry. Use this to manage the "
                        "engine registry. Supports individual management (list, details, add, "
                        "copy, update, delete) and bulk operations (update_all) for global settings "
                        "like time control or UCI options. IMPORTANT: When using 'copy', always provide "
                        "any modified parameters (like time control or options) in the same call to avoid extra steps.",
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

    auto& tools = response["result"]["tools"] = Json::JsonValue::array();
    for (const auto& info : toolsToRegister) {
        auto& tool = tools[tools.size()];
        tool["name"] = toPublishedToolName(info.name);

        // Use longDescription from main group if available
        std::string description(info.description);
        if (const auto it = groupDefs.find(std::string(info.name)); it != groupDefs.end() && !it->second.longDescription.empty()) {
            description = it->second.longDescription;
        }
        tool["description"] = description;
        tool["inputSchema"] = McpSchemaBuilder::createInputSchema(info, registeredNames);
    }

    messageChannel_.sendMessage(response);
}

void McpServer::callTool(const Json::JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("params") || !jsonObject.at("params").is_object()) {
        return;
    }
    const auto& params = jsonObject.at("params").as_object();
    if (!params.contains("name")) {
        return;
    }

    const std::string requestedName = params.at("name").as_string();
    const std::string name = toCanonicalToolName(requestedName);
    auto response = Json::JsonValue::object();
    response["jsonrpc"] = "2.0";
    if (jsonObject.contains("id")) {
        response["id"] = jsonObject.at("id");
    }

    auto& result = response["result"] = Json::JsonValue::object();
    auto content = Json::JsonValue::array();
    AppReturnCode returnCode = AppReturnCode::NoError;

    try {
        const auto& arguments = params.at("arguments").as_object();

        if (name == "read_report") {
            content = handleReadReport(arguments);
            result["isError"] = false;
        } else if (name == "control") {
            content = handleControlTool(arguments);
            result["isError"] = false;
        } else if (name == "manage_engines") {
            content = McpEngineTool::handleManageEngines(arguments, capabilities_);
            result["isError"] = false;
        } else if (name == "set_logging") {
            content = handleSetLogging(arguments);
            result["isError"] = false;
        } else if (name == "adjudicate") {
            content = handleAdjudicateTool(arguments);
            result["isError"] = false;
        } else if (name == "list_settings") {
            content = handleListSettings(arguments);
            result["isError"] = false;
        } else {
            // Handle active list and execution
            const Cli::TaskType taskType = Cli::TaskType::All;

            const bool deferEngineSetupToQueue =
                isQueueableTool(name) && isBackgroundRequested(arguments);
            if (!deferEngineSetupToQueue) {
                McpEngineTool::setupActiveEngines(arguments, taskType, capabilities_);
            }
            content = runRunnerTool(name, arguments, returnCode);

            result["isError"] = (returnCode == AppReturnCode::GeneralError ||
                                  returnCode == AppReturnCode::InvalidParameters ||
                                  returnCode == AppReturnCode::EngineError ||
                                  returnCode == AppReturnCode::EngineMissbehaviour);
        }
    } catch (const std::exception& e) {
        content = Json::JsonValue::array();
        auto& errorContent = content[0U];
        errorContent["type"] = "text";
        errorContent["text"] = std::format("Error executing tool '{}': {}", requestedName, e.what());
        result["isError"] = true;
        returnCode = AppReturnCode::GeneralError;
    }

    result["content"] = std::move(content);

    messageChannel_.sendMessage(response);
}

QaplaHelpers::ConfigData McpServer::mapJsonToConfigData(
    const Json::JsonValue::Object& arguments, const std::string& defaultId)
{
    QaplaHelpers::ConfigData configData;
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section> sections;

    // Process all arguments into globals or sections
    for (const auto& [key, value] : arguments) {
        processParameter(key, value, sections, configData);
    }

    // Post-process sections: ensure IDs exist and add to config
    for (auto& [name, section] : sections) {
        if (name == "engine") {
            // Engines are managed by the engine tool, we only get names here.
            continue;
        }
        const bool hasId = std::ranges::any_of(section.entries, [](const auto& entry) {
            return entry.first == "id";
        });

        if (!hasId) {
            section.addEntry("id", defaultId.empty() ? "all" : defaultId);
        }

        configData.addSection(section);
    }

    return configData;
}

namespace {
    void ensureTaskGroupSection(QaplaHelpers::ConfigData& configData, std::string_view toolName) {
        if (!isQueueableTool(toolName)) {
            return;
        }

        if (configData.getSectionMap(std::string(toolName)).has_value()) {
            return;
        }

        QaplaHelpers::IniFile::Section section;
        section.name = std::string(toolName);
        section.addEntry("id", "all");
        configData.addSection(section);
    }
}

void McpServer::processParameter(const std::string& key, const Json::JsonValue& value,
    std::unordered_map<std::string, QaplaHelpers::IniFile::Section>& otherGroupedSections,
    QaplaHelpers::ConfigData& configData) {

    if (value.is_array()) {
        for (const auto& item : value.as_array()) {
            if (item.is_object()) {
                QaplaHelpers::IniFile::Section s;
                s.name = key;
                for (const auto& [propKey, propVal] : item.as_object()) {
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

void McpServer::listResources(const Json::JsonValue& requestId) {
    auto response = Json::JsonValue::object();
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;

    auto& resources = response["result"]["resources"] = Json::JsonValue::array();

    std::error_code ec;
    if (!BaseLogger::logPath_.empty() && std::filesystem::exists(BaseLogger::logPath_, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(BaseLogger::logPath_, ec)) {
            if (ec) {
                break;
            }
            addResourceIfValid(entry, resources);
        }
    }

    messageChannel_.sendMessage(response);
}

void McpServer::addResourceIfValid(const std::filesystem::directory_entry& entry, Json::JsonValue& resources) {
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

    auto& resource = resources[resources.size()];
    resource["uri"] = std::format("qapla://reports/{}/{}", tool, filename);
    resource["name"] = filename;
    resource["description"] = std::format("{} result for tool {}", isPgn ? "PGN" : "Log", tool);
    resource["mimeType"] = isPgn ? "text/x-chess-pgn" : "text/plain";
}

Json::JsonValue McpServer::handleListSettings(const Json::JsonValue::Object& arguments) {
    std::optional<std::vector<std::string>> groups;

    if (arguments.contains("groups") && arguments.at("groups").is_array()) {
        const auto& groupArray = arguments.at("groups").as_array();
        std::vector<std::string> groupList;
        for (const auto& g : groupArray) {
            if (g.is_string()) {
                groupList.push_back(g.as_string());
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

    auto content = Json::JsonValue::array();
    auto& contentObj = content[0U];
    contentObj["type"] = "text";
    contentObj["text"] = report;
    return content;
}

std::string McpServer::extractToolName(std::string_view filename) {
    constexpr std::array prefixes = {
        std::string_view("report-"),
        std::string_view("engine-"),
        std::string_view("sprt-"),
        std::string_view("tournament-"),
        std::string_view("epd-"),
        std::string_view("spsa-"),
        std::string_view("clop-")
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

void McpServer::readResource(const Json::JsonValue::Object& jsonObject) {
    if (!jsonObject.contains("params") || !jsonObject.at("params").is_object()) {
        return;
    }
    const auto& params = jsonObject.at("params").as_object();
    if (!params.contains("uri")) {
        return;
    }

    const std::string& uri = params.at("uri").as_string();

    auto response = Json::JsonValue::object();
    response["jsonrpc"] = "2.0";
    if (jsonObject.contains("id")) {
        response["id"] = jsonObject.at("id");
    }

    auto& contents = response["result"]["contents"] = Json::JsonValue::array();

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

                    auto& content = contents[contents.size()];
                    content["uri"] = uri;
                    content["text"] = buffer.str();
                }
            }
        }
    }

    messageChannel_.sendMessage(response);
}

Json::JsonValue McpServer::handleReadReport(const Json::JsonValue::Object& arguments) {
    if (!arguments.contains("uri")) {
        throw std::runtime_error("Could not read report file: URI missing.");
    }

    const std::string& uri = arguments.at("uri").as_string();
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

                auto content = Json::JsonValue::array();
                auto& textContent = content[0U];
                textContent["type"] = "text";
                textContent["text"] = buffer.str();
                return content;
            }
        }
    }
    throw std::runtime_error("Could not read report file: file not found.");
}

AppReturnCode McpServer::executeRunnerTool(QaplaHelpers::ConfigData& configData,
    bool background, Cli::TaskType forcedTask) {

    Settings::QaplaSettings::instance().applyConfig(configData);

    // Run dispatcher
    return AppRunner::instance().runDispatcher(background, forcedTask);
}

std::string McpServer::formatRunSummary(const std::string& name, AppReturnCode code) {
    std::string summary = std::format("Tool '{}' finished. Result: {}", name, appReturnCodeResultText(code));
    const std::string reportFilename = std::filesystem::path(Logger::reportLogger().getFilename()).filename().string();
    if (!reportFilename.empty()) {
        summary += std::format("\nReport Log Resource: qapla://reports/{}/{}", name, reportFilename);
    }
    return summary;
}

Json::JsonValue McpServer::handleAdjudicateTool(const Json::JsonValue::Object& arguments) {
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

    auto content = Json::JsonValue::array();
    auto& textContent = content[0U];
    textContent["type"] = "text";
    textContent["text"] = "Global adjudication settings updated.";
    return content;
}

Json::JsonValue McpServer::handleControlTool(const Json::JsonValue::Object& arguments) {
    const std::string command = arguments.at("command").as_string();

    std::string result;

    if (command == "status") {
        result = createCombinedControlStatus().stringify();
    } else if (command == "set_concurrency") {
        if (!arguments.contains("value") || !arguments.at("value").is_number()) {
             throw AppError::makeInvalidParameters("Integer value required for set_concurrency.");
        }
        const int value = static_cast<int>(arguments.at("value").as_number());
        AppRunner::setConcurrency(value);
        const auto status = createCombinedControlStatus().stringify();
        result = std::format("Concurrency set to {}.\nStatus: {}", value, status);
    } else if (command == "stop") {
        const auto activeCanceled = JobScheduler::instance().requestCancelActive(false);
        AppRunner::stop(false);
        const auto clearedCount = JobScheduler::instance().clearQueuedJobs();
        result = std::format("All tasks stopped. Active queued-run canceled: {}. Cleared {} queued jobs.",
            activeCanceled ? "yes" : "no", clearedCount);
    } else if (command == "stop_nice") {
        const auto activeCanceled = JobScheduler::instance().requestCancelActive(true);
        AppRunner::stop(true);
        const auto clearedCount = JobScheduler::instance().clearQueuedJobs();
        result = std::format("All tasks stopped gracefully. Active queued-run canceled: {}. Cleared {} queued jobs.",
            activeCanceled ? "yes" : "no", clearedCount);
    } else if (command == "cancel_job") {
        if (!arguments.contains("job_id") || !arguments.at("job_id").is_string()) {
            throw AppError::makeInvalidParameters("String job_id required for cancel_job.");
        }

        const auto& jobId = arguments.at("job_id").as_string();
        const auto canceled = JobScheduler::instance().cancelJob(jobId, true);
        result = canceled
            ? std::format("Cancel requested for job '{}'.", jobId)
            : std::format("Job '{}' not found.", jobId);
    } else if (command == "clear_queue") {
        const auto clearedCount = JobScheduler::instance().clearQueuedJobs();
        result = std::format("Cleared {} queued jobs.", clearedCount);
    } else if (command == "list_results") {
        result = JobScheduler::instance().finishedResultsJson().stringify();
    } else if (command == "clear_results") {
        const auto clearedCount = JobScheduler::instance().clearFinishedResults();
        result = std::format("Cleared {} finished queue results.", clearedCount);
    } else {
         throw AppError::makeInvalidParameters(std::format("Unknown control command '{}'.", command));
    }

    auto content = Json::JsonValue::array();
    auto& textContent = content[0U];
    textContent["type"] = "text";
    textContent["text"] = result;
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
    if (name == "clop") {
        return {"", "clop-report"};
    }
    if (name == "control") {
        return {"", "control-report"};
    }
    return {"", "report"};
}

void McpServer::prepareTaskFile(const std::string& name, Json::JsonValue::Object& toolArgs) {
    if (name == "sprt") {
        bool resume = false;
        if (toolArgs.contains("resume")) {
            resume = toolArgs.at("resume").as_boolean();
            toolArgs.erase("resume");
        }

        std::string filename;
        if (resume && !lastSprtFile_.empty()) {
            filename = lastSprtFile_;
        } else {
             filename = QaplaHelpers::generateTimestampedFilename("sprt-tournament", BaseLogger::logPath_, "qsprt");
             lastSprtFile_ = filename;
        }
        toolArgs["sprt_file"] = filename;
    } else if (name == "tournament") {
        bool resume = false;
        if (toolArgs.contains("resume")) {
            resume = toolArgs.at("resume").as_boolean();
            toolArgs.erase("resume");
        }

        std::string filename;
        if (resume && !lastTournamentFile_.empty()) {
            filename = lastTournamentFile_;
        } else {
             filename = QaplaHelpers::generateTimestampedFilename("tournament-outcome", BaseLogger::logPath_, "qtour");
             lastTournamentFile_ = filename;
        }
        toolArgs["tournament_file"] = filename;
    }
}

Json::JsonValue McpServer::runRunnerTool(const std::string& name, const Json::JsonValue::Object& arguments,
    AppReturnCode& returnCode) {
    auto toolArgs = arguments;  // Create a copy to modify
    bool background = false;

    // Check for background execution parameter
    if (toolArgs.contains("mcp_background")) {
        background = toolArgs.at("mcp_background").as_boolean();
        toolArgs.erase("mcp_background"); // Remove so it doesn't fail settings parsing
    } else if (toolArgs.contains("background")) {
        background = toolArgs.at("background").as_boolean();
        toolArgs.erase("background");
    }

    if (toolArgs.contains("engines")) {
        toolArgs.erase("engines");
    }

    const auto jobIntent = extractJobIntentForQueue(name, toolArgs, background);

    prepareTaskFile(name, toolArgs);

    auto [_, reportBaseName] = getTaskConfigInfo(name);

    Logger::logBaseName_ = reportBaseName;

    auto paramsConfig = mapJsonToConfigData(toolArgs);
    ensureTaskGroupSection(paramsConfig, name);
    QaplaHelpers::ConfigData configData = globalAdjudicationConfig_;

    // Copy paramsConfig into configData
    mergeGlobalConfig(configData, paramsConfig);

    auto content = Json::JsonValue::array();
    auto& textContent = content[0U];
    textContent["type"] = "text";

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
    } else if (name == "clop") {
        reportGroups.insert(reportGroups.end(), { "clop", "clopvalue", "openings", "pgnoutput" });
    } else if (name == "test") {
        reportGroups.insert(reportGroups.end(), { "test" });
    }

    // Show limited columns for run summary (Name and Value mainly)
    std::vector<SettingsReporter::Column> reportColumns = {
        SettingsReporter::Column::FullName,
        SettingsReporter::Column::Value
    };

    const auto generateSettingsReport = [&reportGroups, &reportColumns]() {
        return SettingsReporter::generateReport(reportGroups, reportColumns);
    };

    if (background && isQueueableTool(name)) {
        QueueJob queueEntry;
        queueEntry.jobType = queueJobTypeForTool(name);
        queueEntry.toolName = name;
        queueEntry.jobIntent = jobIntent;
        queueEntry.reportBaseName = reportBaseName;
        queueEntry.configData = configData;
        queueEntry.executionArguments = arguments;

        const auto jobId = JobScheduler::instance().enqueue(std::move(queueEntry));
        returnCode = AppReturnCode::NoError;

        const auto summary = createQueueStartSummary(name, jobId, JobScheduler::instance().queueStatusJson());
        const auto settingsReport = generateSettingsReport();
        textContent["text"] = summary + "\n\n" + settingsReport;
    } else if (background) {
        returnCode = executeRunnerTool(configData, background, Cli::getTaskType(name));
        std::string summary = std::format("Tool '{}' started in background.", name);
        summary += std::format("\nReport Log might be available at: qapla://reports/{}/<timestamped_file>", name);
        const auto settingsReport = generateSettingsReport();
        textContent["text"] = summary + "\n\n" + settingsReport;
    } else {
        returnCode = executeRunnerTool(configData, background, Cli::getTaskType(name));
        std::string runSummary = formatRunSummary(name, returnCode);
        const auto settingsReport = generateSettingsReport();
        textContent["text"] = runSummary + "\n\n" + settingsReport;
    }

    return content;
}

Json::JsonValue McpServer::handleSetLogging(const Json::JsonValue::Object& arguments) {
    auto configData = mapJsonToConfigData(arguments);
    Settings::Manager::instance().parseInput(configData, true);

    // 1. MCP Logging Level
    if (arguments.contains("logging_mcp")) {
        const auto& val = arguments.at("logging_mcp");
        if (val.is_string()) {
            const std::string lvl = val.as_string();
            TraceLevel mcpLevel = TraceLevel::result;
            if (lvl == "all") {
                mcpLevel = TraceLevel::info;
            } else if (lvl == "none") {
                mcpLevel = TraceLevel::none;
            }
            Logger::reportLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, mcpLevel);
        }
    }

    // 2. Engine Logging
    if (arguments.contains("logging_engine")) {
        bool enable = true;
        const auto& val = arguments.at("logging_engine");

        if (val.is_boolean()) {
            enable = val.as_boolean();
        } else if (val.is_string()) {
            enable = (val.as_string() == "true");
        }

        if (enable) {
             EngineLogger::engineLogger().setTraceLevel(TraceLevel::none, TraceLevel::info, TraceLevel::error);
        } else {
             EngineLogger::engineLogger().setTraceLevel(TraceLevel::none, TraceLevel::none, TraceLevel::none);
        }
    }

    // 3. Log Path - already handled by mapJsonToConfigData?
    // mapJsonToConfigData handles generic keys. logging_path -> [logging] path.
    // But BaseLogger::logPath_ static member needs explicit update if not observing settings.
    if (arguments.contains("logging_path")) {
        const auto& val = arguments.at("logging_path");
        if (val.is_string()) {
             BaseLogger::logPath_ = val.as_string();
        }
    }

    auto content = Json::JsonValue::array();
    auto& textContent = content[0U];
    textContent["type"] = "text";
    textContent["text"] = "Logging configuration updated.";
    return content;
}

} // namespace QaplaTester::Mcp
