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

#include "engine-settings-helper.h"
#include "settings-manager.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../game-manager/game-manager-pool.h"
#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"

#include <format>

namespace QaplaTester::Settings::Helper {

namespace {

    /**
     * @brief Merges engine settings with global defaults and applies logging overrides.
     */
    [[nodiscard]] ValueMap determineFinalOptions(
        const GroupInstance& instance,
        const GroupInstance* eachDefault,
        const GroupInstance* globalLogging) {

        const GroupInstance merged = eachDefault != nullptr ? instance.mergeWithDefaults(*eachDefault) : instance;
        ValueMap options = merged.getValues();

        // Global logging policy can force engines to 'none' trace level
        if (globalLogging != nullptr && !globalLogging->get<bool>("engine")) {
            options["trace"] = std::string("none");
        }

        return options;
    }

    /**
     * @brief Adds an engine defined by a direct command line.
     */
    void addEngineByCommand(const ValueMap& options) {
        const auto config = EngineConfig::createFromValueMap(options);
        EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
    }

    /**
     * @brief Adds an engine by referencing an existing named configuration.
     */
    void addEngineByReference(const std::string& configName, const ValueMap& options) {
        const auto* baseConfig = EngineWorkerFactory::getConfigManager().getConfig(configName);
        if (baseConfig == nullptr) {
            throw AppError::makeInvalidParameters(std::format("Engine configuration '{}' not found.", configName));
        }

        auto config = *baseConfig;
        config.setCommandLineOptions(options, true);
        EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
    }

    /**
     * @brief Identifies the engine name for error reporting.
     */
    [[nodiscard]] std::string getEngineIdentifier(const ValueMap& options) {
        auto it = options.find("name");
        if (it != options.end()) {
            const auto& name = std::get<std::string>(it->second);
            if (!name.empty()) {
                return std::format(" (for {})", name);
            }
        }
        return "";
    }

    /**
     * @brief Registers an engine defined with id="config" in the ConfigManager.
     */
    void registerBaseConfiguration(
        const GroupInstance& instance,
        const GroupInstance* eachDefault,
        const GroupInstance* globalLogging) {

        const ValueMap options = determineFinalOptions(instance, eachDefault, globalLogging);
        const auto config = EngineConfig::createFromValueMap(options);
        EngineWorkerFactory::getConfigManagerMutable().getAllConfigsMutable().push_back(config);
    }

    /**
     * @brief Processes a single engine group instance from settings.
     */
    void processEngineInstance(
        const GroupInstance& instance,
        const GroupInstance* eachDefault,
        const GroupInstance* globalLogging) {

        const ValueMap options = determineFinalOptions(instance, eachDefault, globalLogging);
        
        // Priority 1: Direct command executable
        auto itCmd = options.find("cmd");
        if (itCmd != options.end() && !std::get<std::string>(itCmd->second).empty()) {
            addEngineByCommand(options);
            return;
        }

        // Priority 2: Named configuration reference
        auto itConf = options.find("conf");
        if (itConf != options.end() && !std::get<std::string>(itConf->second).empty()) {
            addEngineByReference(std::get<std::string>(itConf->second), options);
            return;
        }

        // Error if neither is provided
        throw AppError::makeInvalidParameters(
            std::format("No engine command or configuration provided{}. Please specify either 'cmd' or 'conf'.", 
                getEngineIdentifier(options)));
    }

} // namespace

bool applyEngineSettings(Manager& manager, const std::string& groupName) {
    const bool isRapid = manager.get<bool>("rapid");
    
    GameManagerPool::getInstance().setRapid(isRapid);
    EngineWorkerFactory::setRapid(isRapid);

    const auto engineInstances = manager.getGroupInstances(groupName);
    const auto eachDefault = manager.getGroupInstance("each");
    const auto globalLogging = manager.getGroupInstance("logging");

    // 1. First pass: Register base configurations (id="config")
    for (const auto& instance : engineInstances) {
        if (QaplaHelpers::to_lowercase(instance.get<std::string>("id")) == "config") {
            registerBaseConfiguration(
                instance,
                eachDefault.has_value() ? &(*eachDefault) : nullptr,
                globalLogging.has_value() ? &(*globalLogging) : nullptr);
        }
    }

    // 2. Second pass: Process active engines (id!="config")
    for (const auto& instance : engineInstances) {
        if (QaplaHelpers::to_lowercase(instance.get<std::string>("id")) != "config") {
            processEngineInstance(
                instance,
                eachDefault.has_value() ? &(*eachDefault) : nullptr,
                globalLogging.has_value() ? &(*globalLogging) : nullptr);
        }
    }

    EngineWorkerFactory::assignUniqueDisplayNames();
    
    return isRapid;
}

void addEnginesFromList(const std::string& engineNamesStr) {
    auto names = QaplaHelpers::split(engineNamesStr, ',');
    
    for (auto& name : names) {
        name = QaplaHelpers::trim(name);
        if (name.empty()) {
            continue;
        }

        const auto* config = EngineWorkerFactory::getConfigManager().getConfig(name);
        if (config == nullptr) {
            throw AppError::makeInvalidParameters(std::format("Engine configuration '{}' not found.", name));
        }
        
        EngineWorkerFactory::getActiveEnginesMutable().push_back(*config);
    }
    
    EngineWorkerFactory::assignUniqueDisplayNames();
}

} // namespace QaplaTester::Settings::Helper
