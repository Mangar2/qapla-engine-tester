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

#include "../game-manager/pair-tournament.h"
#include "../engine-handling/engine-config.h"
#include "../base-elements/table-format.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace QaplaTester {

class GameManagerPool;

/**
 * @brief Configuration for a single parameter optimized by CLOP.
 */
struct CLOPParameterConfig {
    std::string name;
    double defaultValue;
    double minValue;
    double maxValue;
};

using CLOPParameterList = std::vector<CLOPParameterConfig>;

/**
 * @brief Configuration for CLOP optimization.
 */
struct CLOPConfig {
    CLOPParameterList parameters;
    uint32_t maxActivePairs = 32;
    uint32_t samples = 100;
    uint32_t gamesPerSample = 8;
    uint32_t warmupSamples = 8;
    uint32_t outcomeInterval = 10;
    uint32_t maxWeightIterations = 25;
    uint32_t openingsSeed = 0;
    double h = 3.0;
    double priorVariance = 100.0;
    std::string openingsFile;
};

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
    struct LogisticModel {
        std::vector<double> coefficients;
    };

    void workerThreadFunction();
    void scheduleNextSample();
    void onPairFinished(PairTournament* sender);

    [[nodiscard]] std::vector<double> createNormalizedSample();
    [[nodiscard]] std::vector<double> denormalizeValues(const std::vector<double>& normalizedValues) const;
    [[nodiscard]] std::vector<double> normalizeValues(const std::vector<double>& values) const;
    [[nodiscard]] EngineConfig createConfiguredEngine(const std::vector<double>& values) const;

    [[nodiscard]] LogisticModel fitQuadraticLogisticRegression() const;
    [[nodiscard]] double fitLogisticMean() const;
    [[nodiscard]] double confidenceDeviation(double meanLogit) const;
    void updateDesignWeights();
    void updateEstimatedOptimum();

    [[nodiscard]] size_t featureCount() const;
    [[nodiscard]] std::vector<double> buildFeatureVector(const std::vector<double>& normalizedValues) const;
    [[nodiscard]] double evaluateQuadratic(const LogisticModel& model, const std::vector<double>& normalizedValues) const;

    EngineConfig baseEngine_;
    EngineConfig baselineEngine_;
    CLOPConfig config_;
    std::shared_ptr<StartPositions> startPositions_;
    GameManagerPool* pool_ = nullptr;

    std::vector<CLOPSample> samples_;
    std::vector<double> estimatedParameters_;

    std::vector<CLOPSample> activeSamples_;
    uint32_t nextRound_ = 0;

    mutable std::mutex stateMutex_;
    std::condition_variable stateCondition_;

    std::thread workerThread_;
    std::atomic<bool> stopWorker_ = false;
    bool initialized_ = false;
    bool scheduled_ = false;
    bool finished_ = false;

    std::mt19937 rng_;
};

} // namespace QaplaTester
