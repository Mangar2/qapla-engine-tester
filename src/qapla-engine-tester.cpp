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
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
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

#include "engine-tester/engine-report.h"
#include "engine-tester/engine-test-controller.h"
#include "engine-handling/engine-worker-factory.h"
#include "epd-manager.h"
#include "sprt/sprt-manager.h"
#include "spsa-optimizer.h"
#include "tournament.h"
#include "opening/pgn-save.h"

#include "cli/input-handler.h"
#include "cli/cli-settings-manager.h"
#include "cli/qapla-settings.h"

#include "game-manager/game-manager-pool.h"
#include "game-manager/adjudication-manager.h"

#include "base-elements/app-error.h"
#include "base-elements/timer.h"
#include "base-elements/time-control.h"
#include "base-elements/logger.h"

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

static auto runEpd(AppReturnCode code) {
    const auto& epdConfig = CliSettings::QaplaSettings::instance().getEpdConfig();
    if (!epdConfig) return code;

    uint32_t concurrency = CliSettings::Manager::get<unsigned int>("concurrency");
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "epd-report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    auto epdManager = std::make_shared<EpdManager>();

    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        std::string name = engine.getName();
        std::string earlyStop = "Early stop - Seen plies: " + 
            std::to_string(epdConfig->seenPlies) + " Min time: " + std::to_string(epdConfig->minTime) + "s";
        Logger::reportLogger().log("Using engine: " + name 
            + " Concurrency: " + std::to_string(concurrency) + " Max Time: " + std::to_string(epdConfig->maxTime) + "s "
            + earlyStop);
        epdManager->initialize(epdConfig->file, epdConfig->maxTime, epdConfig->minTime, epdConfig->seenPlies);
        epdManager->schedule(engine);
        GameManagerPool::getInstance().waitForTask();
        code = logChecklist(code, TraceLevel::info);
        if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
            bool success = epdManager->getSuccessRate() >= epdConfig->minSuccess / 100.0;
            code = success ? code : AppReturnCode::MissedTarget;
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
    const auto concurrency = CliSettings::Manager::get<unsigned int>("concurrency");

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    auto isMontecarlo = CliSettings::Manager::getGroupInstance("sprt")->get<bool>("montecarlo");
    
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
        auto manager = std::make_shared<SprtManager>();
        if (isMontecarlo) {
            manager->runMonteCarloTest(*sprtConfig);
        }
        else {
            auto filename = CliSettings::Manager::getGroupInstance("sprt")->get<std::string>("resultfile");
            manager->createTournament(activeEngines, *sprtConfig);
            GameManagerPool& pool = GameManagerPool::getInstance();
            manager->schedule(manager, concurrency, pool);
            pool.waitForTask();
            if (!filename.empty()) {
                manager->save(filename);
            }
			code = updateCode(code, EngineReport::logAll(TraceLevel::command, manager->getResult()));
			Logger::reportLogger().log("sprt all games completed", TraceLevel::result);

            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                std::optional<bool> decision;
                auto sprtResults = manager->getSprtResults();
                if (!sprtResults.empty() && !sprtResults.front().empty()) {
                    decision = sprtResults.front().front().decision;
                }
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

static auto runSpsa(AppReturnCode code) {
    const auto& spsaConfig = CliSettings::QaplaSettings::instance().getSPSAConfig();
    if (!spsaConfig) return code;
    const auto concurrency = CliSettings::Manager::get<unsigned int>("concurrency");

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    
    if (activeEngines.size() != 1) {
        Logger::reportLogger().log("SPSA optimization requires exactly one engine. Please define one engine, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    if (!CliSettings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined for SPSA optimization. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    checkTimeControl();
    setLoggerConfig({
        .logPath = CliSettings::QaplaSettings::instance().getLogPath(),
        .reportLogBaseName = "spsa-report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = LogFileStrategy::global
    });
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    
    try {
        auto optimizer = std::make_shared<SPSAOptimizer>();
        optimizer->createSPSA(activeEngines[0], *spsaConfig);
        
        GameManagerPool& pool = GameManagerPool::getInstance();
        optimizer->scheduleSPSA(concurrency, pool);
        pool.waitForTask();
        
        // Print final results
        auto currentParams = optimizer->getCurrentParameters();
        Logger::reportLogger().log("SPSA optimization completed", TraceLevel::result);
        std::ostringstream oss;
        oss << "\nFinal optimized parameters:\n";
        for (size_t i = 0; i < spsaConfig->parameters.size(); ++i) {
            oss << "  " << spsaConfig->parameters[i].name << ": " 
                << currentParams[i] << "\n";
        }
        Logger::reportLogger().log(oss.str(), TraceLevel::result);
        
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log("Exception during SPSA run: " + std::string(e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::reportLogger().log("Unknown exception during SPSA run: ", TraceLevel::error);
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
        GameManagerPool::getInstance().getAdjudicationManager().printTestResult(std::cout);
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

static void setAdjudicationOptions() {
    const auto& drawConfig = CliSettings::QaplaSettings::instance().getDrawAdjudicationConfig();
    if (drawConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setDrawAdjudicationConfig(*drawConfig);
    }

    const auto& resignConfig = CliSettings::QaplaSettings::instance().getResignAdjudicationConfig();
    if (resignConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setResignAdjudicationConfig(*resignConfig);
    }
}

static void setPgnOptions() {
    const auto& pgnOptions = CliSettings::QaplaSettings::instance().getPgnOptions();
    if (!pgnOptions) return;

    PgnSave::tournament().setOptions(*pgnOptions);
}


static AppReturnCode run() {
    AppReturnCode returnCode = AppReturnCode::NoError;

    InputHandler::inputLoop(
        CliSettings::QaplaSettings::instance().getArguments().size() == 1 
        || CliSettings::Manager::get<bool>("interactive"));

    handleGlobalOptions(returnCode);
    setPgnOptions();
    setAdjudicationOptions();

    if (auto test = CliSettings::Manager::getGroupInstance("test")) {
        returnCode = runTest(*test, returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getEpdConfig()) {
        returnCode = runEpd(returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getTournamentConfig()) {
        returnCode = runTournament(returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getSprtConfig()) {
        returnCode = runSprt(returnCode);
    }

    if (CliSettings::QaplaSettings::instance().getSPSAConfig()) {
        returnCode = runSpsa(returnCode);
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

