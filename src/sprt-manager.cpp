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

bool SprtManager::wait() {
    GameManagerPool::getInstance().waitForTask();
    return true;
};

void SprtManager::createTournament(
    const EngineConfig& engine0, const EngineConfig& engine1, const SprtConfig& config) {

	config_ = config;
    PgnIO::tournament().initialize("Sprt");

    if (!startPositions_) startPositions_ = std::make_shared<StartPositions>();

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
        startPositions_->games = std::move(pgnReader.loadGames(config.openings.file));
    }
    else {
        throw AppError::makeInvalidParameters(
            "Unsupported openings format: " + config.openings.format);
        return;
    }

    PairTournamentConfig ptc;
    ptc.games = config.maxGames;
    ptc.repeat = 2;
    ptc.round = 0;
    ptc.swapColors = true;
    ptc.openings = config.openings;

    tournament_.initialize(engine0, engine1, ptc, startPositions_);
	tournament_.setVerbose(false);
}

void SprtManager::schedule(int concurrency) { 
	auto duel = tournament_.getResult();
    std::cout << "sprt engines " << duel.getEngineA() << " vs " << duel.getEngineB()
        << " elo [" << config_.eloLower << ", " << config_.eloUpper << "]"
        << " alpha " << config_.alpha << " beta " << config_.beta
        << " maxgames " << config_.maxGames
        << " concurrency " << concurrency << std::endl;

    GameManagerPool::getInstance().setConcurrency(concurrency, true);
    GameManagerPool::getInstance().addTaskProvider(this, 
        tournament_.getEngineA(), tournament_.getEngineB(), config_.maxGames);
}

std::optional<GameTask> SprtManager::nextTask() {
    auto result = computeSprt().first;
    rememberStop_ = rememberStop_ || result.has_value();
    if (rememberStop_) return std::nullopt;

    return tournament_.nextTask();
}

void SprtManager::setGameRecord(const std::string& taskId, const GameRecord& record) {
	bool engine1IsWhite = tournament_.getEngineA().getName() == record.getWhiteEngineName();
    tournament_.setGameRecord(taskId, record);

    auto [cause, result] = record.getGameResult();
    auto duel = tournament_.getResult();

    auto [decision, info] = computeSprt();

    std::ostringstream oss;
    oss << std::left
        << "  match game " << std::setw(4) << record.getRound()
        << " result " << std::setw(7) << to_string(engine1IsWhite ? result : switchGameResult(result))
        << " cause " << std::setw(21) << to_string(cause)
        << " sprt " << info
        << " engines " << duel.toString();

    if (!decision_) {
        Logger::testLogger().log(oss.str(), TraceLevel::result);
    }
    if (decision) {
		if (!decision_) GameManagerPool::getInstance().stopAll();
        decision_ = decision;
    }
}

void SprtManager::save(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Failed to open SPRT result file for saving: " + filename);
    }

    out << tournament_.getEngineA() << "\n";
    out << tournament_.getEngineB() << "\n";

    tournament_.trySaveIfNotEmpty(out);
}

