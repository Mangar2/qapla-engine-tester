/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#pragma once

#include "../spsa-optimizer.h"
#include "../pair-tournament.h"
#include "../game-result.h"
#include <vector>
#include <optional>

namespace QaplaTester::Test {

/**
 * @brief Helper class for building SPSA optimizers with pre-played games for testing.
 * 
 * Simplifies test setup by providing methods to "play" games with specific results
 * without actually running engines.
 */
class SPSABuilder {
public:
    SPSAOptimizer optimizer;

    /**
     * @brief Creates an SPSA optimizer with the given engine and configuration.
     * @param engine The engine to optimize
     * @param config SPSA configuration
     */
    SPSABuilder(const EngineConfig& engine, const SPSAConfig& config) {
        optimizer.createSPSA(engine, config);
    }

    /**
     * @brief Plays a single game in the specified perturbation's pair tournament.
     * 
     * @param perturbationIndex Index of the perturbation (0-based)
     * @param result Game result (WhiteWins, BlackWins, Draw)
     * @param cause Cause of game end (default: Checkmate)
     * @return true if game was played successfully, false if no more games available
     */
    bool playGame(size_t perturbationIndex, GameResult result, 
                  GameEndCause cause = GameEndCause::Checkmate) {
        auto pairOpt = optimizer.getPairTournament(perturbationIndex);
        if (!pairOpt.has_value()) {
            return false;
        }
        
        auto* pair = const_cast<PairTournament*>(*pairOpt);
        auto task = pair->nextTask();
        if (!task.has_value()) {
            return false;
        }
        
        auto record = task->gameRecord;
        record.setGameEnd(cause, result);
        pair->setGameRecord(task->taskId, record);
        return true;
    }

    /**
     * @brief Plays multiple games in sequence for a perturbation.
     * 
     * @param perturbationIndex Index of the perturbation
     * @param games Vector of (result, cause) pairs to play
     * @return Number of games successfully played
     */
    size_t playGames(size_t perturbationIndex, 
                     const std::vector<std::pair<GameResult, GameEndCause>>& games) {
        size_t played = 0;
        for (const auto& [result, cause] : games) {
            if (!playGame(perturbationIndex, result, cause)) {
                break;
            }
            ++played;
        }
        return played;
    }

    /**
     * @brief Plays multiple games with just results (using default cause).
     * 
     * @param perturbationIndex Index of the perturbation
     * @param results Vector of game results
     * @return Number of games successfully played
     */
    size_t playGames(size_t perturbationIndex, const std::vector<GameResult>& results) {
        size_t played = 0;
        for (const auto& result : results) {
            if (!playGame(perturbationIndex, result)) {
                break;
            }
            ++played;
        }
        return played;
    }

    /**
     * @brief Plays all available games for a perturbation with specified results.
     * 
     * Useful for completing entire matches. Results cycle if fewer results than games.
     * 
     * @param perturbationIndex Index of the perturbation
     * @param results Vector of results to cycle through
     * @return Number of games played
     */
    size_t playAllGames(size_t perturbationIndex, const std::vector<GameResult>& results) {
        size_t played = 0;
        size_t resultIdx = 0;
        
        while (playGame(perturbationIndex, results[resultIdx % results.size()])) {
            ++played;
            ++resultIdx;
        }
        
        return played;
    }

    /**
     * @brief Plays a realistic game sequence with varied results and causes.
     * 
     * Creates a pattern: win, draw, win (checkmate), loss (resign), 
     * win (time), draw (repetition), etc.
     * 
     * @param perturbationIndex Index of the perturbation
     * @param gameCount Number of games to play
     * @return Number of games actually played
     */
    size_t playRealisticSequence(size_t perturbationIndex, size_t gameCount) {
        std::vector<std::pair<GameResult, GameEndCause>> sequence = {
            {GameResult::WhiteWins, GameEndCause::Checkmate},
            {GameResult::Draw, GameEndCause::DrawByRepetition},
            {GameResult::WhiteWins, GameEndCause::Checkmate},
            {GameResult::BlackWins, GameEndCause::Resignation},
            {GameResult::WhiteWins, GameEndCause::Timeout},
            {GameResult::Draw, GameEndCause::DrawByFiftyMoveRule},
            {GameResult::BlackWins, GameEndCause::Checkmate},
            {GameResult::Draw, GameEndCause::DrawByAgreement},
        };
        
        size_t played = 0;
        for (size_t idx = 0; idx < gameCount && idx < sequence.size(); ++idx) {
            if (!playGame(perturbationIndex, sequence[idx].first, sequence[idx].second)) {
                break;
            }
            ++played;
        }
        
        return played;
    }

    /**
     * @brief Completes an entire perturbation with wins for the plus variant.
     * 
     * Plays all games in a perturbation with WhiteWins (engine+ winning),
     * simulating a positive parameter change.
     * 
     * @param perturbationIndex Index of the perturbation
     * @return Number of games played
     */
    size_t completePerturbationWithWins(size_t perturbationIndex) {
        return playAllGames(perturbationIndex, {GameResult::WhiteWins});
    }

    /**
     * @brief Completes an entire perturbation with losses for the plus variant.
     * 
     * Plays all games in a perturbation with BlackWins (engine- winning),
     * simulating a negative parameter change.
     * 
     * @param perturbationIndex Index of the perturbation
     * @return Number of games played
     */
    size_t completePerturbationWithLosses(size_t perturbationIndex) {
        return playAllGames(perturbationIndex, {GameResult::BlackWins});
    }

    /**
     * @brief Completes an entire perturbation with draws.
     * 
     * Plays all games in a perturbation with draws,
     * simulating no effect from parameter change.
     * 
     * @param perturbationIndex Index of the perturbation
     * @return Number of games played
     */
    size_t completePerturbationWithDraws(size_t perturbationIndex) {
        return playAllGames(perturbationIndex, {GameResult::Draw});
    }

    /**
     * @brief Gets the number of perturbations in this optimizer.
     */
    size_t perturbationCount() const {
        return optimizer.perturbationCount();
    }

    /**
     * @brief Gets the current parameter values.
     */
    std::vector<double> getCurrentParameters() const {
        return optimizer.getCurrentParameters();
    }

    /**
     * @brief Gets the number of completed iterations.
     */
    size_t getCompletedIterations() const {
        return optimizer.getCompletedIterations();
    }
};

} // namespace QaplaTester::Test
