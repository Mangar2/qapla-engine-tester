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
#pragma once

#include "engine-worker-factory.h"
#include "uci-adapter.h"
#include "checklist.h"

std::unique_ptr<EngineWorker> EngineWorkerFactory::createEngineByName(const std::string& name) {
    auto engineConfig = configManager_.getConfig(name);
    if (!engineConfig) {
        throw std::runtime_error("Configuration for engine not found, name: " + name);
    }
    auto executablePath = engineConfig->getExecutablePath();
    auto workingDirectory = engineConfig->getWorkingDirectory();
    auto identifierStr = "#" + std::to_string(identifier_);
    auto adapter = std::make_unique<UciAdapter>(executablePath, workingDirectory, name, identifierStr);
    auto worker = std::make_unique<EngineWorker>(std::move(adapter), identifierStr, engineConfig->getOptionValues());
    identifier_++;
    return std::move(worker);
}

EngineList EngineWorkerFactory::createEnginesByName(const std::string& name, std::size_t count) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(count);
    constexpr int RETRY = 3;

    for (int retry = 0; retry < RETRY; retry++) {
        futures.clear();
        for (std::size_t i = 0; i < count; ++i) {
            // We initialize all engines in the first loop
            if (engines.size() <= i) {
                engines.push_back(createEngineByName(name));
                futures.push_back(engines.back()->getStartupFuture());
            }
            else if (engines[i]->failure()) {
                // The retry loops recreate engines having exceptions in the startup process
                engines[i] = createEngineByName(name);
                futures.push_back(engines[i]->getStartupFuture());
            }
        }
        // Wait for all newly created engines.
        for (auto& f : futures) {
            try {
                f.get();
            }
            catch (const std::exception& e) {
                Checklist::logCheck("Engine starts and stops fast and without problems", false, std::string(e.what()));
            }
            catch (...) {
                Checklist::logCheck("Engine starts and stops fast and without problems", false, "Unknown error");
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
