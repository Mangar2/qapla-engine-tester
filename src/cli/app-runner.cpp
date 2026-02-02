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

#include "app-runner.h"
#include "qapla-settings.h"
#include "../engine-tester/engine-report.h"
#include "../engine-tester/engine-test-controller.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../epd/epd-manager.h"
#include "../sprt/sprt-manager.h"
#include "../sprt/sprt-tournament-file.h"
#include "../spsa/spsa-optimizer.h"
#include "../tournament/tournament.h"
#include "../tournament/tournament-file.h"
#include "../opening/pgn-save.h"
#include "../game-manager/game-manager-pool.h"
#include "../game-manager/adjudication-manager.h"
#include "../base-elements/logger.h"
#include "settings-manager.h"

#include <format>
#include <sstream>

namespace QaplaTester {

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

static void checkTimeControl() {
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        if (!engine.getTimeControl().isValid()) {
            throw AppError::makeInvalidParameters(std::format(
                "No valid time control defined for engine '{}'. Please specify a time control using 'tc' option.",
                engine.getName()));
        }
    }
}

AppReturnCode AppRunner::runTest(const Settings::GroupInstance& test, AppReturnCode code) {
    Settings::QaplaSettings::instance().applyLoggerConfig("engine-report");
    Logger::reportLogger().logAligned("Summary test report log: ", Logger::reportLogger().getFilename());

    EngineTestController controller;
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        const auto& name = engine.getName();
        try {
            EngineReport::reportUnderruns = test.get<bool>("underrun");
            controller.runAllTests(engine, static_cast<int>(test.get<uint32_t>("numgames")));
        }
        catch (const AppError& ex) {
            Logger::reportLogger().log(std::format("Application error during engine test for {}: {}", name, ex.what()), 
                TraceLevel::error);
            code = ex.getReturnCode();
        }
        catch (const std::exception& e) {
            Logger::reportLogger().log(std::format("Application error during engine test for {}: {}", name, e.what()), 
                TraceLevel::error);
            code = AppReturnCode::GeneralError;
        }
        catch (...) {
            Logger::reportLogger().log(std::format("Unknown exception during engine test for {}", name), TraceLevel::error);
            code = AppReturnCode::GeneralError;
        }
        code = logChecklist(code);
    }
    return code;
}

AppReturnCode AppRunner::runEpd(AppReturnCode code) {
    const auto& epdConfig = Settings::QaplaSettings::instance().getEpdConfig();
    if (!epdConfig) {
        return code;
    }

    const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
    Settings::QaplaSettings::instance().applyLoggerConfig("epd-report");
    auto epdManager = std::make_shared<EpdManager>();

    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        const auto& name = engine.getName();
        const std::string earlyStop = std::format("Early stop - Seen plies: {} Min time: {}s",
            epdConfig->seenPlies, epdConfig->minTime);

        Logger::reportLogger().log(std::format("Using engine: {} Concurrency: {} Max Time: {}s {}",
            name, concurrency, epdConfig->maxTime, earlyStop));
        epdManager->initialize(epdConfig->file, epdConfig->maxTime, epdConfig->minTime, epdConfig->seenPlies);
        GameManagerPool& pool = GameManagerPool::getInstance();

        pool.setConcurrency(concurrency, true);
        epdManager->schedule(engine, pool);
        pool.waitForTask();
        Logger::reportLogger().log(std::format("Finished EPD test for engine: {}, success rate: {:.2f}%", 
            name, epdManager->getSuccessRate() * 100.0));
        if (code == AppReturnCode::NoError) {
            const bool success = epdManager->getSuccessRate() >= epdConfig->minSuccess / 100.0;
            code = success ? code : AppReturnCode::MissedTarget;
        }
    }
    return code;
}

