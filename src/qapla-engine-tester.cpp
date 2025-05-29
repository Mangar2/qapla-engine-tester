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
#include "engine-worker-factory.h"
#include "cli-settings-manager.h"
#include "epd-manager.h"

bool runEpd() {
    auto epdList = CliSettingsManager::getGroup("epd");
	if (epdList.empty()) {
		return false; // No EPD settings provided
	}
	int concurrency = CliSettingsManager::get<int>("concurrency");
    EpdManager epdManager;
	for (auto& epd : epdList) {
        std::string file;
        int maxTime = 10;
        int minTime = 2;
        int seenPlies = 3;
		for (const auto& [key, value] : epd) {
			if (key == "file") {
				file = std::get<std::string>(value);
			}
			else if (key == "maxtime") {
				maxTime = std::get<int>(value);
			}
			else if (key == "mintime") {
				minTime = std::get<int>(value);
			}
			else if (key == "seenplies") {
				seenPlies = std::get<int>(value);
			}
		}
		for (const auto& engineConfig : EngineWorkerFactory::getConfigManager().getAllConfigs()) {
            std::string name = engineConfig.getName();
			Logger::testLogger().log("Using engine: " + name);
            epdManager.analyzeEpd(file, name, concurrency, maxTime, minTime, seenPlies);
            epdManager.wait();
		}
	}
    return true;
}

int main(int argc, char** argv) {
    try {
        Logger::testLogger().setTraceLevel(TraceLevel::commands);
        Logger::testLogger().log("Qapla Engine Tester - Prerelease 0.2.0 (c) by Volker Boehm\n");

        CliSettingsManager::registerSetting("concurrency", "Maximal number of in parallel running engines", true, 20,
            CliSettingsManager::ValueType::Int);
        CliSettingsManager::registerSetting("games-number", "Number of games to play", false, 20,
            CliSettingsManager::ValueType::Int);
	    CliSettingsManager::registerSetting("enginepath", "Path to an engine executable. Use the --engine parameter for more control on engines", false, "",
            CliSettingsManager::ValueType::PathExists);
        CliSettingsManager::registerSetting("logpath", "Path to the logging directory", false, std::string("."), 
            CliSettingsManager::ValueType::PathExists);
	    CliSettingsManager::registerSetting("testlevel", "Test level (0=all, 1=basic, 2=advanced)", false, 0,
		    CliSettingsManager::ValueType::Int);

        CliSettingsManager::registerGroup("epd", "Defines an epd configuration", {
            { "file",      { "Path and file name", true, "speelman Endgame.epd", CliSettingsManager::ValueType::PathExists } },
            { "maxtime",   { "Maximum allowed time in seconds per move during EPD analysis.", false, 20, CliSettingsManager::ValueType::Int } },
            { "mintime",   { "Minimum required time for an early stop, when a correct move is found", false, 2, CliSettingsManager::ValueType::Int } },
            { "seenplies", { "Amount of plies one of the expected moves must be shown to stop early", false, -1, CliSettingsManager::ValueType::Int } }
            });

        CliSettingsManager::registerGroup("engine", "Defines an engine configuration", {
            { "name",      { "Name of the engine", false, "", CliSettingsManager::ValueType::String } },
            { "cmd",       { "Path to executable", true, "", CliSettingsManager::ValueType::PathExists } },
            { "dir",       { "Working directory", false, ".", CliSettingsManager::ValueType::PathExists } },
            { "proto",     { "Protocol (uci/xboard)", false, "uci", CliSettingsManager::ValueType::String } },
            { "option.[name]",  { "UCI engine option", false, "", CliSettingsManager::ValueType::String } }
            });

        CliSettingsManager::parseCommandLine(argc, argv);
        // Setting engines
        EngineWorkerFactory::getConfigManagerMutable().addOrReplaceConfigurations(CliSettingsManager::getGroup("engine"));
        std::string enginePath = CliSettingsManager::get<std::string>("enginepath");
        if (enginePath != "") {
            EngineWorkerFactory::getConfigManagerMutable().addOrReplaceConfig(EngineConfig::createFromPath(enginePath));
        }

        if (!runEpd()) {
            Logger::engineLogger().setLogFile("qapla-engine-trace");
            Logger::testLogger().setLogFile("qapla-engine-report");
            Logger::testLogger().log("Detailed engine communication log: " + Logger::engineLogger().getFilename());
            Logger::testLogger().log("Summary test report log: " + Logger::testLogger().getFilename());
            Logger::testLogger().log("All tests will start in 1 second(s)...\n");
            std::this_thread::sleep_for(std::chrono::seconds(1));

            EngineTestController controller;
            controller.runAllTests(enginePath);
        }
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

