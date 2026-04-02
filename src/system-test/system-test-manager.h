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

#include "../engine-handling/engine-config.h"
#include "../game-manager/game-task.h"
#include "../game-manager/game-manager-pool.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace QaplaTester {

struct SystemTestConfig {
    uint32_t maxCores = 1;
    uint32_t step = 1;
    uint32_t stepTimeSeconds = 30;
    bool test = false;
};

class SystemTestManager : public GameTaskProvider {
public:
    SystemTestManager() = default;

    /**
     * @brief Initializes test setup and stores the single engine configuration.
     * @param engine Engine configuration used for both colors.
     * @param config System test settings.
     */
    void initialize(const EngineConfig& engine, const SystemTestConfig& config);

    /**
     * @brief Schedules this provider in the game manager pool.
     * @param self Shared owner of this manager.
     * @param pool Pool used for scheduling and concurrency control.
     */
    void schedule(const std::shared_ptr<SystemTestManager>& self, GameManagerPool& pool);

    /**
     * @brief Creates the next game task using one fixed start position.
     * @return Next game task if test is still active.
     */
    std::optional<GameTask> nextTask() override;

    /**
     * @brief Collects NPS data from finished games and handles step transitions.
     * @param taskId Identifier of the finished task.
     * @param record Completed game record.
     */
    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    /**
     * @brief Returns true if all configured steps have completed.
     */
    [[nodiscard]] bool isFinished() const {
        return finished_.load();
    }

private:
    struct StepAggregate {
        struct PlyStats {
            uint64_t count = 0;
            double sumNps = 0.0;
            double sumNpsSquared = 0.0;
            uint32_t firstDepth = 0;
            bool depthMismatchLogged = false;
            std::string firstMove;
            bool moveMismatchLogged = false;
        };
        uint64_t games = 0;
        uint64_t samples = 0;
        uint64_t totalNodes = 0;
        std::vector<PlyStats> plies;
    };

    struct StepResult {
        uint32_t concurrency = 0;
        uint64_t games = 0;
        uint64_t samples = 0;
        uint64_t stepElapsedMs = 0;
        double totalNps = 0.0;
        double averageNps = 0.0;
        double averageStandardDeviation = 0.0;
    };

    [[nodiscard]] GameTask createTask(uint64_t taskNumber) const;
    void accumulatePlyStats(size_t ply, const MoveRecord& move);
    void updateStepIfRequired();
    void completeCurrentStepAndAdvance();
    [[nodiscard]] static StepResult buildStepResult(uint32_t concurrency, const StepAggregate& aggregate, uint64_t stepElapsedMs);
    static void logStepResult(const StepResult& result, double baselineStandardDeviation);
    void resetAggregate();
    [[nodiscard]] bool storeReplaySeedIfNeeded(const GameRecord& record);
    void startReplayPhaseAfterSeed();

    EngineConfig engine_;
    SystemTestConfig config_;
    std::shared_ptr<GameManagerPool::PoolController> poolController_;

    std::chrono::steady_clock::time_point stepStartTime_;
    std::chrono::steady_clock::time_point stepDeadline_;

    mutable std::mutex stateMutex_;
    StepAggregate stepAggregate_;
    std::optional<double> baselineStandardDeviation_;
    std::optional<GameRecord> replayRecord_;

    size_t seedPlyCount_ = 0;

    std::atomic<uint64_t> nextTaskId_ = 1;
    std::atomic<bool> finished_ = false;
    std::atomic<bool> replayReady_ = false;
    std::atomic<uint32_t> currentConcurrency_ = 1;
    std::atomic<uint32_t> initialConcurrency_ = 1;
};

} // namespace QaplaTester
