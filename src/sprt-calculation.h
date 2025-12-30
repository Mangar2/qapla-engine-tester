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
 * @brief Result of a SPRT computation containing all values for display.
 */
struct SprtResult {
    std::optional<bool> decision;  // true if H1 accepted, false if H0 accepted, nullopt if inconclusive
    std::string info;              // Human-readable decision info
    double llr;                    // Log-Likelihood Ratio
    double lowerBound;             // Lower decision boundary
    double upperBound;             // Upper decision boundary
    double drawElo;                // Computed drawElo value
    int winsA;                     // Wins for engine A
    int draws;                     // Number of draws
    int winsB;                      // Wins for engine B
    std::string engineA;           // Name of engine A
    std::string engineB;           // Name of engine B
    int eloLower;                  // Lower elo bound from config
    int eloUpper;                  // Upper elo bound from config
    bool reachedMaxGames = false;  // true if max games limit was reached without decision

    /**
     * @brief Checks if the SPRT test has finished.
     * @return true if a decision was made or max games limit was reached.
     */
    bool isFinished() const {
        return decision.has_value() || reachedMaxGames;
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
     * @param winsA Wins for engine A.
     * @param draws Number of draws.
     * @param winsB Wins for engine B.
     * @param engineA Name of engine A.
     * @param engineB Name of engine B.
     * @param eloLower Lower bound of H0 hypothesis.
     * @param eloUpper Upper bound of H1 hypothesis.
     * @param alpha Type I error probability.
     * @param beta Type II error probability.
     * @param maxGames Maximum number of games before stopping.
     * @return SprtResult containing decision, LLR, bounds and all relevant values.
     */
    SprtResult compute(
        int winsA, int draws, int winsB,
        const std::string& engineA, const std::string& engineB,
        int eloLower, int eloUpper,
        double alpha, double beta,
        uint32_t maxGames);

    /**
     * @brief Computes a human-readable SPRT info string.
     * @param result The SPRT result to format.
     * @return A formatted string containing the SPRT decision or bounds.
     */
    std::string formatInfo(const SprtResult& result);

} // namespace ClassicalSprt

/**
 * @brief Pentanomial SPRT implementation for paired games.
 * 
 * This is an experimental implementation for pentanomial SPRT analysis.
 * It is currently NOT used in the active tournament system.
 * 
 * The pentanomial approach analyzes game pairs (A vs B, B vs A) and categorizes
 * outcomes into 5 categories: (2,0), (1.5,0.5), (1,1), (0.5,1.5), (0,2).
 */
namespace PentaSprt {

    /**
     * @brief Computes pentanomial outcome probabilities for a given Elo difference and draw rate.
     *
     * @param elo Elo difference between players.
     * @param drawRate Expected draw rate (between 0 and 1).
     * @return Array of 5 probabilities for outcomes: [WW, WD, WL+DD, DL, LL].
     */
    std::array<double, 5> computeProbabilities(double elo, double drawRate);

    /**
     * @brief Computes the pentanomial Log-Likelihood Ratio.
     *
     * @param results Array of observed pentanomial outcome counts.
     * @param p0 Probabilities under H0 hypothesis.
     * @param p1 Probabilities under H1 hypothesis.
     * @return Computed LLR value.
     */
    double computeLLR(
        const std::array<int64_t, 5>& results,
        const std::array<double, 5>& p0,
        const std::array<double, 5>& p1);

    /**
     * @brief Pentanomial SPRT state tracker.
     * 
     * This class maintains the state of a pentanomial SPRT test and updates
     * the decision based on recorded game pair outcomes.
     */
    class Analyzer {
    public:
        /**
         * @brief Constructs a pentanomial SPRT analyzer.
         * @param alpha Maximum type I error.
         * @param beta Maximum type II error.
         * @param elo0 Elo difference under H0.
         * @param elo1 Elo difference under H1.
         * @param drawRate Expected draw rate.
         */
        Analyzer(double alpha, double beta, double elo0, double elo1, double drawRate);

        /**
         * @brief Records the outcome of a game pair.
         * @param resultIndex Index in [0,4] representing the pentanomial outcome.
         */
        void record(int resultIndex);

        /**
         * @brief Returns the current decision status.
         * @return std::optional<bool> with true for H1, false for H0, std::nullopt if undecided.
         */
        [[nodiscard]] std::optional<bool> status() const {
            return status_;
        }

        /**
         * @brief Returns the current log-likelihood ratio.
         */
        [[nodiscard]] double llr() const {
            return llr_;
        }

        /**
         * @brief Returns the internal pentanomial result counters.
         */
        [[nodiscard]] const std::array<int64_t, 5>& results() const {
            return results_;
        }

        /**
         * @brief Returns the total number of games represented by all recorded pairs.
         */
        [[nodiscard]] int gameCount() const;

    private:
        std::array<int64_t, 5> results_{};
        double elo0_;
        double elo1_;
        double drawRate_;
        double la_;
        double lb_;
        double llr_ = 0.0;
        double minLlr_ = 0.0;
        double maxLlr_ = 0.0;
        double sq0_ = 0.0;
        double sq1_ = 0.0;
        double o0_ = 0.0;
        double o1_ = 0.0;
        std::optional<bool> status_;
    };

} // namespace PentaSprt

} // namespace QaplaTester
