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

EngineList EngineWorkerFactory::createUci(
    const std::filesystem::path& executablePath,
    std::optional<std::filesystem::path> workingDirectory,
    std::size_t count)
{
    std::vector<std::unique_ptr<EngineWorker>> engines;
    engines.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        auto identifierStr = "#" + std::to_string(identifier_);
        auto adapter = std::make_unique<UciAdapter>(executablePath, workingDirectory, identifierStr);
        auto worker = std::make_unique<EngineWorker>(std::move(adapter), identifierStr);
        engines.push_back(std::move(worker));
        identifier_++;
    }
    std::vector<std::future<void>> futures;
    futures.reserve(count);
    for (auto& worker : engines) {
        futures.push_back(worker->getStartupFuture());
    }
    for (auto& f : futures) {
        f.get(); // blockiert bis Engine fertig
    }
    return engines;
}

EngineList EngineWorkerFactory::createEnginesByName(const std::string& name, std::size_t count) {
    std::vector<std::unique_ptr<EngineWorker>> engines;
    engines.reserve(count);
    auto engineConfig = configManager_.getConfig(name);
    auto executablePath = engineConfig->getExecutablePath();
	auto workingDirectory = engineConfig->getWorkingDirectory();

    for (std::size_t i = 0; i < count; ++i) {
        auto identifierStr = "#" + std::to_string(identifier_);
        auto adapter = std::make_unique<UciAdapter>(executablePath, workingDirectory, identifierStr);
        auto worker = std::make_unique<EngineWorker>(std::move(adapter), identifierStr);
        engines.push_back(std::move(worker));
        identifier_++;
    }
    std::vector<std::future<void>> futures;
    futures.reserve(count);
    for (auto& worker : engines) {
        futures.push_back(worker->getStartupFuture());
    }
    for (auto& f : futures) {
        f.get(); // blockiert bis Engine fertig
    }
    return engines;
}
