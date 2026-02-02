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

#include "sprt-manager.h"
#include "sprt-calculation.h"

#include "../opening/opening-parser.h"
#include "../opening/pgn-save.h"

#include "../game-manager/game-manager-pool.h"

#include "../base-elements/logger.h"

#include <sstream>
#include <iomanip>
#include <ctime>
#include <array>

namespace QaplaTester {

SprtManager::~SprtManager() {
    // Stop any running Monte Carlo test
    stopMonteCarloTest();
    
    // Wait for the thread to finish if it's running
    if (monteCarloThread_.joinable()) {
        monteCarloThread_.join();
    }
}

void SprtManager::createTournament(
    const std::vector<EngineConfig>& engines, const SprtConfig& config) {

    if (engines.size() < 2) {
        throw AppError::makeInvalidParameters(
            "SPRT tournament requires at least 2 engines, got " + std::to_string(engines.size()));
    }

    if (config.pentanomial && config.model == "bayesian") {
        throw AppError::makeInvalidParameters(
            "SPRT: Pentanomial statistics not supported with Bayesian model.");
    }

    AppError::throwOnInvalidOption({ "normalized", "logistic", "bayesian" }, config.model, "Unsupported model for SPRT");

    // Select engine0 (under test) and engine1 (comparison) based on gauntlet flags
    // Find all engines with and without gauntlet flag
    std::vector<EngineConfig> gauntletEngines;
    std::vector<EngineConfig> nonGauntletEngines;
    
    for (const auto& engine : engines) {
        if (engine.isGauntlet()) {
            gauntletEngines.push_back(engine);
        } else {
            nonGauntletEngines.push_back(engine);
        }
    }

    // Selection logic: if exactly one non-gauntlet engine exists, use it as comparison
    // Otherwise fall back to index-based selection for backward compatibility
    if (nonGauntletEngines.size() == 1 && !gauntletEngines.empty()) {
        // Gauntlet mode: first gauntlet engine is under test, non-gauntlet is comparison
        engine0_ = gauntletEngines[0];
        engine1_ = nonGauntletEngines[0];
    } else {
        // Fallback: use first two engines (backward compatibility)
        engine0_ = engines[0];
        engine1_ = engines[1];
    }

    config_ = config;

    if (!startPositions_) {
        startPositions_ = std::make_shared<StartPositions>();
    }

    if (config.openings.file.empty()) {
        throw AppError::makeInvalidParameters("SPRT: No openings file provided.");
    }

    OpeningParser parser;
    startPositions_->games = parser.parse(config.openings.file);

    if (startPositions_->games.empty()) {
        throw AppError::makeInvalidParameters(
            "No valid openings found in file: " + config.openings.file);
    }

    tournamentConfig_.games = config.maxGames;
    tournamentConfig_.repeat = 2;
    tournamentConfig_.round = 0;
    tournamentConfig_.swapColors = true;
    tournamentConfig_.openings = config.openings;

    // Re-initialize tournament, preserving previous results if engines match
    auto savedTournament = std::move(pairing_);
    pairing_ = std::make_unique<PairTournament>();
    pairing_->initialize(engine0_, engine1_, tournamentConfig_, startPositions_);
    if (pairing_->matches(*savedTournament)) {
        pairing_->copyResultsFrom(*savedTournament);
    }
    pairing_->setVerbose(false);
    pairing_->setPositionName("SPRT");

}

void SprtManager::schedule(const std::shared_ptr<SprtManager>& self, uint32_t concurrency, GameManagerPool& pool) {
    
	// Initialize PGN output - at this point all tournament data is loaded
	bool isResumingTournament = pairing_ && pairing_->hasResults();
	PgnSave::tournament().initialize("Sprt", isResumingTournament);
	
    sprtCallback_ = InputHandler::getInstance().registerCommandCallback(
        InputHandler::ImmediateCommand::Info,
        [this](InputHandler::ImmediateCommand, const InputHandler::CommandValue&) {
            auto result = getResult();
            result.printOutcome(std::cout);
        });
	auto duel = pairing_->getResult();
    
    std::string startMsg = std::format("sprt engines {} ({}) vs {} ({}) elo [{}, {}] alpha {} beta {} maxgames {} concurrency {}\n",
        duel.getEngineA(), engine0_.getTimeControl().toPgnTimeControlString(),
        duel.getEngineB(), engine1_.getTimeControl().toPgnTimeControlString(),
        config_.eloLower, config_.eloUpper,
        config_.alpha, config_.beta,
        config_.maxGames,
        concurrency);
    Logger::reportLogger().logStatus(startMsg, "sprt");

    pool.setConcurrency(concurrency, true);
    pool.addTaskProvider(self, pairing_->getEngineA(), pairing_->getEngineB());
    pool.startManagers();
}

std::optional<GameTask> SprtManager::nextTask() {
    return pairing_->nextTask();
}


void SprtManager::setGameRecord(const std::string& taskId, const GameRecord& record) {
	bool engine1IsWhite = pairing_->getEngineA().getName() == record.getWhiteEngineName();
    pairing_->setGameRecord(taskId, record);

    auto [cause, result] = record.getGameResult();
    auto duel = pairing_->getResult();

    uint32_t resultIndex = record.getRound() - 1;

    std::scoped_lock lock(sprtResultsMutex_);

    if (sprtResults_.size() <= resultIndex) {
        sprtResults_.resize(resultIndex + 1);
    }

    SprtResultsPerTournament& resultsForRound = sprtResults_[resultIndex];
    
    if (!resultsForRound.empty() && resultsForRound[0].decision.has_value()) {
        return;
    }

    resultsForRound.clear();

    auto configuredResult = computeSprt(config_.model, config_.pentanomial);
    resultsForRound.push_back(configuredResult);

    for (const auto* model : { "normalized", "logistic", "bayesian" }) {
        if (model != config_.model || config_.pentanomial) {
            auto altResult = computeSprt(model, false);
            resultsForRound.push_back(altResult);
        }
    }
    for (const auto* model : { "normalized", "logistic" }) {
        if (model != config_.model || !config_.pentanomial) {
            auto altPentaResult = computeSprt(model, true);
            resultsForRound.push_back(altPentaResult);
        }
    }

    std::ostringstream oss;
    oss << std::left
        << "match game " << std::setw(4) << record.getGameInRound()
        << " result " << std::setw(7) << to_string(engine1IsWhite ? result : switchGameResult(result))
        << " cause " << std::setw(21) << to_string(cause)
        << " sprt " << configuredResult.info
        << " engines " << duel.toString();

    Logger::reportLogger().logStatus(oss.str(), "sprt", TraceLevel::result);

    if (gameFinishedCallback_) {
        try {
            gameFinishedCallback_();
        } catch (const std::exception& ex) {
            Logger::reportLogger().log("Error in game finished callback: " + 
                std::string(ex.what()), TraceLevel::error);
        }
    }

    finishTournament();
}

std::optional<QaplaHelpers::IniFile::Section> SprtManager::getSection() const {
    return pairing_->getSectionIfNotEmpty("sprt-tournament");
}

void SprtManager::setGameResults(const QaplaHelpers::IniFile::SectionList& sections) {
    if (sections.empty()) {
        return;
    }
    const auto& section = sections[0];
    std::string engineA;
    std::string engineB;
    uint32_t round = 0;
    std::string games;
    try {
         for (const auto& [key, value]: section.entries) {
            if (key == "engineA") {
                engineA = value;
            } else if (key == "engineB") {
                engineB = value;
            } else if (key == "round") {
                round = std::stoul(value) - 1;
            } else if (key == "games") {
                games = value;
            }
        }
        if (!games.empty() && pairing_->matches(round, engineA, engineB)) {
            pairing_->fromSection(section);
        }
    } catch (const std::exception& ex) {
        Logger::reportLogger().log("Failed to load SPRT tournament from section: " + std::string(ex.what()), TraceLevel::error);
    }
}

SprtResult SprtManager::computeSprt(std::optional<std::string> model, std::optional<bool> usePentanomial) const {
    auto duel = pairing_->getResult();
    
    SprtParameters params {
        .winsA = duel.winsEngineA,
        .draws = duel.draws,
        .winsB = duel.winsEngineB,
        .engineA = duel.getEngineA(),
        .engineB = duel.getEngineB(),
        .eloLower = config_.eloLower,
        .eloUpper = config_.eloUpper,
        .alpha = config_.alpha,
        .beta = config_.beta,
        .maxGames = config_.maxGames,
        .model = model ? *model : config_.model,
        .pentanomial = usePentanomial ? *usePentanomial : config_.pentanomial,
        .pentaWW = duel.pentaWW,
        .pentaWD = duel.pentaWD,
        .pentaWL = duel.pentaWL,
        .pentaDD = duel.pentaDD,
        .pentaLD = duel.pentaLD,
        .pentaLL = duel.pentaLL
    };
    
    return FastchessSprt::compute(params);
}

void SprtManager::finishTournament() {
    
    bool allHaveDecisions = true;
    bool anyHasDecision = false;
    
    for (const auto& resultsForRound : sprtResults_) {
        if (resultsForRound.empty() || !resultsForRound[0].decision.has_value()) {
            allHaveDecisions = false;
        } else {
            anyHasDecision = true;
        }
    }
    
    if (anyHasDecision) {
        pairing_->setFinishedIf();
    }
    
    if (allHaveDecisions) {
        GameManagerPool::getInstance().stopAll();
    }
}

void SprtManager::simulateGamePair(float elo, float drawRate, SprtEnginesResult& result) {
    // White advantage bias (added to expected score when playing white)
    constexpr float WHITE_BIAS = 0.05F;
    
    // Base expected score for engine A according to Elo formula
    const float baseExpectedScore = 1.0F / (1.0F + std::pow(10.0F, -elo / 400.0F));
    
    // Simulate two games: engineA as white, then as black
    enum class GameOutcome : uint8_t { Win, Draw, Loss };
    std::array<GameOutcome, 2> gameResults;
    
    for (int game = 0; game < 2; ++game) {
        // Apply white bias: +bias when engineA plays white (game 0), -bias when black (game 1)
        const double biasedExpectedScore = baseExpectedScore + (game == 0 ? WHITE_BIAS : -WHITE_BIAS);
        
        // Reduced draw rate based on distance from 50% score
        const double adaptedDrawRate = drawRate * (0.5 - std::abs(0.5 - biasedExpectedScore)) * 2.0;
        
        // Win probability adjusted for draws
        const double winProb = biasedExpectedScore - (adaptedDrawRate / 2.0);
        
        double r = static_cast<double>(rand()) / RAND_MAX;
        if (r < winProb) {
            gameResults[game] = GameOutcome::Win;
            ++result.winsA;
        } else if (r < winProb + adaptedDrawRate) {
            gameResults[game] = GameOutcome::Draw;
            ++result.draws;
        } else {
            gameResults[game] = GameOutcome::Loss;
            ++result.winsB;
        }
    }

    // Update pentanomial statistics (both games from engineA's perspective)
    if (gameResults[0] == GameOutcome::Win && gameResults[1] == GameOutcome::Win) {
        ++result.pentaWW;
    } else if ((gameResults[0] == GameOutcome::Win && gameResults[1] == GameOutcome::Draw) ||
               (gameResults[0] == GameOutcome::Draw && gameResults[1] == GameOutcome::Win)) {
        ++result.pentaWD;
    } else if ((gameResults[0] == GameOutcome::Win && gameResults[1] == GameOutcome::Loss) ||
               (gameResults[0] == GameOutcome::Loss && gameResults[1] == GameOutcome::Win)) {
        ++result.pentaWL;
    } else if (gameResults[0] == GameOutcome::Draw && gameResults[1] == GameOutcome::Draw) {
        ++result.pentaDD;
    } else if ((gameResults[0] == GameOutcome::Loss && gameResults[1] == GameOutcome::Draw) ||
               (gameResults[0] == GameOutcome::Draw && gameResults[1] == GameOutcome::Loss)) {
        ++result.pentaLD;
    } else if (gameResults[0] == GameOutcome::Loss && gameResults[1] == GameOutcome::Loss) {
        ++result.pentaLL;
    }
}

SprtResult SprtManager::computeSprt(
    const SprtEnginesResult& result, const std::string& engineA, const std::string& engineB) const {
    return FastchessSprt::compute({
        .winsA = result.winsA,
        .draws = result.draws,
        .winsB = result.winsB,
        .engineA = engineA,
        .engineB = engineB,
        .eloLower = config_.eloLower,
        .eloUpper = config_.eloUpper,
        .alpha = config_.alpha,
        .beta = config_.beta,
        .maxGames = config_.maxGames,
        .model = config_.model,
        .pentanomial = config_.pentanomial,
        .pentaWW = result.pentaWW,
        .pentaWD = result.pentaWD,
        .pentaWL = result.pentaWL,
        .pentaDD = result.pentaDD,
        .pentaLD = result.pentaLD,
        .pentaLL = result.pentaLL
    });
}

void SprtManager::runMonteCarloSingleTest(
    int simulationsPerElo, float elo, float drawRate, 
    int64_t &noDecisions, int64_t &numH0, int64_t &numH1, int64_t &totalGames)
{
    for (int sim = 0; sim < simulationsPerElo; ++sim)
    {
        // Check stop flag in outer loop
        if (monteCarloShouldStop_.load()) {
            break;
        }

        SprtEnginesResult result;
        std::optional<bool> decision;
        int64_t gamePairsPlayed = 0;
        const int64_t maxGamePairs = config_.maxGames / 2;

        for (; gamePairsPlayed < maxGamePairs; ++gamePairsPlayed)
        {
            // Simulate a pair of games (white/black swap)
            simulateGamePair(elo, drawRate, result);

            // Check for decision every 50 game pairs or at the end
            if (gamePairsPlayed % 50 != 0 && gamePairsPlayed + 1 != maxGamePairs)
            {
                continue;
            }
            
            auto sprtResult = computeSprt(result, "P1", "P2");
            if (sprtResult.decision.has_value())
            {
                decision = sprtResult.decision;
                break;
            }
        }

        if (!decision)
        {
            ++noDecisions;
        }
        else
        {
            numH0 += *decision ? 0 : 1;
            numH1 += *decision ? 1 : 0;
        }
        // Total games = pairs * 2
        totalGames += (gamePairsPlayed + 1) * 2;
    }
}


bool SprtManager::runMonteCarloTest(const SprtConfig& config) {
    // Try to acquire the test lock
    if (monteCarloTestRunning_.exchange(true)) {
        return false;  // Test already running
    }

    // Join previous thread if it exists
    if (monteCarloThread_.joinable()) {
        monteCarloThread_.join();
    }

    // Clear results before starting new test
    clearMonteCarloResult();

    // Reset stop flag
    monteCarloShouldStop_.store(false);

    // Start the test in a background thread
    monteCarloThread_ = std::thread([this, config]() {
        runMonteCarloTestInternal(config);
        // Mark as finished
        monteCarloTestRunning_.store(false);
    });

    return true;  // Test started successfully
}

void SprtManager::runMonteCarloTestInternal(const SprtConfig& config) {
	config_ = config;
    constexpr int simulationsPerElo = 2000;
    constexpr float drawRate = 0.4F;
    
    // Calculate dynamic step size rounded to one decimal place
    float rawStep = std::abs(config.eloUpper - config.eloLower) / 5.0F;
    float step = std::round(rawStep * 10.0F) / 10.0F;
    
    // Generate test range: 2 steps below eloLower to 2 steps above eloUpper
    float startElo = config.eloLower - 2.0F * step;
    float endElo = config.eloUpper + 2.0F * step;
    
    std::vector<float> eloDiffs;
    int numSteps = static_cast<int>(std::round((endElo - startElo) / step)) + 1;
    for (int i = 0; i < numSteps; ++i) {
        float elo = startElo + static_cast<float>(i) * step;
        eloDiffs.push_back(std::round(elo * 10.0F) / 10.0F);
    }

    {
        std::scoped_lock lock(monteCarloResultMutex_);
        monteCarloResult_ = {};
        monteCarloResult_.config = config;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::string mcStartMsg = std::format("Running SPRT Monte carlo simulation: | Elo range: [{}, {}] | alpha: {}, beta: {} | maxGames: {} | step: {}",
        config.eloLower, config.eloUpper, config.alpha, config.beta, config.maxGames, step);
    Logger::reportLogger().logStatus(mcStartMsg, "sprt");

    std::vector<std::thread> threads;
    threads.reserve(eloDiffs.size());
    
    for (float elo : eloDiffs) {
        threads.emplace_back([this, elo]() {
            // Check if we should stop
            if (monteCarloShouldStop_.load()) {
                return;
            }

            int64_t numH1 = 0;
            int64_t numH0 = 0;
            int64_t noDecisions = 0;
            int64_t totalGames = 0;

            runMonteCarloSingleTest(simulationsPerElo, elo, drawRate, noDecisions, numH0, numH1, totalGames);

            double avgGames = (simulationsPerElo > 0) ? static_cast<double>(totalGames) / simulationsPerElo : 0.0;
            double noDecisionPercent = (static_cast<double>(noDecisions) * 100.0) / simulationsPerElo;
            double h0AcceptedPercent = (static_cast<double>(numH0) * 100.0) / simulationsPerElo;
            double h1AcceptedPercent = (static_cast<double>(numH1) * 100.0) / simulationsPerElo;

            {
                std::scoped_lock lock(monteCarloResultMutex_);
                monteCarloResult_.rows.push_back({
                    .eloDifference = elo,
                    .noDecisionPercent = noDecisionPercent,
                    .h0AcceptedPercent = h0AcceptedPercent,
                    .h1AcceptedPercent = h1AcceptedPercent,
                    .avgGames = avgGames
                });
                std::ranges::sort(monteCarloResult_.rows,
                    [](const MonteCarloResultRow& a, const MonteCarloResultRow& b) {
                        return a.eloDifference < b.eloDifference;
                    });
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (monteCarloShouldStop_.load()) {
        Logger::reportLogger().logStatus("Monte Carlo test stopped early.", "sprt");
    }

    // Sort results by eloDifference and output
    {
        std::scoped_lock lock(monteCarloResultMutex_);

        for (const auto& row : monteCarloResult_.rows) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1)
                << "Simulated elo difference: " << std::setw(6) << row.eloDifference
                << "  No Decisions: " << std::setw(6) << row.noDecisionPercent << "%"
                << "  H0 Accepted: " << std::setw(6) << row.h0AcceptedPercent << "%"
                << "  H1 Accepted: " << std::setw(6) << row.h1AcceptedPercent << "%"
                << "  Average Games: " << std::setw(6) << row.avgGames;
            Logger::reportLogger().logStatus(oss.str(), "sprt");
        }
    }
}

void SprtManager::stopMonteCarloTest() {
    monteCarloShouldStop_.store(true);
}

void SprtManager::clearMonteCarloResult() {
    std::scoped_lock lock(monteCarloResultMutex_);
    monteCarloResult_.rows.clear();
}

void SprtManager::withMonteCarloResult(const std::function<void(const MonteCarloResult&)>& callback) {
    std::scoped_lock lock(monteCarloResultMutex_);
    callback(monteCarloResult_);
}

void SprtManager::logFinalResult() const {
    if (sprtResults_.empty() || sprtResults_.front().empty()) {
        return;
    }
    
    const auto& finalResult = sprtResults_.front().front();
    
    std::ostringstream oss;
    oss << "SPRT final result: " << finalResult.info;
    if (finalResult.decision.has_value()) {
        oss << " | decision: " << (*finalResult.decision ? "H1 Accepted" : "H0 Accepted");
    } else {
        oss << " | decision: Inconclusive";
    }
    oss << " | LLR: " << std::fixed << std::setprecision(2) << finalResult.llr
        << " | games: " << (finalResult.winsA + finalResult.draws + finalResult.winsB)
        << " (W:" << finalResult.winsA << " D:" << finalResult.draws << " L:" << finalResult.winsB << ")";
    
    Logger::reportLogger().logStatus(oss.str(), "sprt", TraceLevel::result);
}

} // namespace QaplaTester
