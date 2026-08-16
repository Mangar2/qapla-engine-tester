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
#include "fastchess-sprt.h"
#include "../base-elements/elo-helper.h"

#include <cmath>
#include <format>
#include <sstream>
#include <iomanip>

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

/**
 * @brief Widest gap between the hypotheses that SPRT is meant for.
 *
 * Every difference between H0 and H1 leads to the same decision. Within a few Elo that is a
 * detail; over a wide range the decision carries almost no information about the size of the
 * difference, and the result says so.
 */
constexpr float narrowHypothesisGapElo = 10.0F;

/**
 * @brief Formats an Elo value without the trailing zeros of the underlying float.
 */
std::string formatElo(float elo) {
    return std::format("{:g}", elo);
}

std::string formatInfo(const SprtResult& result) {
    if (result.decision.has_value()) {
        // The decision states which hypothesis was accepted, nothing more: it neither proves the
        // accepted value nor measures the difference. The size is reported separately, and
        // deliberately without an error margin - the test stops when the numbers look extreme, so
        // an interval taken from exactly those games would read more precise than it is.
        const double computedElo = computeElo(result.winsA, result.winsB, result.draws);
        auto info = std::format(
            "{} accepted (H0: {} elo, H1: {} elo). Computed elo difference ({} - {}): {:+.0f} elo "
            "from {} games",
            *result.decision ? "H1" : "H0",
            formatElo(result.eloH0), formatElo(result.eloH1),
            result.engineA, result.engineB, computedElo,
            result.winsA + result.draws + result.winsB);

        if ((result.eloH1 - result.eloH0) > narrowHypothesisGapElo) {
            info += std::format(
                ". Note: H0 and H1 are {} elo apart. SPRT is meant for small differences; over such "
                "a range the decision says little about the size of the difference",
                formatElo(result.eloH1 - result.eloH0));
        }
        return info;
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


} // namespace SprtBase

// ============================================================================
// Classical Trinomial SPRT (Derived from cutechess)
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
    float eloH0, float eloH1,
    double alpha, double beta,
    uint32_t maxGames) {

	const double drawElo = computeDrawElo(winsA, draws, winsB);

    const double x = std::pow(10.0, -drawElo / 400.0); 
	const double xSquare = (x + 1.0) * (x + 1.0); 
    const double scale = 4.0 * x / xSquare;

    const auto p0 = computeProbabilities(static_cast<double>(eloH0) / scale, drawElo);
    const auto p1 = computeProbabilities(static_cast<double>(eloH1) / scale, drawElo);

    const double llr = computeLLR(winsA, draws, winsB, p0, p1);
    const auto [lBound, uBound] = SprtBase::computeBounds(alpha, beta);
    int totalGames = winsA + draws + winsB;

    std::optional<bool> decision;
    if (llr >= uBound) {
        decision = true; // Accept H1
    } else if (llr <= lBound) {
        decision = false; // Accept H0
    } else {
        decision = std::nullopt; // No decision yet
    }

    SprtResult result {
        .llr = llr,
        .lowerBound = lBound,
        .upperBound = uBound,
        .winsA = winsA,
        .draws = draws,
        .winsB = winsB,
        .engineA = engineA,
        .engineB = engineB,
        .eloH0 = eloH0,
        .eloH1 = eloH1,
        .decision = decision,
        .reachedMaxGames = std::cmp_greater_equal(totalGames, maxGames),
        .model = "bayesian",
        .pentanomial = false
    };

    result.info = SprtBase::formatInfo(result);
    return result;
}


} // namespace ClassicalSprt

// ============================================================================
// Fastchess SPRT (Recommended)
// ============================================================================

namespace FastchessSprt {

SprtResult compute(const SprtParameters& params) {

    fastchess::SPRT sprt(params.alpha, params.beta, params.eloH0, params.eloH1, params.model, true);
    bool report_penta = params.pentanomial;
    fastchess::SPRT::isValid(params.alpha, params.beta, params.eloH0, params.eloH1, params.model, report_penta);
    
    fastchess::Stats stats {
        .wins = params.winsA,
        .draws = params.draws,
        .losses = params.winsB,
        .penta_WW = params.pentaWW,
        .penta_WD = params.pentaWD,
        .penta_WL = params.pentaWL,
        .penta_DD = params.pentaDD,
        .penta_LD = params.pentaLD,
        .penta_LL = params.pentaLL
    };
    
    double llr = sprt.getLLR(stats, report_penta);
    auto result = sprt.getResult(llr);
    int totalGames = params.winsA + params.draws + params.winsB; 

    std::optional<bool> decision;
    if (result == fastchess::SPRT_H1) {
        decision = true; // Accept H1
    } else if (result == fastchess::SPRT_H0) {
        decision = false; // Accept H0
    } else {
        decision = std::nullopt; // No decision yet
    }
    
    SprtResult sprtResult {
        .llr = llr,
        .lowerBound = sprt.getLowerBound(),
        .upperBound = sprt.getUpperBound(),
        .winsA = params.winsA,
        .draws = params.draws,
        .winsB = params.winsB,
        .engineA = params.engineA,
        .engineB = params.engineB,
        .eloH0 = params.eloH0,
        .eloH1 = params.eloH1,
        .decision = decision,
        .reachedMaxGames = result == fastchess::SPRT_CONTINUE && 
            (std::cmp_greater_equal(totalGames, params.maxGames)),
        .model = params.model,
        .pentanomial = params.pentanomial
    };
    
    sprtResult.info = SprtBase::formatInfo(sprtResult);
    
    return sprtResult;
}

} // namespace FastchessSprt

} // namespace QaplaTester
