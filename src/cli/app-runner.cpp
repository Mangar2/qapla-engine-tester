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
#include "../base-elements/json-helper.h"
#include "../base-elements/table-format.h"
#include "settings-manager.h"

#include <format>
#include <sstream>

namespace QaplaTester {

namespace {

[[nodiscard]] Mcp::JsonValue makeEmptyObject() {
    return Mcp::JsonHelper::makeObject({});
}

[[nodiscard]] Mcp::JsonValue parseJsonText(const std::string& serializedJson) {
    std::string_view jsonView = serializedJson;
    try {
        return Mcp::JsonHelper::parse(jsonView);
    } catch (...) {
        return makeEmptyObject();
    }
}

[[nodiscard]] Mcp::JsonValue createSprtStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Sprt || !app.getSprtManager()) {
        return makeEmptyObject();
    }

    const auto result = app.getSprtManager()->computeSprt();
    Mcp::JsonValue::Object statusObject;
    statusObject["llr"] = Mcp::JsonHelper::makeNumber(result.llr);
    statusObject["lower_bound"] = Mcp::JsonHelper::makeNumber(result.lowerBound);
    statusObject["upper_bound"] = Mcp::JsonHelper::makeNumber(result.upperBound);
    statusObject["elo_h0"] = Mcp::JsonHelper::makeNumber(result.eloH0);
    statusObject["elo_h1"] = Mcp::JsonHelper::makeNumber(result.eloH1);
    statusObject["games"] = Mcp::JsonHelper::makeNumber(static_cast<double>(result.winsA + result.winsB + result.draws));
    statusObject["wins"] = Mcp::JsonHelper::makeNumber(static_cast<double>(result.winsA));
    statusObject["losses"] = Mcp::JsonHelper::makeNumber(static_cast<double>(result.winsB));
    statusObject["draws"] = Mcp::JsonHelper::makeNumber(static_cast<double>(result.draws));
    return Mcp::JsonHelper::makeObject(std::move(statusObject));
}

[[nodiscard]] Mcp::JsonValue createTournamentStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Tournament || !app.getTournament()) {
        return makeEmptyObject();
    }

    Mcp::JsonValue::Object statusObject;
    const auto ratingJson = TableFormat::toJson("ratingTable", app.getTournament()->getRatingStatusTable());
    const auto outcomeJson = TableFormat::toJson("outcome", app.getTournament()->getOutcomeStatusTable());
    statusObject["rating"] = parseJsonText(ratingJson);
    statusObject["outcome"] = parseJsonText(outcomeJson);
    return Mcp::JsonHelper::makeObject(std::move(statusObject));
}

[[nodiscard]] Mcp::JsonValue createEpdStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Epd || !app.getEpdManager()) {
        return makeEmptyObject();
    }

    Mcp::JsonValue::Object statusObject;
    const auto resultsJson = TableFormat::toJson("epdStatus", app.getEpdManager()->getStatusTable());
    statusObject["results"] = parseJsonText(resultsJson);
    return Mcp::JsonHelper::makeObject(std::move(statusObject));
}

[[nodiscard]] Mcp::JsonValue createSpsaStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Spsa || !app.getSPSAOptimizer()) {
        return makeEmptyObject();
    }

    Mcp::JsonValue::Object statusObject;
    const auto parametersJson = TableFormat::toJson("spsaStatus", app.getSPSAOptimizer()->getStatusTable());
    statusObject["parameters"] = parseJsonText(parametersJson);
    statusObject["completed_iterations"] = Mcp::JsonHelper::makeNumber(
        static_cast<double>(app.getSPSAOptimizer()->getCompletedIterations()));
    return Mcp::JsonHelper::makeObject(std::move(statusObject));
}

} // namespace

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
            auto isMcp = Settings::Manager::instance().get<bool>("mcp");
            throw AppError::makeInvalidParameters(std::format(
                "No valid time control defined for engine '{}'. Please specify a time control using '{}' option.",
                engine.getName(), isMcp ? "engine_tc" : "tc"));
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

AppReturnCode AppRunner::runEpd(AppReturnCode code, bool background) {
    const auto& epdConfig = Settings::QaplaSettings::instance().getEpdConfig();
    if (!epdConfig) {
        return code;
    }

    const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
    Settings::QaplaSettings::instance().applyLoggerConfig("epd-report");
    epdManager_ = std::make_shared<EpdManager>();

    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        const auto& name = engine.getName();
        const std::string earlyStop = std::format("Early stop - Seen plies: {} Min time: {}s",
            epdConfig->seenPlies, epdConfig->minTime);

        Logger::reportLogger().log(std::format("Using engine: {} Concurrency: {} Max Time: {}s {}",
            name, concurrency, epdConfig->maxTime, earlyStop));
        epdManager_->initialize(epdConfig->file, epdConfig->maxTime, epdConfig->minTime, epdConfig->seenPlies);
        GameManagerPool& pool = GameManagerPool::getInstance();

        pool.setConcurrency(concurrency, true);
        epdManager_->schedule(engine, pool);
        
        if (background) {
            Logger::reportLogger().log("Task started in background.", TraceLevel::result);
            return code;
        }

        pool.waitForTask();
        Logger::reportLogger().log(std::format("Finished EPD test for engine: {}, success rate: {:.2f}%", 
            name, epdManager_->getSuccessRate() * 100.0));
        if (code == AppReturnCode::NoError) {
            const bool success = epdManager_->getSuccessRate() >= epdConfig->minSuccess / 100.0;
            code = success ? code : AppReturnCode::MissedTarget;
        }
    }
    return code;
}

