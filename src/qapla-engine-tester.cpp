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

#include "checklist.h"
#include "engine-test-controller.h"
#include "logger.h"
#include "cli-settings-manager.h"

int main(int argc, char** argv) {
    try {

        CliSettingsManager::registerSetting("concurrency", "Maximal number of in parallel running engines", true, 20,
            CliSettingsManager::ValueType::Int);
        CliSettingsManager::registerSetting("games-number", "Number of games to play", false, 20,
            CliSettingsManager::ValueType::Int);
	    CliSettingsManager::registerSetting("engine", "Path to engine executable", true, 
            "",
            //"C:\\Chess\\cutechess-cli\\qapla0.3\\viridithas3.0.0-avx2.exe",
            //"C:\\chess\\delivery\\Qapla0.3.2\\Qapla0.3.2-win-x86.exe",
            CliSettingsManager::ValueType::PathExists);
        CliSettingsManager::registerSetting("logpath", "Path to the logging directory", false, std::string("."), 
            CliSettingsManager::ValueType::PathExists);
	    CliSettingsManager::registerSetting("testlevel", "Test level (0=all, 1=basic, 2=advanced)", false, 0,
		    CliSettingsManager::ValueType::Int);
        CliSettingsManager::parseCommandLine(argc, argv);

        std::string enginePath = CliSettingsManager::get<std::string>("engine");

        const std::size_t engineCount = 1;
        Logger::engineLogger().setLogFile("qapla-engine-trace");
        Logger::testLogger().setLogFile("qapla-engine-report");
	    Logger::testLogger().setTraceLevel(TraceLevel::commands);
        Logger::testLogger().log("Qapla Engine Tester - Prerelease 0.1.0 (c) by Volker Boehm\n");
        Logger::testLogger().log("Detailed engine communication log: " + Logger::engineLogger().getFilename());
        Logger::testLogger().log("Summary test report log: " + Logger::testLogger().getFilename());
	    Logger::testLogger().log("All tests will start in 1 second(s)...\n");  
        std::this_thread::sleep_for(std::chrono::seconds(1));

        EngineTestController controller;
        controller.runAllTests(enginePath);
    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during engine test: " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during engine test.", TraceLevel::error);
	}
    
	Checklist::log();
    if (argc == 1) {
        std::cout << "Press Enter to quit...";
        std::cin.get();
    }
    return 0;
}

