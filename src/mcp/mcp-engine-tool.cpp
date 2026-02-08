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

#include "mcp-engine-tool.h"
#include "mcp-converter.h"
#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"
#include "../engine-handling/engine-worker-factory.h"
#include <format>
#include <sstream>

namespace QaplaTester::Mcp {

JsonValue::Array McpEngineTool::handleManageEngines(const JsonValue::Object& arguments, QaplaConfiguration::EngineCapabilities& capabilities) {
    JsonValue::Array content;
    JsonValue::Object textContent;
    textContent["type"] = JsonValue{ .data = std::string("text") };

    const std::string command = arguments.at("command").asString();
    
    std::string result;
    if (command == "list") {
        result = listEngines();
    } else if (command == "details") {
        result = getEngineDetails(arguments, capabilities);
    } else if (command == "add") {
        result = addOrUpdateEngine(arguments, false, capabilities);
    } else if (command == "update") {
        result = addOrUpdateEngine(arguments, true, capabilities);
    } else if (command == "copy") {
        result = copyEngine(arguments);
    } else if (command == "update_all") {
        result = updateAllEngines(arguments, capabilities);
    } else {
        throw AppError::makeInvalidParameters(std::format("Unknown engine command '{}'.", command));
    }

    textContent["text"] = JsonValue{ .data = result };
    content.push_back(JsonValue{ .data = textContent });
    return content;
}

std::string McpEngineTool::listEngines() {
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

std::string McpEngineTool::getEngineDetails(const JsonValue::Object& arguments, const QaplaConfiguration::EngineCapabilities& capabilities) {
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
    details += formatSupportedOptions(config, capabilities);

    return details;
}

std::string McpEngineTool::addOrUpdateEngine(const JsonValue::Object& arguments, bool isUpdate, QaplaConfiguration::EngineCapabilities& capabilities) {
    if (!arguments.contains("engine_name") || arguments.at("engine_name").asString().empty()) {
        throw AppError::makeInvalidParameters("Engine 'engine_name' is required.");
    }
    if (!isUpdate && (!arguments.contains("engine_cmd") || arguments.at("engine_cmd").asString().empty())) {
        throw AppError::makeInvalidParameters("Engine 'engine_cmd' (path to executable) is required for adding an engine via MCP.");
    }
    const std::string name = arguments.at("engine_name").asString();

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
        capabilities.autoDetect();
        return std::format("Engine '{}' added successfully.", name);
    } 
    
    *config = newConfig;
    capabilities.autoDetect();
    return std::format("Engine '{}' updated successfully.", name);
}

std::string McpEngineTool::copyEngine(const JsonValue::Object& arguments) {
    if (!arguments.contains("engine_name") || !arguments.contains("engine_copyName")) {
        throw AppError::makeInvalidParameters("'engine_name' and 'engine_copyName' are required for 'copy' command.");
    }
    const std::string name = arguments.at("engine_name").asString();
    const std::string newName = arguments.at("engine_copyName").asString();
    
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

std::string McpEngineTool::updateAllEngines(const JsonValue::Object& arguments, QaplaConfiguration::EngineCapabilities& capabilities) {
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
    capabilities.autoDetect();
    return "All registered engines updated successfully.";
}

void McpEngineTool::setupActiveEngines(const JsonValue::Object& arguments) {
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
            
            const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
            if (config != nullptr) {
            activeEngines.push_back(*config);
            } else {
            throw AppError::makeInvalidParameters(std::format("Engine '{}' not found in registry.", name));
            }
    }
    
    EngineWorkerFactory::assignUniqueDisplayNames();
}

} // namespace QaplaTester::Mcp
