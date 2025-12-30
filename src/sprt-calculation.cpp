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

#include "sprt-calculation.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <compare>

namespace QaplaTester {

// ============================================================================
// Base SPRT Functions
// ============================================================================

namespace SprtBase {

std::tuple<double, double> computeBounds(double alpha, double beta) {
    const double lBound = std::log(beta / (1.0 - alpha));         
    const double uBound = std::log((1.0 - beta) / alpha);         
    return { lBound, uBound };
}

double logisticScore(double elo) {
    return 1.0 / (1.0 + std::pow(10.0, -elo / 400.0));
}

} // namespace SprtBase

// ============================================================================
// Classical Trinomial SPRT (Currently Active)
// ============================================================================

namespace ClassicalSprt {

double computeDrawElo(double wins, double draws, double losses) {
    wins += 0.5;
    draws += 0.5;
    losses += 0.5;

    auto count = wins + draws + losses;
    auto pWin = wins / count;
    auto pLoss = losses / count;
	return 200.0 * std::log10((1.0 - pLoss) / pLoss * (1.0 - pWin) / pWin);
}

std::tuple<double, double, double> computeProbabilities(double bayesElo, double drawElo) {
    const double pWin = 1.0 / (1.0 + std::pow(10.0, (drawElo - bayesElo) / 400.0));
    const double pLoss = 1.0 / (1.0 + std::pow(10.0, (drawElo + bayesElo) / 400.0));
    const double pDraw = 1.0 - pWin - pLoss;
    return { pWin, pDraw, pLoss };
}

double computeLLR(double wins, double draws, double losses,
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

SprtResult compute(
    int winsA, int draws, int winsB,
    const std::string& engineA, const std::string& engineB,
    int eloLower, int eloUpper,
    double alpha, double beta,
    uint32_t maxGames) {

	const double drawElo = computeDrawElo(winsA, draws, winsB);

    const double x = std::pow(10.0, -drawElo / 400.0); 
	const double xSquare = (x + 1.0) * (x + 1.0); 
    const double scale = 4.0 * x / xSquare;

    const auto p0 = computeProbabilities(eloLower / scale, drawElo);
    const auto p1 = computeProbabilities(eloUpper / scale, drawElo);

    const double llr = computeLLR(winsA, draws, winsB, p0, p1);
    const auto [lBound, uBound] = SprtBase::computeBounds(alpha, beta);

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
    result.eloLower = eloLower;
    result.eloUpper = eloUpper;

    if (llr >= uBound) { 
        result.decision = true;
    } else if (llr <= lBound) {
        result.decision = false;
	} else {
        result.decision = std::nullopt;
    }

    // Check if max games limit was reached without a decision
    int totalGames = winsA + draws + winsB;
    result.reachedMaxGames = !result.decision.has_value() && 
        (std::cmp_greater_equal(totalGames, maxGames));

    result.info = formatInfo(result);
    
    return result;
}

std::string formatInfo(const SprtResult& result) {
    if (result.decision.has_value()) {
        if (*result.decision) {
            return "H1 accepted, " + result.engineA + " is at least " + std::to_string(result.eloUpper)
                + " elo stronger than " + result.engineB;
        }
        return "H0 accepted, " + result.engineA + " is not stronger than " + result.engineB
            + " by at least " + std::to_string(result.eloLower) + " elo.";
    }
    
    // Check if max games limit reached without decision
    if (result.reachedMaxGames) {
        int totalGames = result.winsA + result.draws + result.winsB;
        std::ostringstream oss;
        oss << "No decision after " << totalGames << " games. "
            << "LLR=" << std::fixed << std::setprecision(2) << result.llr
            << " (bounds: [" << result.lowerBound << ", " << result.upperBound << "]). "
            << "Score: " << result.winsA << "/" << result.draws << "/" << result.winsB
            << " (" << result.engineA << " vs " << result.engineB << ").";
        return oss.str();
    }
    
    std::ostringstream oss;
    oss << "[ " << std::fixed << std::setprecision(2) << result.lowerBound << " < " 
        << std::setw(5) << result.llr << " < " << result.upperBound << " ]";
    return oss.str();
}

} // namespace ClassicalSprt

// ============================================================================
// Pentanomial SPRT (Experimental - Not Currently Used)
// ============================================================================

namespace PentaSprt {

std::array<double, 5> computeProbabilities(double elo, double drawRate) {
    const double expectedScore = SprtBase::logisticScore(elo);
    const double pDraw = drawRate;
    const double decisiveFactor = 1.0 - pDraw;
    const double pWin = decisiveFactor * expectedScore;
    const double pLoss = decisiveFactor * (1.0 - expectedScore);

    std::array<double, 5> p{};
    p[0] = pWin * pWin;
    p[4] = pLoss * pLoss;
    p[1] = 2.0 * pWin * pDraw;
    p[3] = 2.0 * pLoss * pDraw;
    p[2] = 2.0 * pWin * pLoss + pDraw * pDraw;
    return p;
}

double computeLLR(
    const std::array<int64_t, 5>& results,
    const std::array<double, 5>& p0,
    const std::array<double, 5>& p1) {
    double llr = 0.0;
    for (std::size_t i = 0; i < results.size(); ++i) {
        const double count = static_cast<double>(results[i]) + 0.5;
        llr += count * std::log(p1[i] / p0[i]);
    }
    return llr;
}

Analyzer::Analyzer(double alpha, double beta, double elo0, double elo1, double drawRate)
    : elo0_(elo0)
    , elo1_(elo1)
    , drawRate_(drawRate)
    , la_(std::log(beta / (1.0 - alpha)))
    , lb_(std::log((1.0 - beta) / alpha)) {
}

int Analyzer::gameCount() const {
    int64_t sum = 0;
    for (auto value : results_) {
        sum += value;
    }
    return static_cast<int>(2 * sum);
}

void Analyzer::record(int resultIndex) {
    if (status_.has_value()) {
        return;
    }

    if (resultIndex < 0 || std::cmp_greater_equal(resultIndex, static_cast<int>(results_.size()))) {
        return;
    }

    results_[static_cast<std::size_t>(resultIndex)] += 1;

    const auto p0 = computeProbabilities(elo0_, drawRate_);
    const auto p1 = computeProbabilities(elo1_, drawRate_);
    llr_ = computeLLR(results_, p0, p1);

    if (llr_ > maxLlr_) {
        const double diff = llr_ - maxLlr_;
        sq1_ += diff * diff;
        maxLlr_ = llr_;
        o1_ = sq1_ / llr_ / 2.0;
    }

    if (llr_ < minLlr_) {
        const double diff = llr_ - minLlr_;
        sq0_ += diff * diff;
        minLlr_ = llr_;
        o0_ = -sq0_ / llr_ / 2.0;
    }

    if (llr_ > lb_ - o1_) {
        status_ = true;
    } else if (llr_ < la_ + o0_) {
        status_ = false;
    }
}

} // namespace PentaSprt

} // namespace QaplaTester
