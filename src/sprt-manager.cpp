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

#include <sstream>
#include <iomanip>
#include <ctime>
#include "epd-reader.h"
#include "sprt-manager.h"
#include "game-manager-pool.h"
#include "logger.h"
#include "pgn-io.h"
#include "engine-config-manager.h"

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
    const EngineConfig& engine0, const EngineConfig& engine1, const SprtConfig& config) {

	config_ = config;
    engine0_ = engine0;
    engine1_ = engine1;

    if (!startPositions_) {
        startPositions_ = std::make_shared<StartPositions>();
    }

    if (config.openings.file.empty()) {
        Logger::testLogger().log("No openings file provided.", TraceLevel::error);
        return;
    }

    if (config.openings.format == "epd" || config.openings.format == "raw") {
        EpdReader reader(config.openings.file);
        for (const auto& entry : reader.all()) {
            if (!entry.fen.empty()) {
                startPositions_->fens.push_back(entry.fen);
            }
        }
    }
    else if (config.openings.format == "pgn") {
        PgnIO pgnReader;
        startPositions_->games = pgnReader.loadGames(config.openings.file);
    }
    else {
        throw AppError::makeInvalidParameters(
            "Unsupported openings format: " + config.openings.format);
    }

    tournamentConfig_.games = config.maxGames;
    tournamentConfig_.repeat = 2;
    tournamentConfig_.round = 0;
    tournamentConfig_.swapColors = true;
    tournamentConfig_.openings = config.openings;

    // Re-initialize tournament, preserving previous results if engines match
    auto savedTournament = std::move(tournament_);
    tournament_ = std::make_unique<PairTournament>();
    tournament_->initialize(engine0_, engine1_, tournamentConfig_, startPositions_);
    if (tournament_->matches(*savedTournament)) {
        tournament_->copyResultsFrom(*savedTournament);
    }
    tournament_->setVerbose(false);
    tournament_->setPositionName("SPRT");
}

void SprtManager::schedule(const std::shared_ptr<SprtManager>& self, uint32_t concurrency, GameManagerPool& pool) {
    
	// Initialize PGN output - at this point all tournament data is loaded
	bool isResumingTournament = tournament_ && tournament_->hasResults();
	PgnIO::tournament().initialize("Sprt", isResumingTournament);
	
    sprtCallback_ = InputHandler::getInstance().registerCommandCallback(
        InputHandler::ImmediateCommand::Info,
        [this](InputHandler::ImmediateCommand, const InputHandler::CommandValue&) {
            auto result = getResult();
            result.printOutcome(std::cout);
        });
	auto duel = tournament_->getResult();
    std::cout << "sprt engines " << duel.getEngineA() << " vs " << duel.getEngineB()
        << " elo [" << config_.eloLower << ", " << config_.eloUpper << "]"
        << " alpha " << config_.alpha << " beta " << config_.beta
        << " maxgames " << config_.maxGames
        << " concurrency " << concurrency << "\n" << std::flush;

    pool.setConcurrency(concurrency, true);
    pool.addTaskProvider(self, tournament_->getEngineA(), tournament_->getEngineB());
    pool.startManagers();
}

std::optional<GameTask> SprtManager::nextTask() {
    auto sprtResult = computeSprt();
    rememberStop_ = rememberStop_ || sprtResult.decision.has_value();
    if (rememberStop_) {
        return std::nullopt;
    }

    return tournament_->nextTask();
}

void SprtManager::setGameRecord(const std::string& taskId, const GameRecord& record) {
	bool engine1IsWhite = tournament_->getEngineA().getName() == record.getWhiteEngineName();
    tournament_->setGameRecord(taskId, record);

    auto [cause, result] = record.getGameResult();
    auto duel = tournament_->getResult();

    auto sprtResult = computeSprt();

    std::ostringstream oss;
    oss << std::left
        << "  match game " << std::setw(4) << record.getRound()
        << " result " << std::setw(7) << to_string(engine1IsWhite ? result : switchGameResult(result))
        << " cause " << std::setw(21) << to_string(cause)
        << " sprt " << sprtResult.info
        << " engines " << duel.toString();

    if (!decision_) {
        Logger::testLogger().log(oss.str(), TraceLevel::result);
    }
    if (sprtResult.decision.has_value()) {
		if (!decision_) {
            GameManagerPool::getInstance().stopAll();
        }
        decision_ = sprtResult.decision;
    }
}

