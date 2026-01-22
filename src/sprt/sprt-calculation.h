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

#pragma once

#include <optional>
#include <string>
#include <array>
#include <cstdint>
#include <tuple>



namespace QaplaTester {

/**
 * @brief Parameters for SPRT computation.
 */
struct SprtParameters {
    int winsA = 0;          ///> Wins for engine A
    int draws = 0;          ///> Number of draws
    int winsB = 0;          ///> Wins for engine B
    std::string engineA;    ///> Name of engine A
    std::string engineB;    ///> Name of engine B
    float eloLower = 0.0F;  ///> Lower bound of H0 hypothesis
    float eloUpper = 3.0F;  ///> Upper bound of H1 hypothesis
    double alpha = 0.05;    ///> Type I error probability
    double beta = 0.05;     ///> Type II error probability
    uint32_t maxGames = 200000; ///> Maximum number of games before stopping
    std::string model = "bayesian"; // "bayesian", "logistic", "normalized"
    bool pentanomial = false;  ///> Use pentanomial statistics
    
    // Pentanomial statistics (from engineA's perspective)
    int pentaWW = 0;  ///> Both games won by engineA
    int pentaWD = 0;  ///> One win, one draw for engineA
    int pentaWL = 0;  ///> One win for engineA, one loss
    int pentaDD = 0;  ///> Both games drawn
    int pentaLD = 0;  ///> One loss, one draw for engineA
    int pentaLL = 0;  ///> Both games lost by engineA
};

/**
 * @brief Result of a SPRT computation containing all values for display.
 */
struct SprtResult {
    std::string info{};            // Human-readable decision info
    double llr;                    // Log-Likelihood Ratio
    double lowerBound;             // Lower decision boundary
    double upperBound;             // Upper decision boundary
    int winsA;                     // Wins for engine A
    int draws;                     // Number of draws
    int winsB;                      // Wins for engine B
    std::string engineA;           // Name of engine A
    std::string engineB;           // Name of engine B
    float eloLower;                // Lower elo bound from config
    float eloUpper;                // Upper elo bound from config
    std::optional<bool> decision;  // true if H1 accepted, false if H0 accepted, nullopt if inconclusive
    bool reachedMaxGames = false;  // true if max games limit was reached without decision
    std::string model;           // Model used: "normalized", "logistic", "bayesian"
    bool pentanomial = false;      // true if pentanomial statistics used, false for trinomial

    /**
     * @brief Checks if the SPRT test has finished.
     * @return true if a decision was made or max games limit was reached.
     */
    [[nodiscard]] bool isFinished() const {
        return decision || reachedMaxGames;
    }

    /**
     * @brief Checks if the SPRT test is currently running.
     * @return true if the test is ongoing.
     */
    [[nodiscard]] bool isRunning() const {
        return !isFinished() && (winsA + winsB + draws) > 0;
    }
};

/**
 * @brief Base calculation functions for SPRT analysis.
 * 
 * This namespace provides fundamental mathematical operations used by both
 * classical trinomial SPRT and pentanomial SPRT implementations.
 */
namespace SprtBase {

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
    std::tuple<double, double> computeBounds(double alpha, double beta);

    /**
     * @brief Computes logistic score from Elo difference.
     * @param elo Elo difference.
     * @return Expected score between 0 and 1.
     */
    double logisticScore(double elo);

    
    /**
     * @brief Computes a human-readable SPRT info string.
     * @param result The SPRT result to format.
     * @return A formatted string containing the SPRT decision or bounds.
     */
    std::string formatInfo(const SprtResult& result);

} // namespace SprtBase

/**
 * @brief Classical trinomial SPRT implementation using BayesElo model.
 * 
 * This implementation is currently active and used for tournament analysis.
 * It models game outcomes as win/draw/loss with BayesElo probability distribution.
 */
namespace ClassicalSprt {

    /**
     * @brief Estimates the drawElo parameter based on regularized outcome counts.
     *
     * @param wins number of wins.
     * @param losses number of losses.
     * @param draws number of draws.
     * @return double Estimated drawElo value.
     */
    double computeDrawElo(double wins, double draws, double losses);

    /**
     * @brief Computes outcome probabilities based on BayesElo parameters.
     *
     * @param bayesElo Elo difference under the hypothesis.
     * @param drawElo Estimated drawElo value.
     * @return std::tuple<double, double, double> Probabilities for win, draw and loss.
     */
    std::tuple<double, double, double> computeProbabilities(double bayesElo, double drawElo);

    /**
     * @brief Computes the Log-Likelihood Ratio (LLR) based on observed and expected probabilities.
     *
     * @param wins Regularized number of wins.
     * @param draws Regularized number of draws.
     * @param losses Regularized number of losses.
     * @param p0 Tuple of probabilities under H0 (win, draw, loss).
     * @param p1 Tuple of probabilities under H1 (win, draw, loss).
     * @return double Computed LLR value.
     */
    double computeLLR(double wins, double draws, double losses,
        const std::tuple<double, double, double>& p0,
        const std::tuple<double, double, double>& p1);

    /**
     * @brief Computes the complete SPRT result for classical trinomial SPRT.
     *
     * @param params Parameters for the SPRT computation.
     * @return SprtResult containing decision, LLR, bounds and all relevant values.
     */
    SprtResult compute(SprtParameters params);


} // namespace ClassicalSprt

/**
 * @brief Fastchess SPRT implementation (recommended).
 * 
 * This uses the fastchess implementation with proper Maximum Likelihood Estimation.
 * Supports multiple models: bayesian, logistic, normalized.
 */
namespace FastchessSprt {

    /**
     * @brief Computes the complete SPRT result using fastchess algorithm.
     *
     * @param params Parameters for the SPRT computation.
     * @return SprtResult containing decision, LLR, bounds and all relevant values.
     */
    SprtResult compute(SprtParameters params);

} // namespace FastchessSprt

} // namespace QaplaTester
