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
#include <mutex>
#include <utility>

#include "time-control.h"
#include "game-record.h"
#include "game-result.h"
#include "engine-report.h"
#include "game-task.h"

namespace QaplaTester {

/**
 * @brief Manages a test tournament that plays multiple games with different time controls.
 * 
 * TestTournament provides a framework for running automated chess tournaments
 * with various time control configurations. It distributes games across different
 * time control pairs and tracks results, time management, and game outcomes.
 * Thread-safe for concurrent game execution.
 */
class TestTournament : public GameTaskProvider {
public:
    /**
     * @brief Constructs a new test tournament.
     * 
     * Initializes the tournament with predefined time control pairs:
     * - 20s+500ms vs 10s+100ms
     * - 10s+500ms vs 5s+100ms
     * - 4s+500ms vs 2s+100ms
     * - 20s+500ms vs 10s (no increment)
     * - 10s+200ms vs 5s (no increment)
     * - 6s+200ms vs 3s (no increment)
     * 
     * Games are evenly distributed across all time control pairs.
     * 
     * @param totalGames Total number of games to play in the tournament
     * @param checklist Pointer to the engine report for logging test results
     */
    explicit TestTournament(uint32_t totalGames, EngineReport* checklist);

    /**
     * @brief Provides the next available task for game execution.
     * 
     * Thread-safe method that generates a new game task with appropriate time controls.
     * Tasks are distributed evenly across all configured time control pairs.
     * 
     * @return A GameTask with configured time controls and start position,
     *         or std::nullopt if all games have been assigned
     */
    std::optional<GameTask> nextTask() override;

    /**
     * @brief Records the result of a completed game.
     * 
     * Stores the game record, validates time management, and logs tournament status.
     * Thread-safe method that can be called from multiple game execution threads.
     * 
     * @param taskId Identifier of the task (currently unused)
     * @param record Complete game record including result, moves, and time usage
     */
    void setGameRecord(
        [[maybe_unused]] const std::string& taskId,
        const GameRecord& record) override;

    /**
     * @brief Flag to enable/disable time limit validation in reports.
     * 
     * When true, the tournament checks that engines:
     * - Keep sufficient time reserve (don't use too much time)
     * - Never drop below 1 second remaining time
     */
    bool checkTimeLimits = false;

private:
    /**
     * @brief Validates time management for a completed game.
     * 
     * Checks that neither engine lost on time and validates that time usage
     * was reasonable for both white and black. Logs results to the checklist.
     * 
     * @param record Game record containing result and time usage data
     */
    void checkTimeManagement(const GameRecord& record);

    /**
     * @brief Calculates expected time usage ratio range based on move count.
     * 
     * Provides acceptable bounds for the ratio of (used time / available time)
     * at game end. Bounds are interpolated from predefined thresholds:
     * - 0-40 moves: 0%-20% to 20%-60%
     * - 40-80 moves: 20%-60% to 40%-90%
     * - 80-160 moves: 40%-90% to 65%-100%
     * - 160-320 moves: 65%-100% to 80%-100%
     * - 320+ moves: 80%-100%
     * 
     * @param moveCount Total number of moves in the game
     * @return Pair of (minimum ratio, maximum ratio), both in range [0.0, 1.0]
     */
    static std::pair<double, double> expectedUsageRatioRange(size_t moveCount);

    /**
     * @brief Validates that time usage is within reasonable bounds.
     * 
     * Checks that the engine's time consumption is appropriate for the game length
     * and time control. Adjusts expectations for increment-heavy time controls.
     * Only validates games with 30+ moves. Logs warnings if time usage is excessive
     * or remaining time is critically low.
     * 
     * @param usedTimeMs Total time used in milliseconds
     * @param tc Time control configuration
     * @param moveCount Number of moves played
     */
    void timeUsageReasonable(uint64_t usedTimeMs, const TimeControl& tc, size_t moveCount);

    /**
     * @brief Logs current tournament statistics.
     * 
     * Thread-safe method that outputs:
     * - Progress (games played / total games)
     * - Win/Draw/Loss distribution
     * - Last used time controls
     * - Game end cause distribution (checkmate, timeout, etc.)
     * 
     * Logged to test logger for monitoring tournament progress.
     */
    void logStatus();

    uint32_t maxGames_;                                              ///< Total number of games in the tournament
    uint32_t current_{};                                             ///< Number of games assigned so far
    std::mutex mutex_;                                               ///< Protects concurrent access to tournament state
    std::vector<GameRecord> gameRecords_;                            ///< Complete results of all finished games
    std::vector<std::pair<TimeSegment, TimeSegment>> timePairs_;     ///< Time control pairs for white and black
    std::vector<int> usageCount_;                                    ///< (Currently unused) Could track time control usage
    EngineReport* checklist_;                                        ///< Report handler for test validation results
};

} // namespace QaplaTester
