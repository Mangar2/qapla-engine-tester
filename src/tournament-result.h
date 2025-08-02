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

#include <vector>
#include <optional>
#include <string>
#include <istream>
#include <ostream>
#include <iomanip>
#include <mutex>
#include <array>
#include <sstream>

#include "game-result.h"
#include "game-record.h"

/**
 * @brief Stores result breakdowns per game termination cause.
 */
struct CauseStats
{
    int win = 0;  ///< Number of wins by this cause (from engineA's perspective)
    int loss = 0; ///< Number of losses by this cause (from engineA's perspective)
    int draw = 0; ///< Number of draws by this cause
};

/**
 * @brief Aggregates duel results between two engines, tracking win/draw/loss counts and termination causes.
 */
struct EngineDuelResult
{
private:
    std::string engineA; ///< First engine name
    std::string engineB; ///< Second engine name
public:
    EngineDuelResult(const std::string &a, const std::string &b)
        : engineA(a), engineB(b), causeStats{}
    {
    }
    EngineDuelResult() = default;

    int winsEngineA = 0;                                                         ///< Wins by engineA
    int winsEngineB = 0;                                                         ///< Wins by engineB
    int draws = 0;                                                               ///< Draw count
    std::array<CauseStats, static_cast<size_t>(GameEndCause::Count)> causeStats; ///< Stats per end cause

    const std::string &getEngineA() const
    {
        return engineA;
    }
    const std::string &getEngineB() const
    {
        return engineB;
    }
    /**
     * @brief Resets all counters to zero.
     */
    inline void clear()
    {
        winsEngineA = 0;
        winsEngineB = 0;
        draws = 0;
        for (auto &cs : causeStats)
            cs = {};
    }

    int total() const
    {
        return winsEngineA + winsEngineB + draws;
    }

    /**
     * @brief Computes the normalized score for engineA.
     * @return Score between 0.0 and 1.0
     */
    inline double engineARate() const
    {
        double totalGames = winsEngineA + winsEngineB + draws;
        return totalGames > 0 ? (winsEngineA * 1.0 + draws * 0.5) / totalGames : 0.0;
    }

    /**
     * @brief Computes the normalized score for engineB.
     * @return Score between 0.0 and 1.0
     */
    inline double engineBRate() const
    {
        double totalGames = winsEngineA + winsEngineB + draws;
        return totalGames > 0 ? (winsEngineB * 1.0 + draws * 0.5) / totalGames : 0.0;
    }

    /**
     * @brief Adds a game result to the duel statistics.
     * @param record Game record to evaluate
     */
    void addResult(const GameRecord &record);

    /**
     * @brief Returns a version of this result with sides switched.
     * @return Reversed EngineDuelResult
     */
    EngineDuelResult switchedSides() const;

    /**
     * @brief Produces a string summary including player names and score.
     * @return Human-readable result string
     */
    inline std::string toString() const
    {
        std::ostringstream oss;
        oss << engineA << " vs " << engineB
            << toResultString();
        return oss.str();
    }

    /**
     * @brief Returns a compact string with only the score portion.
     * @return Result string with W/D/L only
     */
    inline std::string toResultString() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << " ( " << engineARate() << " ) "
            << " W:" << winsEngineA << " D:" << draws << " L:" << winsEngineB;
        return oss.str();
    }

    /**
     * @brief Adds the stats from another duel result.
     * @param other The result to accumulate
     * @return Reference to this object
     */
    EngineDuelResult &operator+=(const EngineDuelResult &other);

    static constexpr std::string_view ANY_ENGINE = "";
};

/**
 * @brief Holds all duel results of one engine and computes an aggregate over them.
 */
struct EngineResult
{
    std::vector<EngineDuelResult> duels;
    std::string engineName;

    /**
     * @brief Returns a single aggregated result across all duels.
     *        engineA is set, engineB is empty.
     * @param targetEngine The name of the engine to aggregate results for.
     */
    EngineDuelResult aggregate(const std::string &targetEngine) const;

    void printResults(std::ostream &os) const;
    void printOutcome(std::ostream &os) const;
};

/**
 * @brief Collects duel results between engines and provides aggregated statistics per engine.
 *        Used to analyze tournament-level performance data.
 */
class TournamentResult
{
public:
    struct Scored
    {
        std::string engineName; ///< The name of the engine
        EngineResult result;    ///< The duel result
        double score;           ///< Normalized score (0.0 to 1.0)
        double elo;             ///< Computed Elo rating
        double total;           ///< Total number of games played
        int error;              ///< Error margin for the Elo rating
    };
    /**
     * @brief Adds a single EngineDuelResult to the internal collection.
     *        Can include matches between any engine pair.
     * @param result A duel result to include.
     */
    void add(const EngineDuelResult &result);

    /**
     * @brief Returns the names of all engines for which results have been recorded.
     * @return A vector of unique engine names.
     */
    std::vector<std::string> engineNames() const;

    /**
     * @brief Computes and returns all duel results for the given engine.
     * @param name The engine name.
     * @return An EngineResult object with individual duels and aggregate data, or std::nullopt if unknown.
     */
    std::optional<EngineResult> forEngine(const std::string &name) const;

    void printSummary(std::ostream &os) const;

    /**
     * @brief Prints the outcome (result causes) of the tournament, including all engines and their results.
     *
     */
    void printOutcome(std::ostream &os) const;

    /**
     * @brief Prints the current rating table in UCI-style key-value format.
     *
     * Format: rank <n> name <engine> elo <elo> error <error> games <n> score <pct> draw <pct>
     *
     * @param os Output stream to write to
     * @param averageElo Average Elo level for scaling ratings (e.g. 2600)
     */
    void printRatingTableUciStyle(std::ostream &os, int averageElo) const;

    /**
     * @brief Computes iterative Elo ratings and error estimates for all engines.
     *
     * Uses opponent-weighted averaging and repeated refinement of Elo values.
     * All engines start from baseElo. The result includes normalized score and error.
     *
     * @param baseElo Starting Elo value (e.g. 2600)
     * @param passes Number of update iterations (e.g. 10)
     * @return Vector of scored engines sorted by descending Elo
     */
    std::vector<Scored> computeAllElos(int baseElo = 2600, int passes = 50) const;

private:
    /**
     * @brief Initializes scored engine data with aggregated results and normalized score.
     *
     * Elo and error remain unset (0) and must be computed separately.
     *
     * @return Vector of Scored entries for all engines with valid game data
     */
    std::vector<Scored> initializeScoredEngines() const;

    /**
     * @brief Computes the average opponent Elo weighted by number of games.
     *
     * @param s The scored engine to evaluate
     * @param currentElo All scored engines with current Elo values
     * @return Weighted average Elo of all opponents
     */
    double averageOpponentElo(const Scored &s, const std::unordered_map<std::string, double>& currentElo) const; 

    std::vector<EngineDuelResult> results_;
};
