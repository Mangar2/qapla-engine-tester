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
#include <memory>

#ifndef _WIN32
#include <signal.h>
#endif

#include "app-error.h"
#include "engine-report.h"
#include "engine-test-controller.h"
#include "logger.h"
#include "engine-worker-factory.h"
#include "cli-settings-manager.h"
#include "qapla-settings.h"
#include "epd-manager.h"
#include "sprt-manager.h"
#include "tournament.h"
#include "timer.h"
#include "time-control.h"
#include "pgn-io.h"
#include "input-handler.h"
#include "game-manager-pool.h"
#include "adjudication-manager.h"

using namespace QaplaTester;
using QaplaHelpers::Timer;

static auto updateCode(AppReturnCode code, AppReturnCode newCode) {
	if (code == AppReturnCode::NoError) {
		return newCode;
	}
	else if (code >= AppReturnCode::EngineError) {
		return std::min(code, newCode);
	}
	return code;
}

static auto logChecklist(AppReturnCode code, TraceLevel traceLevel = TraceLevel::command) {
    auto newCode = EngineReport::logAll(traceLevel);
    if (code == AppReturnCode::NoError) {
        code = newCode;
    }
    else if (code >= AppReturnCode::EngineError) {
        code = std::min(code, newCode);
    }
    return code;
}

static auto runEpd(const CliSettings::GroupInstances& epdList, AppReturnCode code) {
    uint32_t concurrency = CliSettings::Manager::get<unsigned int>("concurrency");
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "epd-report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    auto epdManager = std::make_shared<EpdManager>();
	for (auto& epd : epdList) {
        std::string file;
        uint32_t maxTime = 10;
        uint32_t minTime = 2;
        uint32_t seenPlies = 3;
		file = epd.get<std::string>("file");
		maxTime = epd.get<unsigned int>("maxtime");
		minTime = epd.get<unsigned int>("mintime");
		seenPlies = epd.get<unsigned int>("seenplies");

		for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
            std::string name = engine.getName();
            std::string earlyStop = "Early stop - Seen plies: " + 
                std::to_string(seenPlies) + " Min time: " + std::to_string(minTime) + "s";
			Logger::reportLogger().log("Using engine: " + name 
                + " Concurrency: " + std::to_string(concurrency) + " Max Time: " + std::to_string(maxTime) + "s "
                + earlyStop);
            epdManager->initialize(file, maxTime, minTime, seenPlies);
            epdManager->schedule(engine);
            GameManagerPool::getInstance().waitForTask();
			code = logChecklist(code, TraceLevel::info);
			auto minSuccess = epd.get<int>("minsuccess");
            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                bool success = epdManager->getSuccessRate() >= minSuccess / 100.0;
				code = success ? code : AppReturnCode::MissedTarget;
            }
		}
	}
    return code;
}

static AppReturnCode handleGlobalOptions(AppReturnCode code) {
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    if (CliSettings::Manager::get<bool>("enginelog")) {
        EngineLogger::engineLogger().setTraceLevel(TraceLevel::error, TraceLevel::info);
    }

    return code;
}

static AppReturnCode runTest(const CliSettings::GroupInstance& test, AppReturnCode code) {
    
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    Logger::reportLogger().setTraceLevel(TraceLevel::warning);
    if (!EngineLogger::engineLogger().getFilename().empty()) {
        Logger::reportLogger().logAligned("Engine communication log: ", 
            EngineLogger::engineLogger().getFilename());
    }
    Logger::reportLogger().logAligned("Summary test report log: ", Logger::reportLogger().getFilename());

    EngineTestController controller;
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        std::string name = engine.getName();
        try {
			EngineReport::reportUnderruns = test.get<bool>("underrun");
            controller.runAllTests(engine, test.get<uint32_t>("numgames"));
        }
        catch (const AppError& ex) {
            Logger::reportLogger().log("Application error during engine test for " + name + ": " + std::string(ex.what()), 
                TraceLevel::error);
            code = ex.getReturnCode();
        }
		catch (const std::exception& e) {
			Logger::reportLogger().log("Application error during engine test for " + name + ": " + std::string(e.what()), 
                TraceLevel::error);
            code = AppReturnCode::GeneralError;
		}
		catch (...) {
			Logger::reportLogger().log("Unknown exception during engine test for " + name, TraceLevel::error);
            code = AppReturnCode::GeneralError;
		}
        code = logChecklist(code);
    }
    return code;
}