AppReturnCode AppRunner::runTournament(AppReturnCode code) {
    const auto& tournamentConfig = Settings::QaplaSettings::instance().getTournamentConfig();
    if (!tournamentConfig) {
        return code;
    }

    const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
    Settings::QaplaSettings::instance().applyLoggerConfig("tournament-report");

    if (!Settings::QaplaSettings::instance().getOpenings()) {
        throw AppError::makeInvalidParameters("No openings defined for the tournament. Please define an opening, see --help for more info.");
    }

    checkTimeControl();

    auto tournament = std::make_shared<Tournament>();
    auto tGroup = Settings::Manager::instance().getGroupInstance("tournament");
    auto tfile = tGroup->get<std::string>("file");
    TournamentFile::setSaveCallback(tfile, tGroup->get<uint32_t>("saveinterval"), tournament);

    tournament->createTournament(EngineWorkerFactory::getActiveEngines(), *tournamentConfig);
    TournamentFile::loadGameResults(tfile, tournament);

    try {
        GameManagerPool& pool = GameManagerPool::getInstance();
        tournament->scheduleAll(concurrency, true, pool);
        pool.waitForTask();

        TournamentFile::save(tfile, Settings::Manager::instance(), tournament);
        Logger::reportLogger().log("tournament all games completed", TraceLevel::result);
        
        std::ostringstream oss;
        GameManagerPool::getInstance().getAdjudicationManager().printTestResult(oss);
        Logger::reportLogger().log(oss.str());
        std::string resultString = tournament->getResultString();
        Logger::reportLogger().log(resultString, TraceLevel::result);
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log(std::format("Exception during tournament run: {}", e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::reportLogger().log("Unknown exception during tournament run.", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }

    return code;
}

AppReturnCode AppRunner::runSprt(AppReturnCode code) {
    const auto& sprtConfig = Settings::QaplaSettings::instance().getSprtConfig();
    if (!sprtConfig) {
        return code;
    }
    
    auto sprtGroup = Settings::Manager::instance().getGroupInstance("sprt");
    const auto sprtfile = sprtGroup->get<std::string>("file");
    const bool isMontecarlo = sprtGroup->get<bool>("montecarlo");
    
    if (!isMontecarlo && !Settings::QaplaSettings::instance().getOpenings()) {
        throw AppError::makeInvalidParameters("No openings defined for SPRT tests. Please define an opening, see --help for more info.");
    }
    
    checkTimeControl();
    Settings::QaplaSettings::instance().applyLoggerConfig("sprt-report");
    
    try {
        auto manager = std::make_shared<SprtManager>();
        if (isMontecarlo) {
            manager->runMonteCarloTest(*sprtConfig);
        } else {
            SprtTournamentFile::setSaveCallback(sprtfile, sprtGroup->get<unsigned int>("saveinterval"), manager);
            const auto& activeEngines = EngineWorkerFactory::getActiveEngines();

            manager->createTournament(activeEngines, *sprtConfig);
            SprtTournamentFile::loadGameResults(sprtfile, manager);

            const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
            GameManagerPool& pool = GameManagerPool::getInstance();
            manager->schedule(manager, concurrency, pool);
            pool.waitForTask();

            SprtTournamentFile::save(sprtfile, Settings::Manager::instance(), manager);
            Logger::reportLogger().logStatus("sprt all games completed", "sprt", TraceLevel::result);

            if (code == AppReturnCode::NoError) {
                auto sprtResults = manager->getSprtResults();
                if (!sprtResults.empty() && !sprtResults.front().empty()) {
                    manager->logFinalResult();
                    
                    const auto decision = sprtResults.front().front().decision;
                    if (!decision) {
                        code = AppReturnCode::UndefinedResult;
                    } else if (*decision) {
                        code = AppReturnCode::H1Accepted;
                    } else {
                        code = AppReturnCode::H0Accepted;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log(std::format("Exception during SPRT run: {}", e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

AppReturnCode AppRunner::runSpsa(AppReturnCode code) {
    const auto& spsaConfig = Settings::QaplaSettings::instance().getSPSAConfig();
    if (!spsaConfig) {
        return code;
    }

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.empty()) {
        throw AppError::makeInvalidParameters("No engine defined for SPSA optimization.");
    }

    if (!Settings::QaplaSettings::instance().getOpenings()) {
        throw AppError::makeInvalidParameters("No openings defined for SPSA optimization. Please define an opening.");
    }

    checkTimeControl();
    Settings::QaplaSettings::instance().applyLoggerConfig("spsa-report");

    try {
        auto optimizer = std::make_shared<SPSAOptimizer>();
        optimizer->createSPSA(activeEngines.front(), *spsaConfig);
        
        const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
        GameManagerPool& pool = GameManagerPool::getInstance();
        optimizer->scheduleSPSA(concurrency, pool);
        pool.waitForTask();
        
        Logger::reportLogger().log("SPSA optimization completed.", TraceLevel::result);
    }
    catch (const std::exception& e) {
        Logger::reportLogger().log(std::format("Exception during SPSA run: {}", e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

void AppRunner::setAdjudicationOptions() {
    const auto& drawConfig = Settings::QaplaSettings::instance().getDrawAdjudicationConfig();
    if (drawConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setDrawAdjudicationConfig(*drawConfig);
    }

    const auto& resignConfig = Settings::QaplaSettings::instance().getResignAdjudicationConfig();
    if (resignConfig) {
        GameManagerPool::getInstance().getAdjudicationManager().setResignAdjudicationConfig(*resignConfig);
    }
}

void AppRunner::setPgnConfig() {
    const auto& pgnOptions = Settings::QaplaSettings::instance().getPgnOptions();
    if (!pgnOptions) {
        return;
    }

    PgnSave::tournament().setOptions(*pgnOptions);
}

AppReturnCode AppRunner::runDispatcher() {
    AppReturnCode returnCode = AppReturnCode::NoError;
    bool hasTask = false;

    setPgnConfig();
    setAdjudicationOptions();

    if (Settings::Manager::instance().getGroupInstance("test")) {
        returnCode = runTest(*Settings::Manager::instance().getGroupInstance("test"), returnCode);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("epd")) {
        returnCode = runEpd(returnCode);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("tournament")) {
        returnCode = runTournament(returnCode);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("sprt")) {
        returnCode = runSprt(returnCode);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("spsa")) {
        returnCode = runSpsa(returnCode);
        hasTask = true;
    }

    if (!hasTask) {
        throw AppError::makeInvalidParameters("No task defined. Please specify at least one task like --test, --epd, --sprt, --tournament, or --spsa.");
    }

    return returnCode;
}

} // namespace QaplaTester
