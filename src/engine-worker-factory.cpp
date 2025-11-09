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

#include "engine-worker-factory.h"
#include "uci-adapter.h"
#include "winboard-adapter.h"
#include "engine-report.h"
#include "engine-config-manager.h"

namespace QaplaTester {

void EngineWorkerFactory::assignUniqueDisplayNames() {
    auto& engines = getActiveEnginesMutable();
    EngineConfigManager::assignUniqueDisplayNames(engines);
}


std::unique_ptr<EngineWorker> EngineWorkerFactory::createEngine(const EngineConfig& config) {
    auto executablePath = config.getCmd();
    auto workingDirectory = config.getDir();
    auto identifierStr = "#" + std::to_string(identifier_);
    std::unique_ptr<EngineAdapter> adapter;
    if (config.getProtocol() == EngineProtocol::Uci) {
		adapter = std::make_unique<UciAdapter>(executablePath, workingDirectory, identifierStr);
	}
    else if (config.getProtocol() == EngineProtocol::XBoard) {
        adapter = std::make_unique<WinboardAdapter>(executablePath, workingDirectory, identifierStr);
    } 
    else {
        throw AppError::makeInvalidParameters("Unsupported engine protocol: " + to_string(config.getProtocol()));
    }
    adapter->setSuppressInfoLines(suppressInfoLines_);
    auto worker = std::make_unique<EngineWorker>(std::move(adapter), identifierStr, config);
    identifier_++;
    return std::move(worker);
}

std::unique_ptr<EngineWorker> EngineWorkerFactory::restart(const EngineWorker& worker) {
	return createEngine(worker.getConfig());
}

void EngineWorkerFactory::waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, 
    const EngineConfig& config) {
    auto* checklist = EngineReport::getChecklist(config.getName());
    for (auto& f : futures) {
        try {
            f.get();
        }
        catch (const std::exception& error) {
            checklist->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
        }
        catch (...) {
            checklist->logReport("starts-and-stops-cleanly", false, "Unknown error");
        }
    }
}

void EngineWorkerFactory::waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, 
    const std::vector<EngineConfig>& configs) {
    uint32_t index = 0;
    for (auto& f : futures) {
        EngineReport* checklist;
        const auto& name = configs[index].getName();
        try {
            f.get();
        }
        catch (const std::exception& error) {
            if (!name.empty()) {
                checklist = EngineReport::getChecklist(name);
                checklist->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
            }
        }
        catch (...) {
            if (!name.empty()) {
                checklist = EngineReport::getChecklist(name);
                checklist->logReport("starts-and-stops-cleanly", false, "Unknown error");
            }
        }
        index++;
    }
}

EngineList EngineWorkerFactory::createEngines(const EngineConfig& config, std::size_t count) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(count);
    constexpr int RETRY = 3;
    for (int retry = 0; retry < RETRY; retry++) {
        futures.clear();
        for (std::size_t i = 0; i < count; ++i) {
            // We initialize all engines in the first loop
            if (engines.size() <= i) {
                engines.push_back(createEngine(config));
                futures.push_back(engines.back()->getStartupFuture());
            }
            else if (engines[i]->failure()) {
                // The retry loops recreate engines having exceptions in the startup process
                engines[i] = createEngine(config);
                futures.push_back(engines[i]->getStartupFuture());
            }
        }
        // Wait for all newly created engines.
        waitUntilAllEnginesStarted(futures, config);
    }

    EngineList runningEngines;
    for (auto& engine : engines) {
        if (!engine->failure()) {
            runningEngines.push_back(std::move(engine));
        }
    }
    return runningEngines;
}

EngineList EngineWorkerFactory::createEngines(const std::vector<EngineConfig>& configs, bool noWait) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(configs.size());
    constexpr int RETRY = 3;
    for (int retry = 0; retry < RETRY; retry++) {
        futures.clear();
        uint32_t index = 0;
        for (const auto& config: configs) {
            // We initialize all engines in the first loop
            if (engines.size() <= index) {
                try {
                    engines.push_back(createEngine(config));
                    futures.push_back(engines.back()->getStartupFuture());
                } 
                catch (const std::exception& error) {
                    EngineReport::getChecklist(config.getName())
                        ->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
                }
            }
            else if (engines[index]->failure()) {
                // The retry loops recreate engines having exceptions in the startup process
                engines[index] = createEngine(config);
                futures.push_back(engines[index]->getStartupFuture());
            }
            index++;
        }
        if (noWait) {
            // If noWait is true, we skip waiting for the startup futures.
            // This allows engines to start asynchronously.
            continue;
        }
        // Wait for all newly created engines.
        waitUntilAllEnginesStarted(futures, configs);
    }    
    EngineList runningEngines;
    for (auto& engine : engines) {
        if (noWait || !engine->failure()) {
            runningEngines.push_back(std::move(engine));
        }
    }
    return runningEngines;
}

} // namespace QaplaTester