static void checkTimeControl() {
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        if (!engine.getTimeControl().isValid()) {
            throw AppError::makeInvalidParameters("No valid time control defined for engine '" + engine.getName()
                + "'. Please specify a time control using 'tc' option.");
        }
    }
} 

static auto runSprt(AppReturnCode code) {
    const auto& sprtConfig = CliSettings::QaplaSettings::instance().getSprtConfig();
    if (!sprtConfig) return code;

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    auto isMontecarlo = CliSettings::Manager::getGroupInstance("sprt")->get<bool>("montecarlo");
    
    if (activeEngines.size() < 2 && !isMontecarlo) {
        Logger::reportLogger().log("At least two engines must be defined for SPRT tests. Please define two engines, see --help for more info.",
            TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    if (!isMontecarlo && !CliSettings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined for SPRT tests. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    checkTimeControl();
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "sprt-report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    try {
        uint32_t concurrency = CliSettings::Manager::get<unsigned int>("concurrency");

        auto manager = std::make_shared<SprtManager>();
        if (isMontecarlo) {
            manager->runMonteCarloTest(*sprtConfig);
        }
        else {
            auto filename = CliSettings::Manager::getGroupInstance("sprt")->get<std::string>("resultfile");
            manager->createTournament(activeEngines[0], activeEngines[1], *sprtConfig);
            // manager->load(filename);
            // manager->schedule(manager, concurrency);
            // manager->wait();
            if (!filename.empty()) {
                manager->save(filename);
            }
			code = updateCode(code, EngineReport::logAll(TraceLevel::command, manager->getResult()));
			Logger::reportLogger().log("sprt all games completed", TraceLevel::result);

            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                auto decision = manager->getDecision();
				code = !decision ? AppReturnCode::UndefinedResult : 
                    (*decision ? AppReturnCode::H1Accepted : AppReturnCode::H0Accepted);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log("Exception during sprt run: " + std::string(e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::reportLogger().log("Unknown exception during sprt run: ", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

static AppReturnCode runTournament(AppReturnCode code) {
    const auto& tournamentConfig = CliSettings::QaplaSettings::instance().getTournamentConfig();
    if (!tournamentConfig) return code;

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.size() < 2) {
        Logger::reportLogger().log("At least two engines must be defined. Please define more engines, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    if (!CliSettings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    checkTimeControl();

    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "tournament-report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);

    try {
        uint32_t concurrency = CliSettings::Manager::get<unsigned int>("concurrency");

        Tournament tournament;
        
        tournament.createTournament(activeEngines, *tournamentConfig);
		tournament.load(tournamentConfig->tournamentFilename);
        tournament.scheduleAll(concurrency);
        // tournament.wait();
		if (!tournamentConfig->tournamentFilename.empty()) {
			// tournament.save(tournamentConfig->tournamentFilename);
		}
        Logger::reportLogger().log("tournament all games completed", TraceLevel::result);
        AdjudicationManager::instance().printTestResult(std::cout);
        std::string resultString = tournament.getResultString();
        Logger::reportLogger().log(resultString, TraceLevel::result);

 		code = updateCode(code, EngineReport::logAll(TraceLevel::info, tournament.getResult()));
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log("Exception during tournament run: " + std::string(e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::reportLogger().log("Unknown exception during tournament run.", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }

    return code;
}

static void handleAdjudicationOptions() {
    const auto& drawConfig = CliSettings::QaplaSettings::instance().getDrawAdjudicationConfig();
    if (drawConfig) {
        AdjudicationManager::instance().setDrawAdjudicationConfig(*drawConfig);
    }

    const auto& resignConfig = CliSettings::QaplaSettings::instance().getResignAdjudicationConfig();
    if (resignConfig) {
        AdjudicationManager::instance().setResignAdjudicationConfig(*resignConfig);
    }
}

static void handlePgnOptions() {
    const auto& pgnOptions = CliSettings::QaplaSettings::instance().getPgnOptions();
    if (!pgnOptions) return;

    PgnIO::tournament().setOptions(*pgnOptions);
}

static void handleEngineOptions() {
	EngineWorkerFactory::setSuppressInfoLines(CliSettings::Manager::get<bool>("rapid"));
    std::string enginesFile = CliSettings::Manager::get<std::string>("enginesfile");
    if (!enginesFile.empty()) {
        EngineWorkerFactory::getConfigManagerMutable().loadFromFile(enginesFile);
    }
    auto engineSettings = CliSettings::Manager::getGroupInstances("engine");
	auto eachSetting = CliSettings::Manager::getGroupInstance("each");
	CliSettings::ValueMap eachOptions;
	if (eachSetting) {
		eachOptions = eachSetting->getValues();
	}
    EngineConfig config;

    for (const auto& engine : engineSettings) {
        std::string cmd = engine.get<std::string>("cmd");
        std::string conf = engine.get<std::string>("conf");
        std::string name = engine.get<std::string>("name");

        CliSettings::ValueMap options = engine.getValues();
        options.insert(eachOptions.begin(), eachOptions.end());

        if (!cmd.empty()) {
            config = EngineConfig::createFromValueMap(options);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
		}
		else if (!conf.empty()) {
			auto engineConfig = EngineWorkerFactory::getConfigManager().getConfig(conf);
			if (!engineConfig) {
				throw AppError::makeInvalidParameters("Engine configuration '" + conf + "' not found.");
			}
            config = *engineConfig;
			config.setCommandLineOptions(options, true);
            name = config.getName();
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
		}
		else {
            std::string engineName = name.empty() ? "" : " (for " + name + ")";
            throw AppError::makeInvalidParameters("No engine command or configuration provided"
                + engineName + ".Please specify either 'cmd' or 'conf'.");
		}

    }
    // Ensure that all active engines have different names
    EngineWorkerFactory::assignUniqueDisplayNames();
}

static AppReturnCode run() {
    AppReturnCode returnCode = AppReturnCode::NoError;

    InputHandler::inputLoop(
        CliSettings::QaplaSettings::instance().getArguments().size() == 1 
        || CliSettings::Manager::get<bool>("interactive"));

    handleGlobalOptions(returnCode);
    handlePgnOptions();
    handleEngineOptions();
    handleAdjudicationOptions();

    if (auto test = CliSettings::Manager::getGroupInstance("test")) {
        returnCode = runTest(*test, returnCode);
    }

    auto epdList = CliSettings::Manager::getGroupInstances("epd");
    if (!epdList.empty()) {
        returnCode = runEpd(epdList, returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getTournamentConfig()) {
        returnCode = runTournament(returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getSprtConfig()) {
        returnCode = runSprt(returnCode);
    }
    return returnCode;
}

int main(int argc, char** argv) {
    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif
    Timer timer;
    timer.start();
    

    AppReturnCode returnCode = AppReturnCode::NoError;
    try {
        Logger::reportLogger().setTraceLevel(TraceLevel::command);
        Logger::reportLogger().log("Qapla Engine Tester - Prerelease 0.5.0 (c) by Volker Boehm\n");

        // Initialize settings
        CliSettings::QaplaSettings::instance().init();
        
        // Convert and store arguments
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        CliSettings::QaplaSettings::instance().applyArguments(args);

        returnCode = run();
			
    }
    catch (const AppError& ex) {
		Logger::reportLogger().log("Application error: " + std::string(ex.what()), TraceLevel::error);
        returnCode = ex.getReturnCode();
    }
	catch (const std::exception& e) {
		Logger::reportLogger().log(std::string(e.what()), TraceLevel::error);
        returnCode = AppReturnCode::GeneralError;
	}
	catch (...) {
		Logger::reportLogger().log("Unknown exception, program terminated.", TraceLevel::error);
		returnCode = AppReturnCode::GeneralError;
	}
	timer.printElapsed("Total runtime: ");
	
    // Unregisters the input handler callback before destruction of the input handler
	GameManagerPool::resetInstance();
    return static_cast<int>(returnCode);
}

