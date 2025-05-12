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

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <iostream>

#include "engine-worker-factory.h"
#include "engine-group.h"
#include "game-manager.h"
#include "engine-checklist.h"
#include "engine-test-controller.h"

int main() {
    const std::filesystem::path enginePath = "C:\\Development\\qapla-engine-tester\\Qapla0.3.0-win-x86.exe";
	const std::filesystem::path stockfish = "C:\\Chess\\Engines\\stockfish-windows-x86-64-avx2\\stockfish\\stockfish-windows-x86-64-avx2.exe";
    const std::size_t engineCount = 1;

    std::cout << "[Startup] Waiting 3 seconds before test begins...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "[Startup] Starting test now.\n";

    EngineTestController controller;
	controller.runAllTests(enginePath);
    /*
    EngineWorkerFactory factory;
    std::cout << "Starting engines...\n";
    EngineGroup group(factory.createUci(stockfish, std::nullopt, engineCount));

    std::vector<std::unique_ptr<GameManager>> games;
    std::vector<std::future<void>> futures;

    for (auto& engine : group.engines_) {
        auto manager = std::make_unique<GameManager>(std::move(engine));
        manager->run(); // startet computeMove()
        futures.push_back(manager->getFinishedFuture());
        games.push_back(std::move(manager));
    }

    // Auf Abschluss aller Spiele warten
    for (auto& f : futures) {
        f.get();
    }

    for (auto& gm : games) {
        gm->stop();  
    }
    */
    
	EngineChecklist::print(std::cout);
    return 0;
}

