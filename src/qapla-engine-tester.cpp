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

int main() {
    const std::filesystem::path enginePath = "C:\\Development\\qapla-engine-tester\\Qapla0.3.0-win-x86.exe";
    const std::size_t engineCount = 1000;

    EngineWorkerFactory factory;
    std::cout << "Starting engines...\n";
    EngineGroup group(factory.createUci(enginePath, std::nullopt, engineCount));

	//GameManager gameManager(std::move(group.engines_[0]));  
	//gameManager.start();

    /*
    group.forEach([](EngineWorker& e) {
        e.stop();  
        });
	*/
    std::cout << "All engines completed.\n";
    return 0;
}

