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

#include "../engine-handling/engine-worker-factory.h"
#include "../cli/settings-manager.h"
#include "../cli/app-runner.h"
#include "../engine-handling/engine-config.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/app-error.h"
#include "../base-elements/ini-file.h"

#include <format>
#include <sstream>

namespace QaplaTester::Mcp {

namespace {

    std::string jsonToString(const JsonValue& val) {
        if (val.isString()) {
            return val.asString();
        }
        // Convert bool to string "true"/"false" effectively
        if (val.isBool()) {
            return val.asBool() ? "true" : "false";
        }
        if (val.isNumber()) {
            double d = val.asDouble();
            // preserve integer representation if possible for cleanliness
            if (d == static_cast<double>(static_cast<long long>(d))) {
                return std::format("{}", static_cast<long long>(d));
            }
            return std::format("{}", d);
        }
        return ""; 
    }

    std::string getReadableLabel(const std::string& key) {
        if (key == "cmd") { return "Executable"; }
        if (key == "dir") { return "Working Directory"; }
        if (key == "proto") { return "Protocol"; }
        if (key == "tc") { return "Time Control"; }
        if (key == "args") { return "Arguments"; }
        if (key == "whitepov") { return "White POV"; }
        if (key == "originalName") { return "Reported Name"; }
        return key;
    }

