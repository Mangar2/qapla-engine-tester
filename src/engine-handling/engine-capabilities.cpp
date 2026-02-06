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

#include "engine-capabilities.h"

#include "engine-config.h"
#include "engine-config-manager.h"
#include "engine-worker-factory.h"

#include <thread>
#include <format>

using QaplaTester::EngineConfig;
using QaplaTester::EngineWorkerFactory;
using QaplaTester::EngineWorker;
using QaplaTester::EngineProtocol;

namespace QaplaConfiguration {

std::vector<EngineConfig> EngineCapabilities::collectMissingCapabilities() const {
    std::vector<EngineConfig> configs;
    for (auto& config : EngineWorkerFactory::getConfigManager().getAllConfigs()) {
        if (!hasAnyCapability(config.getCmd(), config.getProtocol())) {
            configs.push_back(config);
        }
    }
    return configs;
}

std::vector<EngineConfig> EngineCapabilities::detectWithProtocol(
    std::vector<EngineConfig>& configs,
    std::optional<EngineProtocol> protocol) 
{
    for (auto& config : configs) {
        auto* mutableConfig = EngineWorkerFactory::getConfigManagerMutable()
            .getConfigMutableByCmdAndProtocol(config.getCmd(), config.getProtocol());
        if (protocol) {
            config.setProtocol(*protocol);
            mutableConfig->setProtocol(*protocol);
        }
    }
    
    auto engines = EngineWorkerFactory::createEngines(configs);
    
    std::vector<EngineConfig> failedConfigs;
    for (const auto& config : configs) {
        auto matchingEngine = std::ranges::find_if(engines,
            [&config](const std::unique_ptr<EngineWorker>& engine) {
                return engine->getConfig().getCmd() == config.getCmd() &&
                       engine->getConfig().getProtocol() == config.getProtocol();
            });
        
        if (matchingEngine == engines.end()) {
            failedConfigs.push_back(config);
        }
    }
    
    storeCapabilities(engines);
    
    return failedConfigs;
}

void EngineCapabilities::storeCapabilities(const std::vector<std::unique_ptr<EngineWorker>>& engines) {
    for (const auto& engine : engines) {
        const auto& command = engine->getConfig().getCmd();
        auto protocol = engine->getConfig().getProtocol();
        
        // Update the config manager with engine name and author
        auto* const config = EngineWorkerFactory::getConfigManagerMutable()
            .getConfigMutableByCmdAndProtocol(command, protocol);
        if (config != nullptr && !engine->getEngineName().empty()) {
            config->setName(engine->getEngineName());
            config->setAuthor(engine->getEngineAuthor());
        }
        
        // Create and store capability
        EngineCapability capability;
        capability.setPath(command);
        capability.setProtocol(protocol);
        capability.setName(engine->getEngineName());
        capability.setAuthor(engine->getEngineAuthor());
        capability.setSupportedOptions(engine->getSupportedOptions());
        addOrReplace(capability);
    }
}

void EngineCapabilities::markAsNotSupported(const std::vector<EngineConfig>& failedConfigs) {
    std::string message = "Auto autodetection completed. Not supported Engine(s):\n";
    for (const auto& config : failedConfigs) {
        auto* mutableConfig = EngineWorkerFactory::getConfigManagerMutable()
            .getConfigMutableByCmdAndProtocol(config.getCmd(), config.getProtocol());
        if (mutableConfig != nullptr) {
            mutableConfig->setProtocol(EngineProtocol::NotSupported);
        }
        message += std::format(" - {}\n", config.getCmd());
    }
    if (notificationCallback_) {
        notificationCallback_(message, "warning");
    }
}

void EngineCapabilities::autoDetect() {
    std::thread([this]() {
        auto configs = collectMissingCapabilities();
        if (configs.empty()) {
            if (notificationCallback_) {
                notificationCallback_("No new engines found.", "note");
            }
            return;
        }
        detecting_ = true;
        if (notificationCallback_) {
            notificationCallback_("Starting engine autodetection.\nThis may take a while...", "note");
        }
        // First try with the protocol already set in the config
        configs = detectWithProtocol(configs, std::nullopt);

        // Detect using UCI protocol first then xboard as uci is more common
        for (const auto protocol : {EngineProtocol::Uci, EngineProtocol::XBoard}) {
            configs = detectWithProtocol(configs, protocol);
            if (configs.empty()) {
                break;
            }
        }
        if (!configs.empty()) {
            markAsNotSupported(configs);
        } else {
            if (notificationCallback_) {
                notificationCallback_("Engine autodetection completed.", "success");
            }
        }
        detecting_ = false;
    }).detach();
}

bool EngineCapabilities::areAllEnginesDetected() const {
    auto allDetected = std::ranges::all_of(EngineWorkerFactory::getConfigManager().getAllConfigs(), 
        [this](const auto& config) 
    {
        return hasAnyCapability(config.getCmd(), config.getProtocol());
    });
    return allDetected;
}

} // namespace QaplaConfiguration
