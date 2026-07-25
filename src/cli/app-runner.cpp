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
#include "../clop/clop-optimizer.h"
#include "../system-test/system-test-manager.h"
#include "../tournament/tournament.h"
#include "../tournament/tournament-file.h"
#include "../opening/pgn-save.h"
#include "../game-manager/game-manager-pool.h"
#include "../game-manager/adjudication-manager.h"
#include "../game-manager/game-state.h"
#include "../perft/perft.h"
#include "../base-elements/logger.h"
#include "../base-elements/oss-tools.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/table-format.h"
#include "../base-elements/timer.h"
#include "settings-manager.h"

#include <format>
#include <sstream>

namespace QaplaTester {

namespace {

[[nodiscard]] Json::JsonValue createSprtStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Sprt || !app.getSprtManager()) {
        return Json::JsonValue::object();
    }

    const auto result = app.getSprtManager()->computeSprt();
    const auto duel = app.getSprtManager()->getDuelResult();
    auto status = Json::JsonValue::object();
    status["llr"] = result.llr;
    status["lower_bound"] = result.lowerBound;
    status["upper_bound"] = result.upperBound;
    status["elo_h0"] = result.eloH0;
    status["elo_h1"] = result.eloH1;
    status["games"] = static_cast<double>(duel.total());
    status["win_rate_percent"] = duel.engineARate() * 100.0;
    status["wins"] = static_cast<double>(result.winsA);
    status["losses"] = static_cast<double>(result.winsB);
    status["draws"] = static_cast<double>(result.draws);
    return status;
}

[[nodiscard]] Json::JsonValue createTournamentStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Tournament || !app.getTournament()) {
        return Json::JsonValue::object();
    }

    auto status = Json::JsonValue::object();
    status["rating"] = TableFormat::toJsonValue("ratingTable", app.getTournament()->getRatingStatusTable());
    status["outcome"] = TableFormat::toJsonValue("outcome", app.getTournament()->getOutcomeStatusTable());
    return status;
}

[[nodiscard]] Json::JsonValue createEpdStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Epd || !app.getEpdManager()) {
        return Json::JsonValue::object();
    }

    const auto statusResult = app.getEpdManager()->getStatusResult();
    auto status = Json::JsonValue::object();
    status["results"] = TableFormat::toJsonValue("epdStatus", statusResult.statusTable);
    status["total_nodes"] = static_cast<double>(statusResult.totalNodes);
    status["solved"] = static_cast<double>(statusResult.solvedCount);
    status["total"] = static_cast<double>(statusResult.totalCount);
    status["hit_rate_percent"] = statusResult.hitRatePercent;
    return status;
}

[[nodiscard]] Json::JsonValue createSpsaStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Spsa || !app.getSPSAOptimizer()) {
        return Json::JsonValue::object();
    }

    auto status = Json::JsonValue::object();
    status["parameters"] = TableFormat::toJsonValue("spsaStatus", app.getSPSAOptimizer()->getStatusTable());
    status["completed_iterations"] = static_cast<double>(app.getSPSAOptimizer()->getCompletedIterations());
    return status;
}

[[nodiscard]] Json::JsonValue createClopStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::Clop || !app.getCLOPOptimizer()) {
        return Json::JsonValue::object();
    }

    auto status = Json::JsonValue::object();
    status["parameters"] = TableFormat::toJsonValue("clopStatus", app.getCLOPOptimizer()->getStatusTable());
    status["completed_samples"] = static_cast<double>(app.getCLOPOptimizer()->getCompletedSamples());
    return status;
}

[[nodiscard]] Json::JsonValue createSystemTestStatus(const AppRunner& app, Cli::TaskType currentTask) {
    if (currentTask != Cli::TaskType::SystemTest || !app.getSystemTestManager()) {
        return Json::JsonValue::object();
    }

    auto status = Json::JsonValue::object();
    status["finished"] = app.getSystemTestManager()->isFinished();
    return status;
}

