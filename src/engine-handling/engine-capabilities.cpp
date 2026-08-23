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
        std::optional<EngineProtocol> chosen;
        if (protocol) {
            chosen = *protocol;
        } else if (config.getProtocol() == EngineProtocol::Unknown
            || config.getProtocol() == EngineProtocol::NotSupported) {
            chosen = EngineProtocol::Uci;
        }
        if (!chosen) {
            continue;
        }

        // The local copy is what this round probes with, so it changes here and now. The stored
        // one is shared, so it changes wherever the owner of that data says -- and it is looked
        // up there rather than here, because a pointer fetched now could be stale by then.
        const auto command = config.getCmd();
        const auto previousProtocol = config.getProtocol();
        config.setProtocol(*chosen);
        applyChange([command, previousProtocol, chosen]() {
            auto* stored = EngineWorkerFactory::getConfigManagerMutable()
                .getConfigMutableByCmdAndProtocol(command, previousProtocol);
            if (stored != nullptr) {
                stored->setProtocol(*chosen);
            }
        });
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
        
        // Everything this engine answered, gathered here and written wherever the owner of that
        // data writes -- the engine object itself is gone by then, so nothing is read from it
        // inside the change.
        EngineCapability capability;
        capability.setPath(command);
        capability.setProtocol(protocol);
        capability.setName(engine->getEngineName());
        capability.setAuthor(engine->getEngineAuthor());
        capability.setSupportedOptions(engine->getSupportedOptions());

        applyChange([this, command, protocol, capability,
                        reportedName = engine->getEngineName(),
                        author = engine->getEngineAuthor()]() {
            auto* const config = EngineWorkerFactory::getConfigManagerMutable()
                .getConfigMutableByCmdAndProtocol(command, protocol);
            if (config != nullptr && !reportedName.empty()) {
                config->setReportedName(reportedName);
                config->setAuthor(author);
                // Also adopt it as the display name while that is still the executable's
                // filename: the reported name is deliberately not persisted (see
                // EngineConfig::toSection), so leaving the display name untouched here loses
                // the detected name again on the next save -- the engine reappears under its
                // filename even though detection had already resolved it.
                if (config->hasDefaultName()) {
                    config->setName(reportedName);
                }
            }
            addOrReplace(capability);
        });
    }
}

void EngineCapabilities::markAsNotSupported(const std::vector<EngineConfig>& failedConfigs) {
    std::string message = "Auto autodetection completed. Not supported Engine(s):\n";
    for (const auto& config : failedConfigs) {
        EngineCapability capability;
        capability.setPath(config.getCmd());
        capability.setProtocol(EngineProtocol::NotSupported);

        applyChange([this, command = config.getCmd(), protocol = config.getProtocol(),
                        capability]() {
            auto* mutableConfig = EngineWorkerFactory::getConfigManagerMutable()
                .getConfigMutableByCmdAndProtocol(command, protocol);
            if (mutableConfig != nullptr) {
                mutableConfig->setProtocol(EngineProtocol::NotSupported);
            }
            addOrReplace(capability);
        });

        message += std::format(" - {}\n", config.getCmd());
    }
    if (notificationCallback_) {
        notificationCallback_(message, "warning");
    }
}

void EngineCapabilities::autoDetectSync() {
    auto configs = collectMissingCapabilities();
    if (configs.empty()) {
        if (notificationCallback_) {
            notificationCallback_("No new engines found.", "note");
        }
        return;
    }
    if (notificationCallback_) {
        notificationCallback_("Starting engine autodetection...", "note");
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
}

void EngineCapabilities::autoDetect() {
    if (detecting_.exchange(true)) {
        return;
    }

    std::thread([this]() {
        autoDetectSync();
        // Through the same route as the changes themselves, and therefore behind them: whoever
        // waits for detection to be finished is waiting to find the results in place, not to
        // hear that they are on their way.
        applyChange([this]() {
            {
                std::lock_guard<std::mutex> lock(detectionMutex_);
                detecting_ = false;
            }
            detectionCv_.notify_all();
        });
    }).detach();
}

void EngineCapabilities::waitForDetection() const {
    std::unique_lock<std::mutex> lock(detectionMutex_);
    detectionCv_.wait(lock, [this] { return !detecting_.load(); });
}

void EngineCapabilities::shutdown() const {
    waitForDetection();
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
