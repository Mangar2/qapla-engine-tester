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

void EngineWorkerFactory::assignUniqueDisplayNames() {
    auto& engines = getActiveEnginesMutable();
    std::unordered_map<std::string, std::vector<std::size_t>> nameGroups;

    std::vector<std::unordered_map<std::string, std::string>> disambiguationMaps;
    for (const auto& engine : getActiveEngines()) {
        disambiguationMaps.push_back(engine.toDisambiguationMap());
    }

    for (std::size_t i = 0; i < disambiguationMaps.size(); ++i) {
        const auto& map = disambiguationMaps[i];
        auto it = map.find("name");
        const std::string& baseName = (it != map.end()) ? it->second : "unnamed";
        nameGroups[baseName].push_back(i);
    }

	// Assign unique names to engines with the same base name
    for (const auto& [baseName, indices] : nameGroups) {
        if (indices.size() == 1)
            continue;

        for (std::size_t index : indices) {
            std::string name =  "[";

            std::string separator = "";
            for (const auto& [key, value] : disambiguationMaps[index]) {
                if (key == "name") continue;

                for (std::size_t i : indices) {
                    const auto& map = disambiguationMaps[i];
                    auto it = map.find(key);
                    if (it == map.end() || it->second != value) {
                        name += separator + key;
                        if (!value.empty())
                            name += "=" + value;
                        separator = ", ";
                        break;
                    }
                }
            }

            name += "]";
            if (name != "[]") {
                engines[index].setName(baseName + " " + name);
            }
        }
    }
}


std::unique_ptr<EngineWorker> EngineWorkerFactory::createEngine(const EngineConfig& config) {
    auto executablePath = config.getCmd();
    auto workingDirectory = config.getDir();
    auto name = config.getName();
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

EngineList EngineWorkerFactory::createEngines(const EngineConfig& config, std::size_t count) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(count);
    constexpr int RETRY = 3;
	EngineReport* checklist = EngineReport::getChecklist(config.getName());
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
        for (auto& f : futures) {
            try {
                f.get();
            }
            catch (const std::exception& e) {
                checklist->logReport("starts-and-stops-cleanly", false, std::string(e.what()));
            }
            catch (...) {
                checklist->logReport("starts-and-stops-cleanly", false, "Unknown error");
            }
        }
    }

    EngineList runningEngines;
    for (auto& engine : engines) {
        if (!engine->failure()) {
            runningEngines.push_back(std::move(engine));
        }
    }
    return runningEngines;
}