AppReturnCode AppRunner::runTournament(AppReturnCode code, bool background) {
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

    tournament_ = std::make_shared<Tournament>();
    auto tGroup = Settings::Manager::instance().getGroupInstance("tournament");
    auto tfile = tGroup->get<std::string>("file");
    
    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    TournamentFile::setSaveCallback(
        tfile,
        tGroup->get<uint32_t>("saveintervalS") * 1000, 
        tournament_, 
        activeEngines);

    tournament_->createTournament(activeEngines, *tournamentConfig);
    TournamentFile::loadGameResults(tfile, tournament_);

    try {
        GameManagerPool& pool = GameManagerPool::getInstance();
        tournament_->scheduleAll(concurrency, true, pool);

        if (background) {
            Logger::reportLogger().log("Task started in background.", TraceLevel::result);
            return code;
        }

        pool.waitForTask();
        
        Logger::reportLogger().log("tournament all games completed", TraceLevel::result);
        
        std::ostringstream oss;
        GameManagerPool::getInstance().getAdjudicationManager().printTestResult(oss);
        Logger::reportLogger().log(oss.str());
        std::string resultString = tournament_->getResultString();
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

AppReturnCode AppRunner::runSprt(AppReturnCode code, bool background) {
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
        sprtManager_ = std::make_shared<SprtManager>();
        auto manager = sprtManager_;
        if (isMontecarlo) {
            manager->runMonteCarloTest(*sprtConfig);
        } else {
            const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
            SprtTournamentFile::setSaveCallback(
                sprtfile,
                sprtGroup->get<unsigned int>("saveintervalS") * 1000,
                manager,
                activeEngines);

            manager->createTournament(activeEngines, *sprtConfig);
            SprtTournamentFile::loadGameResults(sprtfile, manager);

            const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
            GameManagerPool& pool = GameManagerPool::getInstance();
            manager->schedule(manager, concurrency, pool);

            if (background) {
                Logger::reportLogger().log("Task started in background.", TraceLevel::result);
                return code;
            }

            pool.waitForTask();

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

AppReturnCode AppRunner::runSpsa(AppReturnCode code, bool background) {
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
        spsaOptimizer_ = std::make_shared<SPSAOptimizer>();
        spsaOptimizer_->createSPSA(activeEngines.front(), *spsaConfig);
        
        const auto concurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
        GameManagerPool& pool = GameManagerPool::getInstance();
        spsaOptimizer_->scheduleSPSA(concurrency, pool);
        
        if (background) {
            Logger::reportLogger().log("Task started in background.", TraceLevel::result);
            return code;
        }

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

void AppRunner::stop(bool nice) {
    auto& pool = GameManagerPool::getInstance();
    if (nice) {
        pool.setConcurrency(0, true, false);
    }
    else {
        pool.stopAll();
    }
    pool.waitForTask();

    auto& app = AppRunner::instance();
    if (app.currentTask_ == Cli::TaskType::Sprt && app.sprtManager_) {
        app.sprtManager_->save();
    }
    else if (app.currentTask_ == Cli::TaskType::Tournament && app.tournament_) {
        app.tournament_->save();
    }
}

void AppRunner::setConcurrency(int value) {
    GameManagerPool::getInstance().setConcurrency(value, true, true);
}

std::string AppRunner::getRunningGameCount() {
    return std::format("Running games: {}", GameManagerPool::getInstance().runningGameCount());
}

std::string AppRunner::getStatus() {
    auto& app = AppRunner::instance();
    const auto currentTask = app.currentTask_.load();
    const auto runningGames = GameManagerPool::getInstance().runningGameCount();

    Mcp::JsonValue::Object rootObject;
    rootObject["running_games"] = Mcp::JsonHelper::makeNumber(static_cast<double>(runningGames));
    rootObject["current_task"] = Mcp::JsonHelper::makeString(Cli::getTaskId(currentTask));
    rootObject["sprt"] = createSprtStatus(app, currentTask);
    rootObject["tournament"] = createTournamentStatus(app, currentTask);
    rootObject["epd"] = createEpdStatus(app, currentTask);
    rootObject["spsa"] = createSpsaStatus(app, currentTask);

    return Mcp::JsonHelper::serialize(Mcp::JsonHelper::makeObject(std::move(rootObject)));
}

AppReturnCode AppRunner::runDispatcher(bool background) {
    if (GameManagerPool::getInstance().runningGameCount() > 0) {
         throw AppError::makeInvalidParameters("A task is already running. Please stop it first.");
    }

    AppReturnCode returnCode = AppReturnCode::NoError;
    bool hasTask = false;

    setPgnConfig();
    setAdjudicationOptions();

    if (Settings::Manager::instance().getGroupInstance("test")) {
        currentTask_ = Cli::TaskType::Test;
        returnCode = runTest(*Settings::Manager::instance().getGroupInstance("test"), returnCode);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("epd")) {
        currentTask_ = Cli::TaskType::Epd;
        returnCode = runEpd(returnCode, background);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("tournament")) {
        currentTask_ = Cli::TaskType::Tournament;
        returnCode = runTournament(returnCode, background);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("sprt")) {
        currentTask_ = Cli::TaskType::Sprt;
        returnCode = runSprt(returnCode, background);
        hasTask = true;
    }

    if (Settings::Manager::instance().getGroupInstance("spsa")) {
        currentTask_ = Cli::TaskType::Spsa;
        returnCode = runSpsa(returnCode, background);
        hasTask = true;
    }

    if (!hasTask) {
        throw AppError::makeInvalidParameters("No task defined. Please specify at least one task like --test, --epd, --sprt, --tournament, or --spsa.");
    }

    return returnCode;
}

} // namespace QaplaTester