    void populateSectionFromArgs(QaplaHelpers::IniFile::Section& section, const JsonValue::Object& arguments) {
        for (const auto& [key, val] : arguments) {
            // Mapping rules:
            // engine_name -> name
            // engine_option_OptionName -> option.OptionName
            // engine_Param -> Param
            
            std::string keyName;
            
            if (key == "engine_name" || key == "command") {
                continue;
            }
            
            if (key.starts_with("engine_option_")) {
                std::string optName = key.substr(14); // "engine_option_"
                keyName = "option." + optName;
            } else if (key.starts_with("engine_")) {
                keyName = key.substr(7); // "engine_"
            } else {
                continue;
            }
            
            if (!keyName.empty()) {
                section.addEntry(keyName, jsonToString(val));
            }
        }
    }
}

JsonValue::Array McpEngineTool::handleManageEngines(
    const JsonValue::Object& arguments, 
    QaplaConfiguration::EngineCapabilities& capabilities) 
{
    std::string command;
    if (const auto it = arguments.find("command"); it != arguments.end() && it->second.isString()) {
        command = it->second.asString();
    } else {
        throw AppError::makeInvalidParameters("Missing 'command' parameter in manage_engines.");
    }

    std::string output;

    if (command == "list") {
        output = listEngines();
    } else if (command == "details") {
        output = getEngineDetails(arguments, capabilities);
    } else if (command == "add") {
        output = addOrUpdateEngine(arguments, false, capabilities);
    } else if (command == "update") {
        output = addOrUpdateEngine(arguments, true, capabilities);
    } else if (command == "copy") {
        output = copyEngine(arguments);
    } else if (command == "update_all") {
        output = updateAllEngines(arguments, capabilities);
    } else {
        throw AppError::makeInvalidParameters(std::format("Unknown manage_engines command: {}", command));
    }

    JsonValue::Array content;
    JsonValue::Object textContent;
    textContent["type"] = JsonValue{ .data = std::string("text") };
    textContent["text"] = JsonValue{ .data = output };
    content.push_back(JsonValue{ .data = textContent });
    
    return content;
}

std::string McpEngineTool::listEngines() {
    // Read from the Registry (Single Source of Truth for *Available* Engines)
    // Note: Settings::Manager might have "active" engine configs, but the Registry holds the library.
    const auto& configs = EngineWorkerFactory::getConfigManager().getAllConfigs();
    if (configs.empty()) {
        return "No engines registered in the registry.";
    }

    std::string output = "Registered Engines:\n";
    for (const auto& config : configs) {
        output += std::format("- {}\n", config.getName());
    }
    return output;
}

std::string McpEngineTool::getEngineDetails(const JsonValue::Object& arguments, const QaplaConfiguration::EngineCapabilities& capabilities) {
    std::string name;
    if (const auto it = arguments.find("engine_name"); it != arguments.end() && it->second.isString()) {
        name = it->second.asString();
    } else {
        throw AppError::makeInvalidParameters("Command 'details' requires 'engine_name'.");
    }

    const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
    if (config == nullptr) {
        throw AppError::makeInvalidParameters(std::format("Engine '{}' not found.", name));
    }

    std::stringstream ss;
    ss << std::format("Details for '{}':\n", config->getName());
    
    config->visitProperties([&ss](const std::string& key, const std::string& value) {
        if (value.empty()) {
             return;
        }
        ss << std::format("  {}: {}\n", getReadableLabel(key), value);
    });
    
    const auto options = config->getOptionValues();
    if (!options.empty()) {
        ss << "  Configured Options:\n";
        for (const auto& [key, val] : options) {
            ss << std::format("    {} = {}\n", key, val);
        }
    }
    
    if (auto capability = capabilities.getCapability(config->getCmd(), config->getProtocol())) {
        const auto& availOpts = capability->getSupportedOptions();
        if (!availOpts.empty()) {
            ss << "  Available UCI Options:\n";
            for (const auto& opt : availOpts) {
                 ss << std::format("    {} (Type: {})\n", opt.name, EngineOption::to_string(opt.type));
            }
        }
    }

    return ss.str();
}

std::string McpEngineTool::addOrUpdateEngine(
    const JsonValue::Object& arguments, 
    bool isUpdate, 
    QaplaConfiguration::EngineCapabilities& capabilities) 
{
    std::string name;
    if (const auto it = arguments.find("engine_name"); it != arguments.end() && it->second.isString()) {
        name = it->second.asString();
    } else {
        throw AppError::makeInvalidParameters("Command requires 'engine_name'.");
    }

    // 1. Build configuration for Settings::Manager
    QaplaHelpers::IniFile::Section section;
    section.name = "engine"; 
    section.addEntry("name", name);
    section.addEntry("id", "config");
    populateSectionFromArgs(section, arguments);

    QaplaHelpers::ConfigData configData;
    configData.addSection(section);

    // 2. Push to Settings::Manager (Validation happens here)
    Settings::Manager::instance().parseInput(configData, true); // true = overwrite if exists

    syncToEngineRegistry(name);
    capabilities.autoDetect();

    return std::format("Engine '{}' {} successfully.", name, isUpdate ? "updated" : "added");
}

std::string McpEngineTool::copyEngine(const JsonValue::Object& arguments) {
    std::string srcName;
    if (const auto it = arguments.find("engine_name"); it != arguments.end() && it->second.isString()) {
        srcName = it->second.asString();
    } else {
        throw AppError::makeInvalidParameters("Command 'copy' requires 'engine_name'.");
    }

    std::string destName;
    if (const auto it = arguments.find("engine_copyName"); it != arguments.end() && it->second.isString()) {
        destName = it->second.asString();
    } else {
        throw AppError::makeInvalidParameters("Command 'copy' requires 'engine_copyName'.");
    }

    // Identify source
    QaplaHelpers::IniFile::Section search;
    search.name = "engine";
    search.addEntry("name", srcName);
    
    // Define modification (copy = change identity)
    QaplaHelpers::IniFile::Section replace;
    replace.addEntry("name", destName);
    
    // Perform Copy via Manager mechanism
    Settings::Manager::instance().modifyGroupInstance(search, replace);
    
    // Sync to Registry
    syncToEngineRegistry(destName);

    return std::format("Engine '{}' copied to '{}'.", srcName, destName);
}

std::string McpEngineTool::updateAllEngines(
    const JsonValue::Object& arguments, 
    QaplaConfiguration::EngineCapabilities& capabilities) 
{
     auto& configManager = EngineWorkerFactory::getConfigManagerMutable();
     auto engines = configManager.getAllConfigs(); // Copy list to iterate
     
     int count = 0;
     for (const auto& config : engines) {
         // Create a composite argument object for each engine
         JsonValue::Object specificArgs = arguments;
         specificArgs["engine_name"] = JsonValue{.data=config.getName()};
         
         // Reuse addOrUpdateEngine logic (updates Settings::Manager + Registry)
         try {
             addOrUpdateEngine(specificArgs, true, capabilities);
             count++;
         } catch (...) { // NOLINT(bugprone-empty-catch)
             // skip failures
         }
     }
     
     return std::format("Updated {} engines.", count);
}

void McpEngineTool::setupActiveEngines(const JsonValue::Object& arguments, Cli::TaskType taskType, QaplaConfiguration::EngineCapabilities& capabilities) {
    if (!arguments.contains("engines")) {
        return; 
    }
    
    std::string engineList;
    if (arguments.at("engines").isString()) {
        engineList = arguments.at("engines").asString();
    } else {
        return;
    }

    auto names = QaplaHelpers::split(engineList, ',');
    
    if (arguments.contains("engine_tc")) {
        applyGlobalTimeControl(names, arguments.at("engine_tc"), capabilities);
    }

    EngineWorkerFactory::getActiveEnginesMutable().clear();
    AppRunner::collectActiveEngines(taskType);
    
    if (EngineWorkerFactory::getActiveEngines().empty() && !names.empty()) {
        throw AppError::makeInvalidParameters("No valid engines could be activated from the list.");
    }
}


void McpEngineTool::applyGlobalTimeControl(
    const std::vector<std::string>& engineNames, 
    const JsonValue& tcValue, 
    QaplaConfiguration::EngineCapabilities& capabilities)
{
    for (const auto& rawName : engineNames) {
        std::string name = QaplaHelpers::trim(rawName);
        if (name.empty()) {
            continue;
        }

        JsonValue::Object updateArgs;
        updateArgs["engine_name"] = JsonValue{ .data = name };
        updateArgs["engine_tc"] = tcValue;
        
        // Use standard update mechanism
        addOrUpdateEngine(updateArgs, true, capabilities);
    }
}

void McpEngineTool::syncToEngineRegistry(const std::string& engineName) {
    auto instances = Settings::Manager::instance().getGroupInstances("engine");
    const Settings::GroupInstance* matchedInstance = nullptr;
    
    for (const auto& inst : instances) {
        try {
             if (inst.get<std::string>("name") == engineName) {
                 matchedInstance = &inst;
                 break;
             }
        } catch(...) { // NOLINT(bugprone-empty-catch)
            // ignore
        }
    }

    if (matchedInstance == nullptr) {
        throw AppError::make(1, AppReturnCode::GeneralError, "Failed to register engine '" + engineName + "' in Settings Manager.");
    }

    // Create EngineConfig and update Registry
    auto finalValues = matchedInstance->getValues();
    EngineConfig config = EngineConfig::createFromValueMap(finalValues);
    
    auto& configManager = EngineWorkerFactory::getConfigManagerMutable();
    
    // Check if updating existing or adding new
    if (auto* existing = configManager.getConfigMutable(config.getName())) {
        *existing = config;
    } else {
        configManager.addConfig(config);
    }
}

} // namespace QaplaTester::Mcp