void SprtManager::save(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Failed to open SPRT result file for saving: " + filename);
    }

    out << tournament_->getEngineA() << "\n";
    out << tournament_->getEngineB() << "\n";
    auto section = tournament_->getSectionIfNotEmpty("sprt-tournament");
    if (section) {
        QaplaHelpers::IniFile::saveSection(out, *section);
    }   
}

std::optional<QaplaHelpers::IniFile::Section> SprtManager::getSection() const {
    return tournament_->getSectionIfNotEmpty("sprt-tournament");
}

void SprtManager::loadFromSection(const QaplaHelpers::IniFile::Section& section) {
    try {
        tournament_->fromSection(section);
    } catch (const std::exception& ex) {
        // Ignore invalid section
    }
}

void SprtManager::load(const QaplaHelpers::IniFile::Section& section) {
    try {
        tournament_->fromSection(section);
    } catch (const std::exception& ex) {
        // Ignore invalid section
    }
}

/**
 * @brief Computes the decision boundaries for the SPRT test.
 *
 * Given type I and II error probabilities, this function calculates the lower and upper
 * log-likelihood ratio bounds used to decide acceptance of H0 or H1 in a sequential test.
 *
 * @param alpha Maximum allowed type I error (false positive rate).
 * @param beta Maximum allowed type II error (false negative rate).
 * @return std::tuple<double, double> A pair of log-likelihood thresholds (lowerBound, upperBound).
 */
static std::tuple<double, double> sprtBounds(double alpha, double beta) {
    const double lBound = std::log(beta / (1.0 - alpha));         
    const double uBound = std::log((1.0 - beta) / alpha);         
    return { lBound, uBound };
}

/**
 * @brief Estimates the drawElo parameter based on regularized outcome counts.
 *
 * @param wins number of wins.
 * @param losses number of losses.
 * @param draws number of draws.
 * @return double Estimated drawElo value.
 */
static double computeDrawElo(double wins, double draws, double losses) {
    wins += 0.5;
    draws += 0.5;
    losses += 0.5;

    auto count = wins + draws + losses;
    auto pWin = wins / count;
    auto pLoss = losses / count;
	return 200.0 * std::log10((1.0 - pLoss) / pLoss * (1.0 - pWin) / pWin);
}

/**
 * @brief Computes outcome probabilities based on BayesElo parameters.
 *
 * @param bayesElo Elo difference under the hypothesis.
 * @param drawElo Estimated drawElo value.
 * @return std::tuple<double, double, double> Probabilities for win, draw and loss.
 */
static std::tuple<double, double, double> bayesEloProbabilities(double bayesElo, double drawElo) {
    const double pWin = 1.0 / (1.0 + std::pow(10.0, (drawElo - bayesElo) / 400.0));
    const double pLoss = 1.0 / (1.0 + std::pow(10.0, (drawElo + bayesElo) / 400.0));
    const double pDraw = 1.0 - pWin - pLoss;
    return { pWin, pDraw, pLoss };
}

/**
 * @brief Computes the Log-Likelihood Ratio (LLR) based on observed and expected probabilities.
 *
 * @param wins Regularized number of wins.
 * @param draws Regularized number of draws.
 * @param losses Regularized number of losses.
 * @param p0 Tuple of probabilities under H0 (win, loss, draw).
 * @param p1 Tuple of probabilities under H1 (win, loss, draw).
 * @return double Computed LLR value.
 */
static double computeLLR(double wins, double draws, double losses,
    const std::tuple<double, double, double>& p0,
    const std::tuple<double, double, double>& p1) {
    wins += 0.5;
	losses += 0.5;
	draws += 0.5;
    const auto& [pWin0, pDraw0, pLoss0] = p0;
    const auto& [pWin1, pDraw1, pLoss1] = p1;

    return 
        wins * std::log(pWin1 / pWin0) +
        losses * std::log(pLoss1 / pLoss0) +
        draws * std::log(pDraw1 / pDraw0);
}


SprtResult SprtManager::computeSprt() const {
    auto duel = tournament_->getResult();
    return computeSprt(duel.winsEngineA, duel.draws, duel.winsEngineB, duel.getEngineA(), duel.getEngineB());
}