[[nodiscard]] Json::JsonValue createTaskStatusFor(const AppRunner& app, Cli::TaskType taskType) {
    switch (taskType) {
        case Cli::TaskType::Sprt:
            return createSprtStatus(app, Cli::TaskType::Sprt);
        case Cli::TaskType::Tournament:
            return createTournamentStatus(app, Cli::TaskType::Tournament);
        case Cli::TaskType::Epd:
            return createEpdStatus(app, Cli::TaskType::Epd);
        case Cli::TaskType::Spsa:
            return createSpsaStatus(app, Cli::TaskType::Spsa);
        case Cli::TaskType::Clop:
            return createClopStatus(app, Cli::TaskType::Clop);
        case Cli::TaskType::SystemTest:
            return createSystemTestStatus(app, Cli::TaskType::SystemTest);
        default:
            return Json::JsonValue::object();
    }
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

struct ConcurrencyResolution {
    uint32_t configuredConcurrency = 0;
    int physicalCoreCount = 0;
    uint32_t effectiveConcurrency = 1;
    bool usesAutoMode = false;
    bool usedFallback = false;
};

[[nodiscard]] ConcurrencyResolution resolveConfiguredConcurrency() {
    ConcurrencyResolution resolution;
    resolution.configuredConcurrency = Settings::Manager::instance().get<unsigned int>("concurrency");
    resolution.usesAutoMode = resolution.configuredConcurrency == 0U;

    if (!resolution.usesAutoMode) {
        resolution.effectiveConcurrency = resolution.configuredConcurrency;
        return resolution;
    }

    resolution.physicalCoreCount = QaplaHelpers::getPhysicalCoreCount();
    if (resolution.physicalCoreCount <= 1) {
        resolution.effectiveConcurrency = 1U;
        resolution.usedFallback = true;
        return resolution;
    }

    resolution.effectiveConcurrency = static_cast<uint32_t>(resolution.physicalCoreCount - 1);
    return resolution;
}

void logStartConcurrency(std::string_view toolName, const ConcurrencyResolution& resolution) {
    if (!resolution.usesAutoMode) {
        Logger::reportLogger().logStatus(
            std::format(
                "Concurrency resolved (tool={}): configured={}, physical_cores=n/a, effective={}, mode=manual",
                toolName,
                resolution.configuredConcurrency,
                resolution.effectiveConcurrency),
            toolName,
            TraceLevel::result);
        return;
    }

    Logger::reportLogger().logStatus(
        std::format(
            "Concurrency resolved (tool={}): configured=0, physical_cores={}, effective={}, mode=auto{}, formula=max(1, physical-1)",
            toolName,
            resolution.physicalCoreCount,
            resolution.effectiveConcurrency,
            resolution.usedFallback ? " fallback" : ""),
        toolName,
        TraceLevel::result);
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

AppReturnCode AppRunner::runPerft(const Settings::GroupInstance& perft, AppReturnCode code) {
    Settings::QaplaSettings::instance().applyLoggerConfig("perft-report");

    const auto position = perft.get<std::string>("position");
    const auto depth = perft.get<uint32_t>("depth");
    const auto divide = perft.get<bool>("divide");
    const auto showFen = perft.get<bool>("showfen");
    const bool isStartPos = QaplaHelpers::to_lowercase(position) == "startpos";

    GameState state;
    if (!state.setFen(isStartPos, position)) {
        throw AppError::makeInvalidParameters("Invalid FEN for perft: '" + position + "'");
    }

    const auto concurrencyResolution = resolveConfiguredConcurrency();
    logStartConcurrency("perft", concurrencyResolution);
    Logger::reportLogger().logStatus(
        std::format("Running perft: position='{}', depth={}, concurrency={}",
            isStartPos ? "startpos" : position, depth, concurrencyResolution.effectiveConcurrency),
        "perft", TraceLevel::result);

    QaplaHelpers::Timer timer;
    timer.start();
    const auto result = Perft::run(state, depth, concurrencyResolution.effectiveConcurrency, divide, showFen);
    timer.stop();

    if (divide && !result.divide.empty()) {
        TableData table;
        table.headers = { "Move", "Nodes" };
        table.columnWidths = { 10, 16 };
        if (showFen) {
            table.headers.emplace_back("Fen after move");
            table.columnWidths.push_back(70);
        }
        for (const auto& entry : result.divide) {
            std::vector<TableCell> row;
            row.emplace_back(entry.move);
            row.emplace_back(entry.nodes);
            if (showFen) {
                row.emplace_back(entry.fenAfter);
            }
            table.body.push_back(std::move(row));
        }
        Logger::reportLogger().logTable("perftDivide", table, TraceLevel::result);
    }

    const auto elapsedMs = timer.elapsedMs();
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;
    const double nps = elapsedSeconds > 0.0 ? static_cast<double>(result.nodes) / elapsedSeconds : 0.0;

    Logger::reportLogger().logStatus(
        std::format("perft finished: depth={} nodes={} time={} nps={:.0f}",
            depth, result.nodes, QaplaHelpers::formatMs(elapsedMs), nps),
        "perft", TraceLevel::result);

    return code;
}

AppReturnCode AppRunner::runEpd(AppReturnCode code, bool background) {
    const auto& epdConfig = Settings::QaplaSettings::instance().getEpdConfig();
    if (!epdConfig) {
        return code;
    }

    const auto concurrencyResolution = resolveConfiguredConcurrency();
    const auto concurrency = concurrencyResolution.effectiveConcurrency;
    Settings::QaplaSettings::instance().applyLoggerConfig("epd-report");
    logStartConcurrency("epd", concurrencyResolution);
    epdManager_ = std::make_shared<EpdManager>();

    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        const auto& name = engine.getName();
        std::string searchMode = std::format("Max Time: {}s Early stop - Seen plies: {} Min time: {}s",
            epdConfig->maxTime,
            epdConfig->seenPlies,
            epdConfig->minTime);
        if (epdConfig->depth != 0) {
            searchMode = std::format("Fixed depth: {} (maxtime/mintime/seenplies are ignored)", epdConfig->depth);
        } else if (epdConfig->nodes != 0) {
            searchMode = std::format("Fixed nodes: {} (maxtime/mintime/seenplies are ignored)", epdConfig->nodes);
        }

        Logger::reportLogger().log(std::format("Using engine: {} Concurrency: {} {}",
            name,
            concurrency,
            searchMode));
        epdManager_->initialize(epdConfig->file, epdConfig->maxTime, epdConfig->minTime, epdConfig->seenPlies,
            epdConfig->depth, epdConfig->nodes);
        GameManagerPool& pool = GameManagerPool::getInstance();

        pool.setConcurrency(concurrency, true);
        epdManager_->schedule(engine, pool);
        
        if (background) {
            Logger::reportLogger().log("Task started in background.", TraceLevel::result);
            return code;
        }

        pool.waitForTask();
        const auto statusResult = epdManager_->getStatusResult();
        Logger::reportLogger().log(std::format(
            "Finished EPD test for engine: {}, success rate: {:.2f}%, total nodes: {}",
            name,
            statusResult.hitRatePercent,
            statusResult.totalNodes));
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

    const auto concurrencyResolution = resolveConfiguredConcurrency();
    const auto concurrency = concurrencyResolution.effectiveConcurrency;
    Settings::QaplaSettings::instance().applyLoggerConfig("tournament-report");
    logStartConcurrency("tournament", concurrencyResolution);

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
            if (!background) {
                manager->waitMonteCarloCompletion();
            }
        } else {
            const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
            SprtTournamentFile::setSaveCallback(
                sprtfile,
                sprtGroup->get<unsigned int>("saveintervalS") * 1000,
                manager,
                activeEngines);

            manager->createTournament(activeEngines, *sprtConfig);
            SprtTournamentFile::loadGameResults(sprtfile, manager);

            const auto concurrencyResolution = resolveConfiguredConcurrency();
            const auto concurrency = concurrencyResolution.effectiveConcurrency;
            logStartConcurrency("sprt", concurrencyResolution);
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
        
        const auto concurrencyResolution = resolveConfiguredConcurrency();
        const auto concurrency = concurrencyResolution.effectiveConcurrency;
        logStartConcurrency("spsa", concurrencyResolution);
        GameManagerPool& pool = GameManagerPool::getInstance();
        spsaOptimizer_->scheduleSPSA(concurrency, pool);
        
        if (background) {
            Logger::reportLogger().logStatus("Task started in background.", "spsa", TraceLevel::result);
            return code;
        }

        pool.waitForTask();
        
        Logger::reportLogger().logStatus("SPSA optimization completed.", "spsa", TraceLevel::result);
    }
    catch (const std::exception& e) {
        Logger::reportLogger().logStatus(
            std::format("Exception during SPSA run: {}", e.what()), "spsa", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

AppReturnCode AppRunner::runClop(AppReturnCode code, bool background) {
    const auto& clopConfig = Settings::QaplaSettings::instance().getCLOPConfig();
    if (!clopConfig) {
        return code;
    }

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires at least one engine.");
    }

    if (!Settings::QaplaSettings::instance().getOpenings()) {
        throw AppError::makeInvalidParameters("No openings defined for CLOP optimization. Please define an opening.");
    }

    checkTimeControl();
    Settings::QaplaSettings::instance().applyLoggerConfig("clop-report");

    try {
        clopOptimizer_ = std::make_shared<CLOPOptimizer>();
        clopOptimizer_->createCLOP(activeEngines, *clopConfig);

        const auto concurrencyResolution = resolveConfiguredConcurrency();
        const auto concurrency = concurrencyResolution.effectiveConcurrency;
        logStartConcurrency("clop", concurrencyResolution);
        GameManagerPool& pool = GameManagerPool::getInstance();
        clopOptimizer_->scheduleCLOP(concurrency, pool);

        if (background) {
            Logger::reportLogger().logStatus("Task started in background.", "clop", TraceLevel::result);
            return code;
        }

        clopOptimizer_->waitUntilFinished();
        pool.waitForTask();

        Logger::reportLogger().logStatus("CLOP optimization completed.", "clop", TraceLevel::result);
    }
    catch (const std::exception& exception) {
        Logger::reportLogger().logStatus(
            std::format("Exception during CLOP run: {}", exception.what()), "clop", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

AppReturnCode AppRunner::runSystemTest(AppReturnCode code, bool background) {
    const auto systemTestGroup = Settings::Manager::instance().getGroupInstance("systemtest");
    if (!systemTestGroup.has_value()) {
        return code;
    }

    const auto& activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.empty()) {
        throw AppError::makeInvalidParameters("No engine defined for systemtest.");
    }

    checkTimeControl();
    Settings::QaplaSettings::instance().applyLoggerConfig("systemtest-report");

    const auto concurrencyResolution = resolveConfiguredConcurrency();
    const auto configuredMaxCores = systemTestGroup->get<uint32_t>("maxcores");
    const auto resolvedMaxCores = configuredMaxCores == 0 ? concurrencyResolution.effectiveConcurrency : configuredMaxCores;
    if (resolvedMaxCores == 0) {
        throw AppError::makeInvalidParameters("systemtest.maxcores resolved to 0.");
    }

    const SystemTestConfig config {
        .maxCores = resolvedMaxCores,
        .step = systemTestGroup->get<uint32_t>("step"),
        .stepTimeSeconds = systemTestGroup->get<uint32_t>("steptime"),
        .test = systemTestGroup->get<bool>("test")
    };

    try {
        systemTestManager_ = std::make_shared<SystemTestManager>();
        systemTestManager_->initialize(activeEngines.front(), config);
        GameManagerPool& pool = GameManagerPool::getInstance();
        systemTestManager_->schedule(systemTestManager_, pool);

        if (background) {
            Logger::reportLogger().logStatus("Task started in background.", "systemtest", TraceLevel::result);
            return code;
        }

        pool.waitForTask();
        Logger::reportLogger().logStatus("systemtest all games completed", "systemtest", TraceLevel::result);
    }
    catch (const std::exception& exception) {
        Logger::reportLogger().logStatus(
            std::format("Exception during systemtest run: {}", exception.what()),
            "systemtest",
            TraceLevel::error);
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
    else if (app.currentTask_ == Cli::TaskType::Clop && app.clopOptimizer_) {
        app.clopOptimizer_->stop();
    }
}

void AppRunner::setConcurrency(int value) {
    GameManagerPool::getInstance().setConcurrency(value, true, true);
}

std::string AppRunner::getRunningGameCount() {
    return std::format("Running games: {}", GameManagerPool::getInstance().runningGameCount());
}

Json::JsonValue AppRunner::getStatus() {
    auto& app = AppRunner::instance();
    const auto currentTask = app.currentTask_.load();
    const auto runningGames = GameManagerPool::getInstance().runningGameCount();

    auto root = Json::JsonValue::object();
    root["running_games"] = static_cast<double>(runningGames);
    root["current_task"] = Cli::getTaskId(currentTask);
    root["sprt"] = createTaskStatusFor(app, Cli::TaskType::Sprt);
    root["tournament"] = createTaskStatusFor(app, Cli::TaskType::Tournament);
    root["epd"] = createTaskStatusFor(app, Cli::TaskType::Epd);
    root["spsa"] = createTaskStatusFor(app, Cli::TaskType::Spsa);
    root["clop"] = createTaskStatusFor(app, Cli::TaskType::Clop);
    root["systemtest"] = createTaskStatusFor(app, Cli::TaskType::SystemTest);

    return root;
}

Json::JsonValue AppRunner::getTaskStatus(Cli::TaskType taskType) {
    const auto& app = AppRunner::instance();
    return createTaskStatusFor(app, taskType);
}

AppReturnCode AppRunner::runDispatcher(bool background, Cli::TaskType forcedTask) {
    if (GameManagerPool::getInstance().runningGameCount() > 0) {
         throw AppError::makeInvalidParameters("A task is already running. Please stop it first.");
    }

    static bool hasExecutedDispatcherRun = false;
    // An initial log file is created on application start, we ensure to use it.
    if (hasExecutedDispatcherRun) {
        Logger::reportLogger().startNewRunLogFile();
    }

    AppReturnCode returnCode = AppReturnCode::NoError;
    bool hasTask = false;
    const auto shouldRunTask = [forcedTask](Cli::TaskType taskType) {
        return forcedTask == Cli::TaskType::None || forcedTask == taskType;
    };

    setPgnConfig();
    setAdjudicationOptions();

    if (shouldRunTask(Cli::TaskType::Test) && Settings::Manager::instance().getGroupInstance("test")) {
        currentTask_ = Cli::TaskType::Test;
        returnCode = runTest(*Settings::Manager::instance().getGroupInstance("test"), returnCode);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Epd) && Settings::Manager::instance().getGroupInstance("epd")) {
        currentTask_ = Cli::TaskType::Epd;
        returnCode = runEpd(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Tournament) && Settings::Manager::instance().getGroupInstance("tournament")) {
        currentTask_ = Cli::TaskType::Tournament;
        returnCode = runTournament(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Sprt) && Settings::Manager::instance().getGroupInstance("sprt")) {
        currentTask_ = Cli::TaskType::Sprt;
        returnCode = runSprt(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Spsa) && Settings::Manager::instance().getGroupInstance("spsa")) {
        currentTask_ = Cli::TaskType::Spsa;
        returnCode = runSpsa(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Clop) && Settings::Manager::instance().getGroupInstance("clop")) {
        currentTask_ = Cli::TaskType::Clop;
        returnCode = runClop(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::SystemTest) && Settings::Manager::instance().getGroupInstance("systemtest")) {
        currentTask_ = Cli::TaskType::SystemTest;
        returnCode = runSystemTest(returnCode, background);
        hasTask = true;
    }

    if (shouldRunTask(Cli::TaskType::Perft) && Settings::Manager::instance().getGroupInstance("perft")) {
        currentTask_ = Cli::TaskType::Perft;
        returnCode = runPerft(*Settings::Manager::instance().getGroupInstance("perft"), returnCode);
        hasTask = true;
    }

    if (!hasTask) {
        if (forcedTask != Cli::TaskType::None) {
            throw AppError::makeInvalidParameters(std::format(
                "No '{}' task configuration found. The selected task is forced in MCP mode.",
                Cli::getTaskId(forcedTask)));
        }
        throw AppError::makeInvalidParameters("No task defined. Please specify at least one task like --test, --epd, --sprt, --tournament, --spsa, --clop, --systemtest, or --perft.");
    }

    hasExecutedDispatcherRun = true;

    return returnCode;
}

} // namespace QaplaTester
