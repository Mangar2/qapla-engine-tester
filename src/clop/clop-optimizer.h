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

#include "clop-model.h"
#include "clop-types.h"

#include "../game-manager/pair-tournament.h"
#include "../engine-handling/engine-config.h"
#include "../base-elements/table-format.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace QaplaTester {

class GameManagerPool;

/**
 * @brief One completed CLOP sample and its measured outcome.
 */
struct CLOPSample {
    std::vector<double> values;
    std::vector<double> normalizedValues;
    double outcome = 0.5;
    double observationWeight = 1.0;
    double designWeight = 1.0;
    size_t index = 0;
    uint64_t generation = 0;
    std::shared_ptr<PairTournament> pairing;
};

/**
 * @brief Quadratic CLOP optimizer for noisy black-box engine tuning.
 */
class CLOPOptimizer {
public:
    CLOPOptimizer() = default;
    ~CLOPOptimizer();

    /**
     * @brief Initializes CLOP with base engine and optimization config.
     * @param engine Engine configuration to tune.
     * @param config CLOP settings.
     */
    void createCLOP(const EngineConfig& engine, const CLOPConfig& config);

    /**
     * @brief Starts CLOP scheduling.
     * @param concurrency Number of concurrent games.
     * @param pool Shared game manager pool.
     */
    void scheduleCLOP(uint32_t concurrency, GameManagerPool& pool);

    /**
     * @brief Waits until all configured samples are completed.
     */
    void waitUntilFinished();

    /**
     * @brief Requests worker stop.
     */
    void stop();

    /**
     * @brief Returns current estimated best parameters.
     */
    [[nodiscard]] std::vector<double> getEstimatedParameters() const;

    /**
     * @brief Returns number of completed samples.
     */
    [[nodiscard]] size_t getCompletedSamples() const;

    /**
     * @brief Returns status table.
     */
    [[nodiscard]] TableData getStatusTable() const;

private:
    struct RecomputeSnapshot {
        std::vector<CLOPSample> modelSnapshot;
        std::vector<CLOPSample> pendingBatch;
        std::vector<double> previousEstimatedParameters;
        size_t completedSamples = 0;
        size_t activeSamples = 0;
        size_t pendingSamples = 0;
        size_t signalEvidenceSample = 0;
        uint64_t nextModelGeneration = 0;
        bool reachedSampleLimit = false;
        bool signalEvidenceAvailable = false;
        bool shouldRecomputeSignal = false;
        CLOPSignalEvidence signalEvidence{};
        std::vector<double> nextEstimatedParameters;
        TableData diagnosticsTable;
        TableData indicatorTable;
        TableData signalTable;
    };

    void schedulerThreadFunction();
    void recomputeThreadFunction();
    [[nodiscard]] bool collectRecomputeSnapshot(RecomputeSnapshot& snapshot);
    void computeRecomputeSnapshot(RecomputeSnapshot& snapshot);
    void commitRecomputeSnapshot(
        const RecomputeSnapshot& snapshot,
        bool& shouldLog,
        bool& isFinishedNow);
    void updateFinishedStateLocked();
    void scheduleNextSample();
    void onPairFinished(PairTournament* sender);

    [[nodiscard]] std::vector<double> createNormalizedSample(
        const std::vector<CLOPSample>& modeledSamples,
        const std::vector<double>& estimatedParameters,
        size_t scheduledCount);
    [[nodiscard]] EngineConfig createConfiguredEngine(const std::vector<double>& values) const;

    EngineConfig baseEngine_;
    EngineConfig baselineEngine_;
    CLOPConfig config_;
    std::unique_ptr<CLOPModel> model_;
    std::shared_ptr<StartPositions> startPositions_;
    GameManagerPool* pool_ = nullptr;

    std::vector<CLOPSample> samples_;
    std::vector<CLOPSample> pendingResults_;
    std::vector<double> estimatedParameters_;
    size_t completedSamples_ = 0;
    size_t lastLoggedCompletedSamples_ = 0;
    size_t lastSignalEvidenceSample_ = 0;
    uint64_t modelGeneration_ = 0;
    size_t nextSampleIndex_ = 0;
    CLOPSignalEvidence lastSignalEvidence_{};

    std::vector<CLOPSample> activeSamples_;
    uint32_t nextRound_ = 0;
    std::chrono::steady_clock::time_point lastRecomputeAt_{};
    std::chrono::milliseconds minRecomputeInterval_{ 250 };

    mutable std::mutex stateMutex_;
    std::condition_variable stateCondition_;
    std::condition_variable recomputeCondition_;

    std::thread schedulerThread_;
    std::thread recomputeThread_;
    std::atomic<bool> stopWorker_ = false;
    bool recomputeRunning_ = false;
    bool initialized_ = false;
    bool scheduled_ = false;
    bool finished_ = false;

    std::mt19937 rng_;
};

} // namespace QaplaTester