SprtResult SprtManager::computeSprt(
    int winsA, int draws, int winsB, const std::string& engineA, const std::string& engineB) const {
	const double drawElo = computeDrawElo(winsA, draws, winsB);

    const double x = std::pow(10.0, -drawElo / 400.0); 
	const double xSquare = (x + 1.0) * (x + 1.0); 
    const double scale = 4.0 * x / xSquare;

    const auto p0 = bayesEloProbabilities(config_.eloLower / scale, drawElo);
    const auto p1 = bayesEloProbabilities(config_.eloUpper / scale, drawElo);

    const double llr = computeLLR(winsA, draws, winsB, p0, p1);
    const auto [lBound, uBound] = sprtBounds(config_.alpha, config_.beta);

    SprtResult result;
    result.llr = llr;
    result.lowerBound = lBound;
    result.upperBound = uBound;
    result.drawElo = drawElo;
    result.winsA = winsA;
    result.draws = draws;
    result.winsB = winsB;
    result.engineA = engineA;
    result.engineB = engineB;
    result.eloLower = config_.eloLower;
    result.eloUpper = config_.eloUpper;

    if (llr >= uBound) { 
        result.decision = true;
    } else if (llr <= lBound) {
        result.decision = false;
	} else {
        result.decision = std::nullopt;
    }

    result.info = computeSprtInfo(result);
    
    return result;
}

std::string SprtManager::computeSprtInfo(const SprtResult& result) {
    if (result.decision.has_value()) {
        if (*result.decision) {
            return "H1 accepted, " + result.engineA + " is at least " + std::to_string(result.eloLower)
                + " elo stronger than " + result.engineB;
        }
        return "H0 accepted, " + result.engineA + " is not stronger than " + result.engineB
            + " by at least " + std::to_string(result.eloUpper) + " elo.";
    }
    
    std::ostringstream oss;
    oss << "[ " << std::fixed << std::setprecision(2) << result.lowerBound << " < " 
        << std::setw(5) << result.llr << " < " << result.upperBound << " ]";
    return oss.str();
}

void SprtManager::runMonteCarloSingleTest(int simulationsPerElo, int elo, double drawRate, int64_t &noDecisions, int64_t &numH0, int64_t &numH1, int64_t &totalGames)
{
    for (int sim = 0; sim < simulationsPerElo; ++sim)
    {
        // Check stop flag in outer loop
        if (monteCarloShouldStop_.load()) {
            break;
        }

        // Reset intern
        int winsP1 = 0;
        int winsP2 = 0;
        int draws = 0;
        /*
        if (sim % 1000 == 0) {
            std::cout << "Simulation " << sim << " for Elo " << elo << std::endl;
            double avgGames = (sim > 0) ? static_cast<double>(totalGames) / sim : 0.0;
            std::cout << elo << ", " << correctDecisions << ", " << avgGames << "\n";
        }
        */
        const double trueScore = 1.0 / (1.0 + std::pow(10.0, -elo / 400.0));
        const double winProb = (1.0 - drawRate) * trueScore;

        std::optional<bool> decision;
        uint64_t g = 0;

        for (; g < config_.maxGames; ++g)
        {
            double r = static_cast<double>(rand()) / RAND_MAX;
            if (r < winProb)
            {
                ++winsP1;
            }
            else if (r < winProb + drawRate)
            {
                ++draws;
            }
            else
            {
                ++winsP2;
            }
            auto sprtResult = computeSprt(winsP1, draws, winsP2, "P1", "P2");
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
        totalGames += (g + 1);
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
    constexpr int simulationsPerElo = 1000;
    constexpr double drawRate = 0.4;
    constexpr std::array<int, 11> eloDiffs = { -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25 };

    {
        std::scoped_lock lock(monteCarloResultMutex_);
        monteCarloResult_ = {};
        monteCarloResult_.config = config;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::cout << "Running SPRT Monte carlo simulation: "
        << " | Elo range: [" << config.eloLower << ", " << config.eloUpper << "]"
        << " | alpha: " << config.alpha << ", beta: " << config.beta
        << " | maxGames: " << config.maxGames << "\n" << std::flush;

    for (int elo : eloDiffs) {
        // Check if we should stop
        if (monteCarloShouldStop_.load()) {
            std::cout << "Monte Carlo test stopped early.\n" << std::flush;
            break;
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
        }

        std::cout << std::fixed << std::setprecision(1)
            << "Simulated elo difference: " << std::setw(6) << elo
            << "  No Decisions: " << std::setw(6) << noDecisionPercent << "%"
            << "  H0 Accepted: " << std::setw(6) << h0AcceptedPercent << "%"
            << "  H1 Accepted: " << std::setw(6) << h1AcceptedPercent << "%"
            << "  Average Games: " << std::setw(6) << avgGames << "\n";
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

} // namespace QaplaTester
