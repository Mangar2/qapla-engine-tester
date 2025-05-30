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
#include "timer.h"

bool runEpd() {
    auto epdList = CliSettings::Manager::getGroupInstances("epd");
	if (epdList.empty()) {
		return false; // No EPD settings provided
	}
	int concurrency = CliSettings::Manager::get<int>("concurrency");
    EpdManager epdManager;
	for (auto& epd : epdList) {
        std::string file;
        int maxTime = 10;
        int minTime = 2;
        int seenPlies = 3;
		file = epd.get<std::string>("file");
		maxTime = epd.get<int>("maxtime");
		minTime = epd.get<int>("mintime");
		seenPlies = epd.get<int>("seenplies");
		for (const auto& engineConfig : EngineWorkerFactory::getConfigManager().getAllConfigs()) {
            std::string name = engineConfig.getName();
            std::string earlyStop = minTime < 0 ? "" : "Early stop - Seen plies: " + std::to_string(seenPlies) + " Min time: " + std::to_string(minTime) + "s";
			Logger::testLogger().log("Using engine: " + name 
                + " Concurrency: " + std::to_string(concurrency) + " Max Time: " + std::to_string(maxTime) + "s "
                + earlyStop);
            epdManager.analyzeEpd(file, name, concurrency, maxTime, minTime, seenPlies);
            epdManager.wait();
		}
	}
    return true;
}

bool runTest() {
    auto tests = CliSettings::Manager::getGroupInstances("test");
    if (tests.empty()) {
        return false; // No EPD settings provided
    }
	auto test = tests[0]; // Assuming only one test group is defined
    Logger::testLogger().setLogFile("qapla-engine-report");
    Logger::testLogger().log("Detailed engine communication log: " + Logger::engineLogger().getFilename());
    Logger::testLogger().log("Summary test report log: " + Logger::testLogger().getFilename());

    EngineTestController controller;
    for (const auto& engineConfig : EngineWorkerFactory::getConfigManager().getAllConfigs()) {
        Checklist::clear();
        std::string name = engineConfig.getName();
        try {
            controller.runAllTests(name, test.get<int>("numgames"), test.get<int>("level"));
        }
		catch (const std::exception& e) {
			Logger::testLogger().log("Exception during engine test for " + name + ": " + std::string(e.what()), TraceLevel::error);
		}
		catch (...) {
			Logger::testLogger().log("Unknown exception during engine test for " + name, TraceLevel::error);
		}
        Checklist::log();
    }
    return true;
}

int main(int argc, char** argv) {
    bool isEngineTest = false;
    Timer timer;
    timer.start();
    try {
        Logger::testLogger().setTraceLevel(TraceLevel::commands);
        Logger::testLogger().log("Qapla Engine Tester - Prerelease 0.2.0 (c) by Volker Boehm\n");

        CliSettings::Manager::registerSetting("concurrency", "Maximal number of in parallel running engines", true, 10,
            CliSettings::ValueType::Int);
		CliSettings::Manager::registerSetting("enginesfile", "Path to an ini file with engine configurations", false, "",
			CliSettings::ValueType::PathExists);
		CliSettings::Manager::registerSetting("enginelog", "Enable engine logging", false, false,
			CliSettings::ValueType::Bool);
	    CliSettings::Manager::registerSetting("enginepath", "Path to an engine executable. Use the --engine parameter for more control on engines", false, "",
            CliSettings::ValueType::PathExists);
        CliSettings::Manager::registerSetting("logpath", "Path to the logging directory", false, std::string("."), 
            CliSettings::ValueType::PathExists);

        CliSettings::Manager::registerGroup("epd", "Configuration to run an epd testset against engines", {
            { "file",      { "Path and file name to the epd file", true, "speelman Endgame.epd", CliSettings::ValueType::PathExists } },
            { "maxtime",   { "Maximum allowed time in seconds per move during EPD analysis.", false, 20, CliSettings::ValueType::Int } },
            { "mintime",   { "Minimum required time for an early stop, when a correct move is found", false, 2, CliSettings::ValueType::Int } },
            { "seenplies", { "Amount of plies one of the expected moves must be shown to stop early (-1 = off)", false, -1, CliSettings::ValueType::Int } }
            });

        CliSettings::Manager::registerGroup("engine", "Defines an engine configuration", {
            { "name",      { "Name of the engine", false, "", CliSettings::ValueType::String } },
            { "cmd",       { "Path to executable", true, "", CliSettings::ValueType::PathExists } },
            { "dir",       { "Working directory", false, ".", CliSettings::ValueType::PathExists } },
            { "proto",     { "Protocol (uci/xboard)", false, "uci", CliSettings::ValueType::String } },
            { "option.[name]",  { "UCI engine option", false, "", CliSettings::ValueType::String } }
            });

        CliSettings::Manager::registerGroup("test", "Test engines", {
            { "numgames",  { "Number of test games to play", false, 20, CliSettings::ValueType::Int } },
            { "level",     { "Test level (0=all, 1=nice, 2=destructive)", false, 0, CliSettings::ValueType::Int } }
            });

        CliSettings::Manager::parseCommandLine(argc, argv);
        // Setting engines
		std::string enginesFile = CliSettings::Manager::get<std::string>("enginesfile");
        if (!enginesFile.empty()) {
            EngineWorkerFactory::getConfigManagerMutable().loadFromFile(enginesFile);
        }
        auto engineSettings = CliSettings::Manager::getGroupInstances("engine");
        EngineWorkerFactory::getConfigManagerMutable().addOrReplaceConfigurations(engineSettings);
        std::string enginePath = CliSettings::Manager::get<std::string>("enginepath");
        if (enginePath != "") {
            EngineWorkerFactory::getConfigManagerMutable().addOrReplaceConfig(EngineConfig::createFromPath(enginePath));
        }
		if (CliSettings::Manager::get<bool>("enginelog")) {
            Logger::engineLogger().setLogFile("qapla-engine-trace");
		}
        runEpd();
        runTest();
			
    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during engine test: " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during engine test.", TraceLevel::error);
	}
    
	timer.printElapsed("Total runtime: ");
    if (argc == 1) {
        std::cout << "Press Enter to quit...";
        std::cin.get();
    }
    return 0;
}

