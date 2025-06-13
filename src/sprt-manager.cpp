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

bool SprtManager::wait() {
    GameManagerPool::getInstance().waitForTask(this);
    return true;
};

void SprtManager::run(
    const EngineConfig& engine0, const EngineConfig& engine1, int concurrency, const SprtConfig& config) {
	config_ = config;
	engineP1Name_ = engine0.getName();
	engineP2Name_ = engine1.getName();

	if (config.openings.file.empty()) {
		Logger::testLogger().log("No openings file provided, using default EPD test set.", TraceLevel::error);
		return;
	}
	if (config.openings.format != "epd" && config.openings.format != "raw") {
		Logger::testLogger().log("Unsupported openings format: " + config.openings.format, TraceLevel::error);
		return;
	}
	EpdReader reader(config.openings.file);
    std::cout << "Running SPRT: " << engineP1Name_ << " vs " << engineP2Name_
        << " | Elo range: [" << config.eloLower << ", " << config.eloUpper << "]"
        << " | alpha: " << config.alpha << ", beta: " << config.beta
        << " | maxGames: " << config.maxGames
        << " | concurrency: " << concurrency << std::endl;
	for (auto& entry : reader.all()) {
		if (entry.fen.empty()) {
			continue;
		}
		startPositions_.push_back(entry.fen);
	}
    winsP1_ = 0;
	winsP2_ = 0;
    gamesStarted_ = 0;
	draws_ = 0;
	nextIndex_ = 0;
    PgnIO::tournament().initialize();
	GameManagerPool::getInstance().setConcurrency(concurrency, true);
	GameManagerPool::getInstance().addTask(this, engine0, engine1);

}

std::optional<GameTask> SprtManager::nextTask(
    const std::string& whiteId,
    const std::string& blackId) {
    if (startPositions_.empty()) return std::nullopt;
    if (config_.maxGames > 0 && gamesStarted_ >= static_cast<uint32_t>(config_.maxGames)) return std::nullopt;
    auto result = computeSprt().first;
    rememberStop_ = rememberStop_ || result.has_value();
    if (rememberStop_) return std::nullopt;
    
    if (gamesStarted_ % 2 == 0 && config_.openings.order == "random") {
        nextIndex_ = std::rand() % startPositions_.size();
    }
    size_t index = nextIndex_;

    GameState gameState;
    gameState.setFen(false, startPositions_[index]);
    auto correctedFen = gameState.getFen();
    GameTask task;
    task.taskType = GameTask::Type::PlayGame;
    task.useStartPosition = false;
    task.fen = correctedFen;
    task.whiteTimeControl = config_.tc;
    task.blackTimeControl = config_.tc;
    task.switchSide = (gamesStarted_ % 2) == 1;
    task.round = gamesStarted_ + 1;

    ++gamesStarted_;
    if ((gamesStarted_ % 2) == 0 && config_.openings.order != "random") {
        ++nextIndex_;
        if (nextIndex_ >= startPositions_.size()) nextIndex_ = 0;
    }

    return task;
}

void SprtManager::setGameRecord(const std::string& whiteId,
    const std::string& blackId,
    const GameRecord& record) {
    auto [cause, result] = record.getGameResult();

    bool engine0White = engineP1Name_ == record.getWhiteEngineName();

    std::string resultText;

    switch (result) {
    case GameResult::WhiteWins:
        if (engine0White) {
            ++winsP1_;
            resultText = engineP1Name_ + " (White) wins";
        }
        else {
            ++winsP2_;
            resultText = engineP2Name_ + " (White) wins";
        }
        break;
    case GameResult::BlackWins:
        if (engine0White) {
            ++winsP2_;
            resultText = engineP2Name_ + " (Black) wins";
        }
        else {
            ++winsP1_;
            resultText = engineP1Name_ + " (Black) wins";
        }
        break;
    case GameResult::Draw:
        ++draws_;
        resultText = "Draw";
        break;
    case GameResult::Unterminated:
        resultText = "Unterminated game";
        break;
    }
    auto [decision, info] = computeSprt();
    std::ostringstream oss; 
    oss << engineP1Name_ 
        << " W:" << winsP1_ << " D:" << draws_ << " L:" << winsP2_
        << " " << to_string(cause)
        << " " << info;
    if (!decision_) {
        // Only long a decision once
        Logger::testLogger().log(oss.str(), TraceLevel::result);
    }
	// Ignore changes of the decision after a decision has been made
    if (decision) decision_ = decision;
    PgnIO::tournament().saveGame(record);
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


std::pair<std::optional<bool>, std::string> SprtManager::computeSprt() const {
    if (winsP1_ < 0 || winsP2_ < 0 || draws_ < 0) {
        return { std::nullopt, "error" };
    }

    const double drawElo = computeDrawElo(winsP1_, draws_, winsP2_);

    const double x = std::pow(10.0, -drawElo / 400.0); 
	const double xSquare = (x + 1.0) * (x + 1.0); 
    const double scale = 4.0 * x / xSquare;

    const auto p0 = bayesEloProbabilities(config_.eloLower / scale, drawElo);
    const auto p1 = bayesEloProbabilities(config_.eloUpper / scale, drawElo);

    const double llr = computeLLR(winsP1_, draws_, winsP2_, p0, p1);
    const auto [lBound, uBound] = sprtBounds(config_.alpha, config_.beta);

    if (llr >= uBound) return {
        true,
        "H1 accepted, " + engineP1Name_ + " is at least " + std::to_string(config_.eloLower)
        + " elo stronger than " + engineP2Name_
    };
	if (llr <= lBound) return {
		false,
		"H0 accepted, " + engineP1Name_ + " is not stronger than " + engineP2Name_ 
        + " by at least " + std::to_string(config_.eloUpper) + " elo."
	};
    std::ostringstream oss;
    oss << "[ " << std::setprecision(2) << lBound << " < " << llr << " < " << uBound << " ]";
    return { 
        std::nullopt, oss.str()
    };
}

/*
void SprtManager::testAgainstConfidenceMethod() {
    const int n = winsP1_ + winsP2_ + draws_;
    const double score = (winsP1_ + 0.5 * draws_) / n;
    const double z = 1.96; // z.B. 95%-Konfidenz – später kalibrierbar
    const double stddev = std::sqrt(score * (1 - score) / n);
    const double lower = score - z * stddev;
    const double upper = score + z * stddev;

    // Umrechnen von Score in Elo-Grenze
    const double eloLower = 400 * std::log10(lower / (1 - lower));
    const double eloUpper = 400 * std::log10(upper / (1 - upper));

    std::cout << "[CI-based] Score: " << score
        << ", EloCI: [" << eloLower << ", " << eloUpper << "]\n";
}
*/

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
    engineP1Name_ = "P1";
	engineP2Name_ = "P2";

    for (int elo : eloDiffs) {
        int64_t numH1 = 0;
        int64_t numH0 = 0;
		int64_t noDecisions = 0;
        int64_t totalGames = 0;

        for (int sim = 0; sim < simulationsPerElo; ++sim) {
            // Reset intern
            winsP1_ = 0;
            winsP2_ = 0;
            draws_ = 0;
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
                if (r < winProb) ++winsP1_;
                else if (r < winProb + drawRate) ++draws_;
                else ++winsP2_;
                auto [result, info] = computeSprt();
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