void SprtManager::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
		return; // If the file doesn't exist, we simply return without loading anything.
    }

    std::stringstream configStream;
    std::string line;

    while (std::getline(in, line)) {
        if (line.starts_with("[round ")) break;
        configStream << line << "\n";
    }

    EngineConfigManager configLoader;
    configLoader.loadFromStream(configStream);
    const std::unordered_set<std::string> validEngines =
        configLoader.findMatchingNames({tournament_.getEngineA(), tournament_.getEngineB()});

    while (!line.empty()) {
        // Every loader ensures that we have a round header as next line
        auto [round, engineA, engineB] = PairTournament::parseRoundHeader(line);
        if (validEngines.contains(engineA) && validEngines.contains(engineB)) {
            if (tournament_.matches(round - 1, engineA, engineB)) {
                line = tournament_.load(in);
                continue;
            }
        }
        PairTournament tmp;
        line = tmp.load(in);
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
std::tuple<double, double> sprtBounds(double alpha, double beta) {
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
double computeDrawElo(double wins, double draws, double losses) {
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
std::tuple<double, double, double> bayesEloProbabilities(double bayesElo, double drawElo) {
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
double computeLLR(double wins, double draws, double losses,
    const std::tuple<double, double, double>& p0,
    const std::tuple<double, double, double>& p1) {
    wins += 0.5;
	losses += 0.5;
	draws += 0.5;
    const auto [pWin0, pDraw0, pLoss0] = p0;
    const auto [pWin1, pDraw1, pLoss1] = p1;

    return 
        wins * std::log(pWin1 / pWin0) +
        losses * std::log(pLoss1 / pLoss0) +
        draws * std::log(pDraw1 / pDraw0);
}


std::pair<std::optional<bool>, std::string> SprtManager::computeSprt(
    int winsA, int draws, int winsB, std::string engineA, std::string engineB) const {
	const double drawElo = computeDrawElo(winsA, draws, winsB);

    const double x = std::pow(10.0, -drawElo / 400.0); 
	const double xSquare = (x + 1.0) * (x + 1.0); 
    const double scale = 4.0 * x / xSquare;

    const auto p0 = bayesEloProbabilities(config_.eloLower / scale, drawElo);
    const auto p1 = bayesEloProbabilities(config_.eloUpper / scale, drawElo);

    const double llr = computeLLR(winsA, draws, winsB, p0, p1);
    const auto [lBound, uBound] = sprtBounds(config_.alpha, config_.beta);

    if (llr >= uBound) return {
        true,
        "H1 accepted, " + engineA + " is at least " + std::to_string(config_.eloLower)
        + " elo stronger than " + engineB
    };
	if (llr <= lBound) return {
		false,
		"H0 accepted, " + engineA + " is not stronger than " + engineB
        + " by at least " + std::to_string(config_.eloUpper) + " elo."
	};
    std::ostringstream oss;
    oss << "[ " << std::fixed << std::setprecision(2) << lBound << " < " 
        << std::setw(5) << llr << " < " << uBound << " ]";
    return { 
        std::nullopt, oss.str()
    };
}

void SprtManager::runMonteCarloTest(const SprtConfig& config) {
	config_ = config;
    constexpr int simulationsPerElo = 1000;
    constexpr double drawRate = 0.4;
    constexpr std::array<int, 11> eloDiffs = { -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25 };

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::cout << "Running SPRT Monte carlo simulation: "
        << " | Elo range: [" << config.eloLower << ", " << config.eloUpper << "]"
        << " | alpha: " << config.alpha << ", beta: " << config.beta
        << " | maxGames: " << config.maxGames << std::endl;

    for (int elo : eloDiffs) {
        int64_t numH1 = 0;
        int64_t numH0 = 0;
		int64_t noDecisions = 0;
        int64_t totalGames = 0;
                
        for (int sim = 0; sim < simulationsPerElo; ++sim) {
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
            const double lossProb = (1.0 - drawRate) * (1.0 - trueScore);

            std::optional<bool> decision;
            int g = 0;

            for (; g < config_.maxGames; ++g) {
                double r = (double)rand() / RAND_MAX;
                if (r < winProb) ++winsP1;
                else if (r < winProb + drawRate) ++draws;
                else ++winsP2;
				auto [result, info] = computeSprt(winsP1, draws, winsP2, "P1", "P2");
                if (result.has_value()) {
                    decision = result;
                    break;
                }
            }

			if (!decision) {
				++noDecisions;
			}
            else {
				numH0 += *decision ? 0 : 1;
				numH1 += *decision ? 1 : 0;
            }
            totalGames += (g + 1);
        }

        double avgGames = (simulationsPerElo > 0) ? static_cast<double>(totalGames) / simulationsPerElo : 0.0;
        std::cout << std::fixed << std::setprecision(1)
            << "Simulated elo difference: " << std::setw(6) << elo
            << "  No Decisions: " << std::setw(6) << (noDecisions * 100.0) / simulationsPerElo << "%"
            << "  H0 Accepted: " << std::setw(6) << (numH0 * 100.0) / simulationsPerElo << "%"
            << "  H1 Accepted: " << std::setw(6) << (numH1 * 100.0) / simulationsPerElo << "%"
            << "  Average Games: " << std::setw(6) << avgGames << "\n";
    }
}

