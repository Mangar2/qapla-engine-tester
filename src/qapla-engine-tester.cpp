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
#include "epd/epd-manager.h"
#include "sprt/sprt-manager.h"
#include "sprt/sprt-tournament-file.h"
#include "sprt/sprt-config-file.h"
#include "spsa/spsa-optimizer.h"
#include "tournament/tournament.h"
#include "tournament/tournament-file.h"
#include "opening/pgn-save.h"
#include "config-file/opening-config.h"
#include "config-file/pgn-config.h"
#include "config-file/adjudication-config.h"

#include "cli/input-handler.h"
#include "cli/settings-manager.h"
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
    const auto& epdConfig = Settings::QaplaSettings::instance().getEpdConfig();
    if (!epdConfig) return code;

    uint32_t concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
    Settings::QaplaSettings::instance().applyLoggerConfig("epd-report");
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

static AppReturnCode runTest(const Settings::GroupInstance& test, AppReturnCode code) {
    Settings::QaplaSettings::instance().applyLoggerConfig("report");
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

static auto sprtFileIO(const std::string filename, uint32_t saveInterval, 
    const std::shared_ptr<SprtManager>& manager) {
    QaplaHelpers::ConfigData configData;
    if (!filename.empty()) {
        if (SprtTournamentFile::loadSprtSettings(filename, configData)) {
            Logger::reportLogger().log("Loaded SPRT tournament state from: " + filename, TraceLevel::result);
        }
    }
    
    // Setup autosave callback if file and interval are specified
    if (!filename.empty() && saveInterval > 0) {
        
        manager->setGameFinishedCallback(
            [filename,
             configData = Settings::QaplaSettings::instance().getConfigData("sprt-tournament"),
             saveInterval,
             saveTrigger = 0u,
             manager = manager.get()]() mutable 
            {
                ++saveTrigger;
                if (saveTrigger >= saveInterval) {
                    saveTrigger = 0;
                    
                    auto section = manager->getSection();
                    if (section) {
                        auto saveData = configData;
                        saveData.addSection(*section);
                        SprtTournamentFile::save(filename, saveData);
                        Logger::reportLogger().log("Auto-saved SPRT state to: " + filename, TraceLevel::info);
                    }
                }
            }
        );
    }
    return configData.getSectionList("round", "sprt-tournament");
}

static auto runSprt(AppReturnCode code) {
    // Get SPRT config (already loaded from file or CLI by readSprtConfig)
    const auto& sprtConfig = Settings::QaplaSettings::instance().getSprtConfig();
    if (!sprtConfig) return code;
    
    auto sprtGroup = Settings::Manager::instance().getGroupInstance("sprt");
    auto sprtfile = sprtGroup->get<std::string>("file");
    auto isMontecarlo = sprtGroup->get<bool>("montecarlo");
    
    // Validate openings (already loaded from file or CLI)
    if (!isMontecarlo && !Settings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined for SPRT tests. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    // Validate time control (for engines loaded from file or CLI)
    checkTimeControl();
    
    Settings::QaplaSettings::instance().applyLoggerConfig("sprt-report");
    
    try {
        auto manager = std::make_shared<SprtManager>();
        if (isMontecarlo) {
            manager->runMonteCarloTest(*sprtConfig);
        } else {
            auto sections = sprtFileIO(sprtfile, sprtGroup->get<unsigned int>("saveinterval"), manager);
            const auto& updatedSprtConfig = Settings::QaplaSettings::instance().getSprtConfig();
            const auto& activeEngines = EngineWorkerFactory::getActiveEngines();

            manager->createTournament(activeEngines, *updatedSprtConfig);
            if (sections) {
                manager->setGameResults(*sections);
            }
            const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
            GameManagerPool& pool = GameManagerPool::getInstance();
            manager->schedule(manager, concurrency, pool);
            pool.waitForTask();
            
            if (!sprtfile.empty()) {
                auto section = manager->getSection();
                QaplaHelpers::ConfigData configData = Settings::QaplaSettings::instance().
                    getConfigData("sprt-tournament");
                if (section) {
                    configData.addSection(*section);
                }
                SprtTournamentFile::save(sprtfile, configData, "sprt-tournament");
            }
            
            code = updateCode(code, EngineReport::logAll(TraceLevel::command, manager->getResult()));
            Logger::reportLogger().log("sprt all games completed", TraceLevel::result);

            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                auto sprtResults = manager->getSprtResults();
                if (!sprtResults.empty() && !sprtResults.front().empty()) {
                    manager->logFinalResult();
                    
                    auto decision = sprtResults.front().front().decision;
                    code = !decision ? AppReturnCode::UndefinedResult : 
                           (*decision ? AppReturnCode::H1Accepted : AppReturnCode::H0Accepted);
                }
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
    const auto& spsaConfig = Settings::QaplaSettings::instance().getSPSAConfig();
    if (!spsaConfig) return code;
    const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    
    if (activeEngines.size() != 1) {
        Logger::reportLogger().log("SPSA optimization requires exactly one engine. Please define one engine, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    if (!Settings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined for SPSA optimization. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    
    checkTimeControl();
    Settings::QaplaSettings::instance().applyLoggerConfig("spsa-report");
    
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
    const auto& tournamentConfig = Settings::QaplaSettings::instance().getTournamentConfig();
    if (!tournamentConfig) return code;

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.size() < 2) {
        Logger::reportLogger().log("At least two engines must be defined. Please define more engines, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    if (!Settings::QaplaSettings::instance().getOpenings()) {
        Logger::reportLogger().log("No openings defined. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    checkTimeControl();

    Settings::QaplaSettings::instance().applyLoggerConfig("tournament-report");

    try {
        uint32_t concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");

        Tournament tournament;
        
        
        // Load tournament state if tournamentfile is specified
        if (!tournamentConfig->tournamentFilename.empty()) {
            QaplaHelpers::ConfigData configData;
            if (TournamentFile::loadIntoTournament(tournamentConfig->tournamentFilename, configData, tournament, "tournament")) {
                Logger::reportLogger().log("Loaded tournament state from: " + tournamentConfig->tournamentFilename, TraceLevel::result);
            } else {
                Logger::reportLogger().log("Failed to load tournament from file: " + tournamentConfig->tournamentFilename, TraceLevel::error);
                return AppReturnCode::GeneralError;
            }
        }
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
    const auto& drawConfig = Settings::QaplaSettings::instance().getDrawAdjudicationConfig();
    if (drawConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setDrawAdjudicationConfig(*drawConfig);
    }

    const auto& resignConfig = Settings::QaplaSettings::instance().getResignAdjudicationConfig();
    if (resignConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setResignAdjudicationConfig(*resignConfig);
    }
}

static void setPgnOptions() {
    const auto& pgnOptions = Settings::QaplaSettings::instance().getPgnOptions();
    if (!pgnOptions) return;

    PgnSave::tournament().setOptions(*pgnOptions);
}


static AppReturnCode run() {
    AppReturnCode returnCode = AppReturnCode::NoError;

    InputHandler::inputLoop(
        Settings::QaplaSettings::instance().getArguments().size() == 1 
        || Settings::Manager::instance().get<bool>("interactive"));

    setPgnOptions();
    setAdjudicationOptions();

    if (auto test = Settings::Manager::instance().getGroupInstance("test")) {
        returnCode = runTest(*test, returnCode);
    }

    if (Settings::QaplaSettings::instance().getEpdConfig()) {
        returnCode = runEpd(returnCode);
    }

    if (Settings::QaplaSettings::instance().getTournamentConfig()) {
        returnCode = runTournament(returnCode);
    }

    if (Settings::QaplaSettings::instance().getSprtConfig()) {
        returnCode = runSprt(returnCode);
    }

    if (Settings::QaplaSettings::instance().getSPSAConfig()) {
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
        Settings::QaplaSettings::instance().init();
        
        // Convert and store arguments
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        Settings::QaplaSettings::instance().applyArguments(args);

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

