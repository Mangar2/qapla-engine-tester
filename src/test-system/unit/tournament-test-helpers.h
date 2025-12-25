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

#include "tournament.h"
#include "pair-tournament.h"
#include "game-result.h"
#include <vector>
#include <optional>

namespace QaplaTester::Test {

/**
 * @brief Helper class for building tournaments with pre-played games for testing.
 * 
 * Simplifies test setup by providing methods to "play" games with specific results
 * without actually running engines.
 */
class TournamentBuilder {
public:
    Tournament tournament;

    /**
     * @brief Creates a tournament with the given engines and configuration.
     * @param engines List of engines to participate
     * @param config Tournament configuration
     */
    TournamentBuilder(const std::vector<EngineConfig>& engines, const TournamentConfig& config) {
        tournament.createTournament(engines, config);
        tournament.setVerbose(false);
    }

    /**
     * @brief Plays a single game in the specified pair tournament.
     * 
     * @param pairTournamentIndex Index of the pair tournament (0-based)
     * @param result Game result (WhiteWins, BlackWins, Draw)
     * @param cause Cause of game end (default: Checkmate)
     * @return true if game was played successfully, false if no more games available
     */
    bool playGame(size_t pairTournamentIndex, GameResult result, 
                  GameEndCause cause = GameEndCause::Checkmate) {
        auto pairOpt = tournament.getPairTournament(pairTournamentIndex);
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
     * @brief Plays multiple games in sequence for a pair tournament.
     * 
     * @param pairTournamentIndex Index of the pair tournament
     * @param games Vector of (result, cause) pairs to play
     * @return Number of games successfully played
     */
    size_t playGames(size_t pairTournamentIndex, 
                     const std::vector<std::pair<GameResult, GameEndCause>>& games) {
        size_t played = 0;
        for (const auto& [result, cause] : games) {
            if (!playGame(pairTournamentIndex, result, cause)) {
                break;
            }
            ++played;
        }
        return played;
    }

    /**
     * @brief Plays multiple games with just results (using default cause).
     * 
     * @param pairIndex Index of the pair tournament
     * @param results Vector of game results
     * @return Number of games successfully played
     */
    size_t playGames(size_t pairTournamentIndex, const std::vector<GameResult>& results) {
        size_t played = 0;
        for (const auto& result : results) {
            if (!playGame(pairTournamentIndex, result)) {
                break;
            }
            ++played;
        }
        return played;
    }

    /**
     * @brief Plays all available games for a pair tournament with specified results.
     * 
     * Useful for completing entire matches. Results cycle if fewer results than games.
     * 
     * @param pairIndex Index of the pair tournament
     * @param results Vector of results to cycle through
     * @return Number of games played
     */
    size_t playAllGames(size_t pairTournamentIndex, const std::vector<GameResult>& results) {
        size_t played = 0;
        size_t resultIdx = 0;
        
        while (playGame(pairTournamentIndex, results[resultIdx % results.size()])) {
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
     * @param pairIndex Index of the pair tournament
     * @param gameCount Number of games to play
     * @return Number of games actually played
     */
    size_t playRealisticSequence(size_t pairIndex, size_t gameCount) {
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
            if (!playGame(pairIndex, sequence[idx].first, sequence[idx].second)) {
                break;
            }
            ++played;
        }
        
        return played;
    }

    /**
     * @brief Gets the number of pair tournaments in this tournament.
     */
    size_t pairTournamentCount() const {
        return tournament.pairTournamentCount();
    }

    /**
     * @brief Gets the result of the tournament.
     */
    TournamentResult getResult() const {
        return tournament.getResult();
    }
};

} // namespace QaplaTester::Test
