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
#include "logger.h"

int main() {
    const std::filesystem::path enginePath = "C:\\Development\\qapla-engine-tester\\Qapla0.3.2-win-x86.exe";
	const std::filesystem::path stockfish = "C:\\Chess\\Engines\\stockfish-windows-x86-64-avx2\\stockfish\\stockfish-windows-x86-64-avx2.exe";
    const std::size_t engineCount = 1;
    Logger::engineLogger().setLogFile("qapla-engine-trace");
    Logger::testLogger().setLogFile("qapla-engine-report");
	Logger::testLogger().setTraceLevel(TraceLevel::commands);
    Logger::testLogger().log("Qapla Engine Tester, Prerelease 0.1.0 (c) by Volker Boehm\n");
    Logger::testLogger().log("The engine test log is available in " + Logger::testLogger().getFilename());
	Logger::testLogger().log("The engine communication log is available in " + Logger::engineLogger().getFilename());
	Logger::testLogger().log("All tests will start in 1 second(s)...\n");  
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EngineTestController controller;
	controller.runAllTests(enginePath);
    
	EngineChecklist::print(std::cout);
    return 0;
}

